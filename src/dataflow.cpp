/* Copyright (c) 2013, Avian Contributors

   Permission to use, copy, modify, and/or distribute this software
   for any purpose with or without fee is hereby granted, provided
   that the above copyright notice and this permission notice appear
   in all copies.

   There is NO WARRANTY for this software.  See license.txt for
   details. */

#include "avian/dataflow.h"
#include "avian/process.h"
#include "avian/util/runtime-array.h"

using namespace vm;
using namespace vm::dataflow;

namespace {

namespace local {

const unsigned InvalidIp = 0xFFFFFFFF;

const unsigned DirtyLocals = 1 << 0;
const unsigned DirtyStack = 1 << 1;
const unsigned DirtyStackPointer = 1 << 2;
const unsigned DirtyReads = 1 << 3;

typedef Operand* Integer;
typedef Operand* Reference;
typedef Operand* Long;
typedef Operand* Double;
typedef Operand* Float;

class State {
 public:
  State(object method, Instruction* instruction, unsigned ip):
    method(method),
    instruction(instruction),
    ip(ip)
  { }

  object method;
  Instruction* instruction;
  unsigned ip;
};

class MyReference {
 public:
  MyReference(object* pointer, MyReference* next):
    pointer(pointer),
    next(next)
  { }

  object* pointer;
  MyReference* next;
};

class Context;

Operand*
operand(Context* c, object type, bool exactType = false,
        Value::V value = Value::V(static_cast<int64_t>(0)));

class Context {
 public:
  class MyProtector: public Thread::Protector {
   public:
    MyProtector(Context* context):
      Protector(context->t), context(context)
    { }

    virtual void visit(Heap::Visitor* v) {
      v->visit(&(context->trace));
      v->visit(&(context->state.method));
      
      for (MyReference* r = context->references; r; r = r->next) {
        v->visit(r->pointer);
      }

      for (Zone::Segment* segment = context->states.segment; segment;
           segment = segment->next)
      {
        for (unsigned i = 0; i < segment->position / sizeof(State); ++i) {
          v->visit(&((reinterpret_cast<State*>(segment->data) + i)->method));
        }
      }
    }

    Context* context;
  };

  Context(Thread* t, Allocator* allocator, object method, object trace):
    t(t),
    allocator(allocator),
    trace(trace),
    graph
    (static_cast<Graph*>
     (allocator->allocate
      (sizeof(Graph)
       + (((trace ? traceLength(t, trace)
            : codeLength(t, methodCode(t, method))) + 1) * BytesPerWord)))),
    states(t->m->system, allocator, 0),
    frame(new (allocator) Frame
          (static_cast<Operand**>
           (allocator->allocate(codeMaxLocals(t, methodCode(t, method)))),
           static_cast<Operand**>
           (allocator->allocate(codeMaxStack(t, methodCode(t, method)))), 0)),
    references(0),
    state(method, 0, InvalidIp),
    frameMask(0),
    protector(this)
  {
    if (trace) {
      for (unsigned i = 0; i < arrayLength(t, traceLocals(t, trace)); ++i) {
        object type = arrayBody(t, traceLocals(t, trace), i);

        frame->locals[i] = type ? operand(this, type) : 0;
      }

      for (unsigned i = 0; i < arrayLength(t, traceStack(t, trace)); ++i) {
        object type = arrayBody(t, traceStack(t, trace), i);

        frame->stack[i] = type ? operand(this, type) : 0;
      }

      frame->sp = arrayLength(t, traceStack(t, trace));
    } else {
      unsigned i = 0;
      if ((methodFlags(t, method) & ACC_STATIC) == 0) {
        frame->locals[i++] = operand(this, methodClass(t, method));
      }

      for (MethodSpecIterator it
             (t, reinterpret_cast<const char*>
              (&byteArrayBody(t, methodSpec(t, method), 0)));
           it.hasNext();)
      {
        switch (*it.next()) {
        case 'L':
        case '[':
          frame->locals[i++] = operand(this, type(t, Machine::JobjectType));
          break;
      
        case 'J':
          frame->locals[i++] = operand(this, type(t, Machine::JlongType));
          frame->locals[i++] = 0;
          break;

        case 'D':
          frame->locals[i++] = operand(this, type(t, Machine::JdoubleType));
          frame->locals[i++] = 0;
          break;
      
        case 'F':
          frame->locals[i++] = operand(this, type(t, Machine::JfloatType));
          break;
      
        default:
          frame->locals[i++] = operand(this, type(t, Machine::JintType));
          break;
        }
      }
    }
  }

  Thread* t;
  Allocator* allocator;
  object trace;
  Instruction* instruction;
  Graph* graph;
  Zone states;
  Frame* frame;
  MyReference* references;
  State state;
  unsigned frameMask;
  MyProtector protector;
};

Thread*
contextThread(Context* c)
{
  return c->t;
}

unsigned&
contextIp(Context* c)
{
  return c->state.ip;
}

object
contextMethod(Context* c)
{
  return c->state.method;
}

ResolveStrategy
contextResolveStrategy(Context*)
{
  return NoResolve;
}

void
followReads(Operand** p)
{
  Operand* o = *p;
  if (o) {
    Read* r = o->read;
    while (r) {
      o = r->next;
      r = o->read;
    }
    *p = o;
  }
}

unsigned
maxLocals(Context* c)
{
  return codeMaxLocals(c->t, methodCode(c->t, contextMethod(c)));
}

unsigned
maxStack(Context* c)
{
  return codeMaxStack(c->t, methodCode(c->t, contextMethod(c)));
}

Operand**
copy(Context* c, Operand** array, unsigned copyCount, unsigned size)
{
  Operand** newArray = static_cast<Operand**>
    (c->allocator->allocate(size * BytesPerWord));

  memcpy(newArray, array, copyCount * BytesPerWord);

  return newArray;
}

Operand**
copy(Context* c, Operand** array, unsigned size)
{
  return copy(c, array, size, size);
}

Instruction*
exitInstruction(Context* c)
{
  Instruction* instruction = c->instruction;

  if (instruction) {
    if (c->frameMask & DirtyReads) {
      if ((c->frameMask & (DirtyStack | DirtyLocals))
          != (DirtyStack | DirtyLocals))
      {
        c->frame = new (c->allocator) Frame
          ((c->frameMask & DirtyLocals) ? c->frame->locals : copy
           (c, c->frame->locals, maxLocals(c)),
           (c->frameMask & DirtyStack) ? c->frame->stack : copy
           (c, c->frame->stack, c->frame->sp, maxStack(c)), c->frame->sp);
      }

      for (unsigned i = 0; i < maxLocals(c); ++i) {
        followReads(c->frame->locals + i);
      }

      for (unsigned i = 0; i < c->frame->sp; ++i) {
        followReads(c->frame->stack + i);
      }
    }

    instruction->exit = c->frame;
    c->instruction = 0;
    c->frameMask = 0;
  }

  return instruction;
}

void
next(Context* c)
{
  // todo: method returns (in trace case) require some stack shuffling
  if (c->states.isEmpty()) {
    c->state = State(0, 0, InvalidIp);
  } else {
    c->state = *static_cast<State*>(c->states.peek(sizeof(State)));
    c->states.pop(sizeof(State));
  }
}

bool
visitInstruction(Context* c)
{
  Instruction* predecessor = exitInstruction(c);

  assert(c->t, c->frameMask == 0);

  Instruction* i = c->graph->instructions[c->state.ip];
  if (i) {
    if (i->exit) {
      for (unsigned j = 0; j < maxLocals(c); ++j) {
        Operand* o = predecessor->exit->locals[j];
        if (o) {
          o->read = i->entry->locals[j]->read;
        }
      }

      for (unsigned j = 0; j < c->frame->sp; ++j) {
        Operand* o = predecessor->exit->stack[j];
        if (o) {
          o->read = i->entry->stack[j]->read;
        }
      }

      next(c);

      return false;
    }
  } else {
    Instruction* i = new (c->allocator) Instruction(c->frame);
    c->instruction = i;
    c->graph->instructions[c->state.ip] = i;
  }

  return true;
}

void
pushInt(Context* c, Integer v)
{
  if ((c->frameMask & DirtyStack) == 0) {
    c->frame = new (c->allocator) Frame
      (c->frame->locals, copy(c, c->frame->stack, c->frame->sp, maxStack(c)),
       c->frame->sp);

    c->frameMask |= DirtyStack;
  }

  c->frame->stack[c->frame->sp ++] = v;
}

void
pushReference(Context* c, Reference v)
{
  pushInt(c, v);
}

void
pushFloat(Context* c, Float v)
{
  pushInt(c, v);
}

void
pushLong(Context* c, Long v)
{
  pushInt(c, v);
  pushInt(c, 0);
}

void
pushDouble(Context* c, Double v)
{
  pushLong(c, v);
}

void
popFrame(Context* c)
{
  next(c);
}

void
dirtyStackPointer(Context* c)
{
  if (c->frameMask == 0) {
    c->frame = new (c->allocator) Frame(*(c->frame));

    c->frameMask |= DirtyStackPointer;
  }
}

Integer
popInt(Context* c)
{
  dirtyStackPointer(c);

  return c->frame->stack[-- c->frame->sp];
}

void
pop(Context* c)
{
  popInt(c);
}

void
contextPop2(Context* c)
{
  pop(c);
  pop(c);
}

void
contextSwap(Context* c)
{
  Operand* a = popInt(c);
  Operand* b = popInt(c);

  pushInt(c, a);
  pushInt(c, b);
}

Reference
popReference(Context* c)
{
  return popInt(c);
}

Float
popFloat(Context* c)
{
  return popInt(c);
}

Long
popLong(Context* c)
{
  expect(c->t, popInt(c) == 0);
  return popInt(c);
}

Double
popDouble(Context* c)
{
  return popLong(c);
}

Reference
peekReference(Context* c)
{
  return c->frame->stack[c->frame->sp - 1];
}

Integer
localInt(Context* c, unsigned index)
{
  return c->frame->locals[index];
}

Reference
localReference(Context* c, unsigned index)
{
  return localInt(c, index);
}

Float
localFloat(Context* c, unsigned index)
{
  return localInt(c, index);
}

Long
localLong(Context* c, unsigned index)
{
  return localInt(c, index);
}

Double
localDouble(Context* c, unsigned index)
{
  return localInt(c, index);
}

void
setLocalInt(Context* c, unsigned index, Integer value)
{
  if ((c->frameMask & DirtyLocals) == 0) {
    c->frame = new (c->allocator) Frame
      (copy(c, c->frame->locals, maxLocals(c)), c->frame->stack, c->frame->sp);

    c->frameMask |= DirtyLocals;
  }

  c->frame->locals[index] = value;
}

void
setLocalReference(Context* c, unsigned index, Reference value)
{
  setLocalInt(c, index, value);
}

void
setLocalFloat(Context* c, unsigned index, Float value)
{
  setLocalInt(c, index, value);
}

void
setLocalLong(Context* c, unsigned index, Long value)
{
  setLocalInt(c, index, value);
  setLocalInt(c, index + 1, 0);
}

void
setLocalDouble(Context* c, unsigned index, Double value)
{
  setLocalInt(c, index, value);
  setLocalInt(c, index + 1, 0);
}

void
reference(Context* c, object* pointer)
{
  c->references = new (c->allocator) MyReference(pointer, c->references);
}

Operand*
operand(Context* c, object type, bool exactType, Value::V value)
{
  Value* v = new (c->allocator) Value(type, exactType, value);

  reference(c, &(v->type));

  if (value.reference and (classVmFlags(c->t, type) & PrimitiveFlag) == 0) {
    reference(c, &(v->value.reference));
  }

  return new (c->allocator) Operand(v, 0);
}

Operand*
appendEventArray(Context* c, Event::Type eventType, void* context,
                 object resultType, unsigned readCount, Operand** reads)
{
  Operand* result;
  Value* resultValue;
  if (resultType) {
    result = operand(c, resultType);
    resultValue = result->value;
  } else {
    result = 0;
    resultValue = 0;
  }

  Event* event = new
    (c->allocator->allocate(sizeof(Event) + (readCount * BytesPerWord)))
    Event(eventType, context, resultValue, readCount);

  for (unsigned i = 0; i < readCount; ++i) {
    Operand* current = reads[i];
    followReads(&current);

    Operand* next = new (c->allocator) Operand(current->value, 0);
    Read* read = new (c->allocator) Read(event, i, next);
    current->read = read;
    event->reads[i] = read;
    
    c->frameMask |= DirtyReads;
  }

  return result;
}

Operand*
appendEvent(Context* c, Event::Type eventType, void* context,
            object resultType, unsigned readCount, ...)
{
  THREAD_RUNTIME_ARRAY(c->t, Operand*, reads, readCount);

  va_list a; va_start(a, readCount);
  for (unsigned i = 0; i < readCount; ++i) {
    RUNTIME_ARRAY_BODY(reads)[i] = va_arg(a, Operand*);
  }
  va_end(a);

  return appendEventArray
    (c, eventType, context, resultType, readCount, RUNTIME_ARRAY_BODY(reads));
}

Operand*
arrayLoad(Context* c, Operand* array, Operand* index, object resultType,
          unsigned elementSize, bool signExtend = true)
{
  return appendEvent
    (c, Event::ArrayLoad, new (c->allocator) Event::ArrayLoadContext
     (elementSize, signExtend), resultType, 2, array, index);
}

Reference
referenceArrayLoad(Context* c, Reference array, Integer index)
{
  object type = array->value->type;
  Thread* t = c->t;

  if (type != vm::type(t, Machine::JobjectType)) {
    assert(t, classArrayElementSize(t, type) == BytesPerWord);

    type = classStaticTable(t, type);
  }

  return arrayLoad(c, array, index, type, BytesPerWord);
}

Integer
byteArrayLoad(Context* c, Reference array, Integer index)
{
  return arrayLoad(c, array, index, type(c->t, Machine::JintType), 1);
}

Integer
charArrayLoad(Context* c, Reference array, Integer index)
{
  return arrayLoad(c, array, index, type(c->t, Machine::JintType), 2, false);
}

Integer
shortArrayLoad(Context* c, Reference array, Integer index)
{
  return arrayLoad(c, array, index, type(c->t, Machine::JintType), 2);
}

Double
doubleArrayLoad(Context* c, Reference array, Integer index)
{
  return arrayLoad(c, array, index, type(c->t, Machine::JdoubleType), 8);
}

Long
longArrayLoad(Context* c, Reference array, Integer index)
{
  return arrayLoad(c, array, index, type(c->t, Machine::JlongType), 8);
}

Float
floatArrayLoad(Context* c, Reference array, Integer index)
{
  return arrayLoad(c, array, index, type(c->t, Machine::JfloatType), 4);
}

Integer
intArrayLoad(Context* c, Reference array, Integer index)
{
  return arrayLoad(c, array, index, type(c->t, Machine::JintType), 4);
}

void
arrayStore(Context* c, Operand* array, Operand* index, Operand* value,
           unsigned size)
{
  appendEvent
    (c, Event::ArrayStore, new (c->allocator) Event::ArrayStoreContext(size),
     0, 3, array, index, value);
}

void
referenceArrayStore(Context* c, Reference array, Integer index,
                    Reference value)
{
  arrayStore(c, array, index, value, BytesPerWord);
}

void
byteArrayStore(Context* c, Reference array, Integer index, Integer value)
{
  arrayStore(c, array, index, value, 1);
}

void
charArrayStore(Context* c, Reference array, Integer index, Integer value)
{
  arrayStore(c, array, index, value, 2);
}

void
shortArrayStore(Context* c, Reference array, Integer index, Integer value)
{
  arrayStore(c, array, index, value, 2);
}

void
doubleArrayStore(Context* c, Reference array, Integer index, Double value)
{
  arrayStore(c, array, index, value, 8);
}

void
longArrayStore(Context* c, Reference array, Integer index, Long value)
{
  arrayStore(c, array, index, value, 8);
}

void
floatArrayStore(Context* c, Reference array, Integer index, Float value)
{
  arrayStore(c, array, index, value, 4);
}

void
intArrayStore(Context* c, Reference array, Integer index, Integer value)
{
  arrayStore(c, array, index, value, 4);
}

Integer
intConstant(Context* c, int32_t value)
{
  return operand(c, type(c->t, Machine::JintType), true, value);
}

Long
longConstant(Context* c, int64_t value)
{
  return operand(c, type(c->t, Machine::JlongType), true, value);
}

Reference
referenceConstant(Context* c, object value)
{
  return operand
    (c, value ? objectClass(c->t, value) : type(c->t, Machine::JobjectType),
     value != 0, value);
}

Double
doubleConstant(Context* c, double value)
{
  return operand
    (c, type(c->t, Machine::JdoubleType), true, doubleToBits(value));
}

Float
floatConstant(Context* c, float value)
{
  return operand
    (c, type(c->t, Machine::JfloatType), true, floatToBits(value));
}

Reference
makeObjectArray(Context* c, object elementType, Integer size)
{
  return appendEvent
    (c, Event::MakeObjectArray, 0, resolveObjectArrayClass
     (c->t, classLoader(c->t, elementType), elementType), 1, size);
}

Integer
loadInt(Context* c, Reference target, unsigned offset, unsigned size = 4,
        bool signExtend = true)
{
  return appendEvent
    (c, Event::Load, new (c->allocator) Event::LoadContext
     (offset, size, signExtend), type(c->t, Machine::JintType), 1, target);
}

Integer
loadWord(Context* c, Reference target, unsigned offset)
{
  return loadInt(c, target, offset, BytesPerWord);
}

Integer
loadByte(Context* c, Reference target, unsigned offset)
{
  return loadInt(c, target, offset, 1);
}

Integer
loadChar(Context* c, Reference target, unsigned offset)
{
  return loadInt(c, target, offset, 2, false);
}

Integer
loadShort(Context* c, Reference target, unsigned offset)
{
  return loadInt(c, target, offset, 2);
}

Float
loadFloat(Context* c, Reference target, unsigned offset)
{
  return appendEvent
    (c, Event::Load, new (c->allocator) Event::LoadContext(offset, 4),
     type(c->t, Machine::JfloatType), 1, target);
}

Double
loadDouble(Context* c, Reference target, unsigned offset)
{
  return appendEvent
    (c, Event::Load, new (c->allocator) Event::LoadContext(offset, 8),
     type(c->t, Machine::JdoubleType), 1, target);
}

Long
loadLong(Context* c, Reference target, unsigned offset)
{
  return appendEvent
    (c, Event::Load, new (c->allocator) Event::LoadContext(offset, 8),
     type(c->t, Machine::JlongType), 1, target);
}

Reference
loadReference(Context* c, Reference target, unsigned offset)
{
  return appendEvent
    (c, Event::Load, new (c->allocator) Event::LoadContext
     (offset, BytesPerWord), type(c->t, Machine::JobjectType), 1, target);
}

void
storeInt(Context* c, Reference target, unsigned offset, Integer value,
         unsigned size = 4)
{
  appendEvent
    (c, Event::Store, new (c->allocator) Event::StoreContext
     (offset, size), 0, 2, target, value);
}

void
storeByte(Context* c, Reference target, unsigned offset, Integer value)
{
  storeInt(c, target, offset, value, 1);
}

void
storeShort(Context* c, Reference target, unsigned offset, Integer value)
{
  storeInt(c, target, offset, value, 2);
}

void
storeFloat(Context* c, Reference target, unsigned offset, Float value)
{
  storeInt(c, target, offset, value);
}

void
storeDouble(Context* c, Reference target, unsigned offset, Double value)
{
  storeInt(c, target, offset, value, 8);
}

void
storeLong(Context* c, Reference target, unsigned offset, Long value)
{
  storeInt(c, target, offset, value, 8);
}

void
storeReference(Context* c, Reference target, unsigned offset, Reference value)
{
  storeInt(c, target, offset, value, BytesPerWord);
}

void
throwException(Context* c, Reference exception)
{
  appendEvent(c, Event::Throw, 0, 0, 1, exception);
  next(c);
}

Event::ObjectContext*
objectContext(Context* c, object value)
{
  Event::ObjectContext* oc = new (c->allocator) Event::ObjectContext(value);

  reference(c, &(oc->value));

  return oc;
}

void
checkCast(Context* c, object type, Reference instance)
{
  appendEvent(c, Event::CheckCast, objectContext(c, type), 0, 1, instance);
}

Integer
instanceOf(Context* c, object type, Reference instance)
{
  return appendEvent
    (c, Event::InstanceOf, objectContext(c, type),
     vm::type(c->t, Machine::JintType), 1, instance);
}

Integer
intToByte(Context* c, Integer v)
{
  return appendEvent
    (c, Event::IntToByte, 0, type(c->t, Machine::JintType), 1, v);
}

Integer
intToChar(Context* c, Integer v)
{
  return appendEvent
    (c, Event::IntToChar, 0, type(c->t, Machine::JintType), 1, v);
}

Integer
intToShort(Context* c, Integer v)
{
  return appendEvent
    (c, Event::IntToShort, 0, type(c->t, Machine::JintType), 1, v);
}

Double
intToDouble(Context* c, Integer v)
{
  return appendEvent
    (c, Event::IntToDouble, 0, type(c->t, Machine::JdoubleType), 1, v);
}

Float
intToFloat(Context* c, Integer v)
{
  return appendEvent
    (c, Event::IntToFloat, 0, type(c->t, Machine::JfloatType), 1, v);
}

Long
intToLong(Context* c, Integer v)
{
  return appendEvent
    (c, Event::IntToLong, 0, type(c->t, Machine::JlongType), 1, v);
}

Double
longToDouble(Context* c, Long v)
{
  return appendEvent
    (c, Event::LongToDouble, 0, type(c->t, Machine::JdoubleType), 1, v);
}

Float
longToFloat(Context* c, Long v)
{
  return appendEvent
    (c, Event::LongToFloat, 0, type(c->t, Machine::JfloatType), 1, v);
}

Integer
longToInt(Context* c, Long v)
{
  return appendEvent
    (c, Event::LongToInt, 0, type(c->t, Machine::JintType), 1, v);
}

Integer
intAdd(Context* c, Integer a, Integer b)
{
  return appendEvent
    (c, Event::Add, 0, type(c->t, Machine::JintType), 2, a, b);
}

Integer
intSubtract(Context* c, Integer a, Integer b)
{
  return appendEvent
    (c, Event::Subtract, 0, type(c->t, Machine::JintType), 2, a, b);
}

Integer
intAnd(Context* c, Integer a, Integer b)
{
  return appendEvent
    (c, Event::And, 0, type(c->t, Machine::JintType), 2, a, b);
}

Integer
intOr(Context* c, Integer a, Integer b)
{
  return appendEvent
    (c, Event::Or, 0, type(c->t, Machine::JintType), 2, a, b);
}

Integer
intXor(Context* c, Integer a, Integer b)
{
  return appendEvent
    (c, Event::Xor, 0, type(c->t, Machine::JintType), 2, a, b);
}

Integer
intMultiply(Context* c, Integer a, Integer b)
{
  return appendEvent
    (c, Event::Multiply, 0, type(c->t, Machine::JintType), 2, a, b);
}

Integer
intDivide(Context* c, Integer a, Integer b)
{
  return appendEvent
    (c, Event::Divide, 0, type(c->t, Machine::JintType), 2, a, b);
}

Integer
intRemainder(Context* c, Integer a, Integer b)
{
  return appendEvent
    (c, Event::Remainder, 0, type(c->t, Machine::JintType), 2, a, b);
}

Integer
intShiftLeft(Context* c, Integer a, Integer b)
{
  return appendEvent
    (c, Event::ShiftLeft, 0, type(c->t, Machine::JintType), 2, a, b);
}

Integer
intShiftRight(Context* c, Integer a, Integer b)
{
  return appendEvent
    (c, Event::ShiftRight, 0, type(c->t, Machine::JintType), 2, a, b);
}

Integer
intUnsignedShiftRight(Context* c, Integer a, Integer b)
{
  return appendEvent
    (c, Event::UnsignedShiftRight, 0, type(c->t, Machine::JintType), 2, a, b);
}

Integer
intNegate(Context* c, Integer a)
{
  return appendEvent
    (c, Event::Negate, 0, type(c->t, Machine::JintType), 1, a);
}

Integer
intEqual(Context* c, Integer a, Integer b)
{
  return appendEvent
    (c, Event::Equal, 0, type(c->t, Machine::JintType), 2, a, b);
}

Integer
intGreater(Context* c, Integer a, Integer b)
{
  return appendEvent
    (c, Event::Greater, 0, type(c->t, Machine::JintType), 2, a, b);
}

Integer
intLess(Context* c, Integer a, Integer b)
{
  return appendEvent
    (c, Event::Less, 0, type(c->t, Machine::JintType), 2, a, b);
}

Long
longAdd(Context* c, Long a, Long b)
{
  return appendEvent
    (c, Event::Add, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Long
longSubtract(Context* c, Long a, Long b)
{
  return appendEvent
    (c, Event::Subtract, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Long
longAnd(Context* c, Long a, Long b)
{
  return appendEvent
    (c, Event::And, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Long
longOr(Context* c, Long a, Long b)
{
  return appendEvent
    (c, Event::Or, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Long
longXor(Context* c, Long a, Long b)
{
  return appendEvent
    (c, Event::Xor, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Long
longMultiply(Context* c, Long a, Long b)
{
  return appendEvent
    (c, Event::Multiply, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Long
longDivide(Context* c, Long a, Long b)
{
  return appendEvent
    (c, Event::Divide, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Long
longRemainder(Context* c, Long a, Long b)
{
  return appendEvent
    (c, Event::Remainder, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Long
longShiftLeft(Context* c, Long a, Integer b)
{
  return appendEvent
    (c, Event::ShiftLeft, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Long
longShiftRight(Context* c, Long a, Integer b)
{
  return appendEvent
    (c, Event::ShiftRight, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Long
longUnsignedShiftRight(Context* c, Long a, Integer b)
{
  return appendEvent
    (c, Event::UnsignedShiftRight, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Integer
longCompare(Context* c, Long a, Long b)
{
  return appendEvent
    (c, Event::Compare, 0, type(c->t, Machine::JintType), 2, a, b);
}

Long
longNegate(Context* c, Long a)
{
  return appendEvent
    (c, Event::Negate, 0, type(c->t, Machine::JlongType), 1, a);
}

Integer
notNull(Context* c, Reference a)
{
  return appendEvent
    (c, Event::NotNull, 0, type(c->t, Machine::JintType), 1, a);
}

Integer
referenceEqual(Context* c, Reference a, Reference b)
{
  return intEqual(c, a, b);
}

void
branch(Context* c, Integer condition, unsigned ifTrue, unsigned ifFalse)
{
  // todo: trace case

  appendEvent
    (c, Event::Branch, new (c->allocator) Event::BranchContext
     (ifTrue, ifFalse), 0, 1, condition);

  new (&(c->states)) State(c->state.method, c->state.instruction, ifFalse);
  
  c->state.ip = ifTrue;
}

Float
doubleToFloat(Context* c, Double v)
{
  return appendEvent
    (c, Event::DoubleToFloat, 0, type(c->t, Machine::JfloatType), 1, v);
}

Integer
doubleToInt(Context* c, Double v)
{
  return appendEvent
    (c, Event::DoubleToInt, 0, type(c->t, Machine::JintType), 1, v);
}

Long
doubleToLong(Context* c, Double v)
{
  return appendEvent
    (c, Event::DoubleToLong, 0, type(c->t, Machine::JlongType), 1, v);
}

Double
doubleAdd(Context* c, Double a, Double b)
{
  return appendEvent
    (c, Event::Add, 0, type(c->t, Machine::JdoubleType), 2, a, b);
}

Double
doubleSubtract(Context* c, Double a, Double b)
{
  return appendEvent
    (c, Event::Subtract, 0, type(c->t, Machine::JdoubleType), 2, a, b);
}

Double
doubleDivide(Context* c, Double a, Double b)
{
  return appendEvent
    (c, Event::Divide, 0, type(c->t, Machine::JdoubleType), 2, a, b);
}

Double
doubleMultiply(Context* c, Double a, Double b)
{
  return appendEvent
    (c, Event::Multiply, 0, type(c->t, Machine::JdoubleType), 2, a, b);
}

Double
doubleRemainder(Context* c, Double a, Double b)
{
  return appendEvent
    (c, Event::Remainder, 0, type(c->t, Machine::JdoubleType), 2, a, b);
}

Double
doubleNegate(Context* c, Double a)
{
  return appendEvent
    (c, Event::Negate, 0, type(c->t, Machine::JdoubleType), 1, a);
}

Integer
doubleCompareGreater(Context* c, Double a, Double b)
{
  return appendEvent
    (c, Event::DoubleCompareGreater, 0, type(c->t, Machine::JintType), 2, a,
     b);
}

Integer
doubleCompareLess(Context* c, Double a, Double b)
{
  return appendEvent
    (c, Event::DoubleCompareLess, 0, type(c->t, Machine::JintType), 2, a, b);
}

Double
floatToDouble(Context* c, Float v)
{
  return appendEvent
    (c, Event::FloatToDouble, 0, type(c->t, Machine::JdoubleType), 1, v);
}

Integer
floatToInt(Context* c, Float v)
{
  return appendEvent
    (c, Event::FloatToInt, 0, type(c->t, Machine::JintType), 1, v);
}

Long
floatToLong(Context* c, Float v)
{
  return appendEvent
    (c, Event::FloatToLong, 0, type(c->t, Machine::JlongType), 1, v);
}

Float
floatAdd(Context* c, Float a, Float b)
{
  return appendEvent
    (c, Event::Add, 0, type(c->t, Machine::JfloatType), 2, a, b);
}

Float
floatSubtract(Context* c, Float a, Float b)
{
  return appendEvent
    (c, Event::Subtract, 0, type(c->t, Machine::JfloatType), 2, a, b);
}

Float
floatDivide(Context* c, Float a, Float b)
{
  return appendEvent
    (c, Event::Divide, 0, type(c->t, Machine::JfloatType), 2, a, b);
}

Float
floatMultiply(Context* c, Float a, Float b)
{
  return appendEvent
    (c, Event::Multiply, 0, type(c->t, Machine::JfloatType), 2, a, b);
}

Float
floatRemainder(Context* c, Float a, Float b)
{
  return appendEvent
    (c, Event::Remainder, 0, type(c->t, Machine::JfloatType), 2, a, b);
}

Float
floatNegate(Context* c, Float a)
{
  return appendEvent
    (c, Event::Negate, 0, type(c->t, Machine::JfloatType), 1, a);
}

Integer
floatCompareGreater(Context* c, Float a, Float b)
{
  return appendEvent
    (c, Event::FloatCompareGreater, 0, type(c->t, Machine::JintType), 2, a,
     b);
}

Integer
floatCompareLess(Context* c, Float a, Float b)
{
  return appendEvent
    (c, Event::FloatCompareLess, 0, type(c->t, Machine::JintType), 2, a, b);
}

void
duplicate(Context* c, unsigned instruction)
{
  switch (instruction) {
  case dup:
    pushInt(c, c->frame->stack[c->frame->sp - 1]);
    break;

  case dup_x1: {
    Operand* a = popInt(c);
    Operand* b = popInt(c);

    pushInt(c, a);
    pushInt(c, b);
    pushInt(c, a);
  } break;

  case dup_x2: {
    Operand* a = popInt(c);
    Operand* b = popInt(c);
    Operand* d = popInt(c);

    pushInt(c, a);
    pushInt(c, d);
    pushInt(c, b);
    pushInt(c, a);
  } break;

  case dup2: {
    Operand* a = popInt(c);
    Operand* b = popInt(c);

    pushInt(c, b);
    pushInt(c, a);
    pushInt(c, b);
    pushInt(c, a);
  } break;

  case dup2_x1: {
    Operand* a = popInt(c);
    Operand* b = popInt(c);
    Operand* d = popInt(c);

    pushInt(c, b);
    pushInt(c, a);
    pushInt(c, d);
    pushInt(c, b);
    pushInt(c, a);
  } break;

  case dup2_x2: {
    Operand* a = popInt(c);
    Operand* b = popInt(c);
    Operand* d = popInt(c);
    Operand* e = popInt(c);

    pushInt(c, b);
    pushInt(c, a);
    pushInt(c, e);
    pushInt(c, d);
    pushInt(c, b);
    pushInt(c, a);
  } break;

  default: abort(c->t);
  }
}

void
acquireForRead(Context* c, object field)
{
  appendEvent(c, Event::AcquireForRead, objectContext(c, field), 0, 0);
}

void
releaseForRead(Context* c, object field)
{
  appendEvent(c, Event::ReleaseForRead, objectContext(c, field), 0, 0);
}

class ReadResource {
 public:
  ReadResource(Context* c, object o): c(c), o(o), protector(c->t, &(this->o)) {
    acquireForRead(c, o);
  }

  ~ReadResource() {
    releaseForRead(c, o);
  }

 private:
  Context* c;
  object o;
  Thread::SingleProtector protector;
};

#define CONTEXT_ACQUIRE_FIELD_FOR_READ(c, field) \
  ReadResource MAKE_NAME(fieldResource_) (c, field)

void
acquireForWrite(Context* c, object field)
{
  appendEvent(c, Event::AcquireForWrite, objectContext(c, field), 0, 0);
}

void
releaseForWrite(Context* c, object field)
{
  appendEvent(c, Event::ReleaseForWrite, objectContext(c, field), 0, 0);
}

class WriteResource {
 public:
  WriteResource(Context* c, object o):
    c(c), o(o), protector(c->t, &(this->o))
  {
    acquireForWrite(c, o);
  }

  ~WriteResource() {
    releaseForWrite(c, o);
  }

 private:
  Context* c;
  object o;
  Thread::SingleProtector protector;
};

#define CONTEXT_ACQUIRE_FIELD_FOR_WRITE(c, field) \
  WriteResource MAKE_NAME(fieldResource_) (c, field)

void
jump(Context* c, unsigned ip)
{
  appendEvent(c, Event::Jump, new (c->allocator) Event::JumpContext(ip), 0, 0);

  c->state.ip = ip;
}

void
jumpToSubroutine(Context* c, unsigned ip)
{
  appendEvent
    (c, Event::JumpToSubroutine, new (c->allocator) Event::JumpContext(ip), 0,
     0);

  new (&(c->states)) State(c->state);

  c->state.ip = ip;
}

void
returnFromSubroutine(Context* c, Integer ip)
{
  appendEvent(c, Event::ReturnFromSubroutine, 0, 0, 1, ip);
}

void
invoke(Context* c, object method, Event::Type eventType, bool isStatic)
{
  Thread* t = c->t;

  object spec = (objectClass(t, method) == type(t, Machine::MethodType))
    ? methodSpec(t, method) : referenceSpec(t, method);

  unsigned footprint;
  unsigned count;
  if (isStatic) {
    footprint = 0;
    count = 0;
  } else {
    footprint = 1;
    count = 1;
  }

  MethodSpecIterator it
    (t, reinterpret_cast<const char*>(&byteArrayBody(t, spec, 0)));

  while (it.hasNext()) {
    switch (*it.next()) {
    case 'J':
    case 'D':
      footprint += 2;
      break;
      
    default:
      ++ footprint;
      break;
    }

    ++ count; 
  }

  THREAD_RUNTIME_ARRAY(t, Operand*, arguments, count);

  count = 0;

  unsigned si = c->frame->sp - footprint;
  while (si != c->frame->sp) {
    Operand* o = c->frame->stack[si++];
    if (o) {
      RUNTIME_ARRAY_BODY(arguments)[count++] = o;
    }
  }

  appendEventArray
    (c, eventType, new (c->allocator) Event::ObjectContext(method),
     0, count, RUNTIME_ARRAY_BODY(arguments));

  dirtyStackPointer(c);

  c->frame->sp -= footprint;

  switch (it.s[1]) {
  case 'J':
    pushLong(c, operand(c, type(t, Machine::JlongType)));
    break;

  case 'D':
    pushDouble(c, operand(c, type(t, Machine::JdoubleType)));
    break;

  case 'F':
    pushFloat(c, operand(c, type(t, Machine::JfloatType)));
    break;

  case 'I':
  case 'Z':
  case 'B':
  case 'S':
  case 'C':
    pushInt(c, operand(c, type(t, Machine::JintType)));
    break;

  case 'V':
    break;

  case '[':
  case 'L': {
    object type;
    if (c->trace) {
      const char* start = it.s + 1;
      if (*start == 'L') ++ start;
      const char* end = start;
      while (*end != ';') ++ end;
      unsigned nameLength = end - start;
      THREAD_RUNTIME_ARRAY(t, char, name, nameLength + 1);
      memcpy(RUNTIME_ARRAY_BODY(name), start, nameLength);
      RUNTIME_ARRAY_BODY(name)[nameLength] = 0;

      type = resolveClass
        (t, classLoader(t, methodClass(t, contextMethod(c))),
         RUNTIME_ARRAY_BODY(name));
    } else {
      type = vm::type(t, Machine::JobjectType);
    }

    pushReference(c, operand(c, type));
  } break;
  }
}

void
invokeVirtual(Context* c, object method)
{
  invoke(c, method, Event::InvokeVirtual, false);
}

void
invokeInterface(Context* c, object method)
{
  invokeVirtual(c, method);
}

void
invokeDirect(Context* c, object method, bool isStatic)
{
  invoke(c, method, Event::InvokeDirect, isStatic);
}

void
acquire(Context* c, Reference instance)
{
  appendEvent(c, Event::Acquire, 0, 0, 1, instance);
}

void
release(Context* c, Reference instance)
{
  appendEvent(c, Event::Release, 0, 0, 1, instance);
}

void
lookupSwitch(Context* c, Integer value, object code, int32_t base,
             int32_t default_, int32_t pairCount)
{
  Event::LookupSwitchContext* lsc = new (c->allocator)
    Event::LookupSwitchContext(code, base, default_, pairCount);

  reference(c, &(lsc->code));

  appendEvent(c, Event::LookupSwitch, lsc, 0, 1, value);

  for (int32_t i = 0; i < pairCount; ++i) {
    unsigned index = c->state.ip + (i * 8) + 4;
    new (&(c->states)) State
      (c->state.method, c->state.instruction, base + codeReadInt32
       (c->t, code, index));
  }
  
  c->state.ip = base + default_;
}

void
tableSwitch(Context* c, Integer value, object code, int32_t base,
            int32_t default_, int32_t bottom, int32_t top)
{
  Event::TableSwitchContext* tsc = new (c->allocator)
    Event::TableSwitchContext(code, base, default_, bottom, top);

  reference(c, &(tsc->code));

  appendEvent(c, Event::TableSwitch, tsc, 0, 1, value);

  for (int32_t i = 0; i < top - bottom + 1; ++i) {
    unsigned index = c->state.ip + (i * 4);
    new (&(c->states)) State
      (c->state.method, c->state.instruction, base + codeReadInt32
       (c->t, code, index));
  }

  c->state.ip = base + default_;
}

Reference
makeMultiArray(Context* c, Integer* counts, unsigned dimensions, object type)
{
  return appendEventArray
    (c, Event::MakeMultiArray, 0, type, dimensions, counts);
}

Reference
make(Context* c, object type)
{
  return appendEvent(c, Event::Make, 0, type, 0);
}

Reference
makeArray(Context* c, unsigned type, Integer length)
{
  return appendEvent
    (c, Event::MakeArray, new (c->allocator) Event::MakeArrayContext(type), 0,
     1, length);
}

void
storeStoreMemoryBarrier(Context* c)
{
  appendEvent(c, Event::StoreStoreMemoryBarrier, 0, 0, 0);
}

void
resolveBootstrap(Context*, object)
{
  // ignore
}

Reference
nullCheck(Context*, Reference r)
{
  // should we record this as an event to force an entry in the stack
  // map to be created?
  return r;
}

#include "bytecode.cpp"

} // namespace local

} // namespace

namespace vm {

namespace dataflow {

Graph*
makeGraph(Thread* t, Allocator* allocator, object method, object trace)
{
  local::Context context(t, allocator, method, trace);

  local::parseBytecode(&context);

  // todo: visit exception handlers if trace == 0

  return context.graph;
}

} // namespace dataflow

} // namespace vm
