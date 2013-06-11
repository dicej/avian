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
pushInt(Context* c, Operand* v)
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
pushReference(Context* c, Operand* v)
{
  pushInt(c, v);
}

void
pushFloat(Context* c, Operand* v)
{
  pushInt(c, v);
}

void
pushLong(Context* c, Operand* v)
{
  pushInt(c, v);
  pushInt(c, 0);
}

void
pushDouble(Context* c, Operand* v)
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

Operand*
popInt(Context* c)
{
  dirtyStackPointer(c);

  return c->frame->stack[-- c->frame->sp];
}

Operand*
popReference(Context* c)
{
  return popInt(c);
}

Operand*
popFloat(Context* c)
{
  return popInt(c);
}

Operand*
popLong(Context* c)
{
  expect(c->t, popInt(c) == 0);
  return popInt(c);
}

Operand*
popDouble(Context* c)
{
  return popLong(c);
}

Operand*
peekReference(Context* c)
{
  return c->frame->stack[c->frame->sp - 1];
}

Operand*
localInt(Context* c, unsigned index)
{
  return c->frame->locals[index];
}

Operand*
localReference(Context* c, unsigned index)
{
  return localInt(c, index);
}

Operand*
localFloat(Context* c, unsigned index)
{
  return localInt(c, index);
}

Operand*
localLong(Context* c, unsigned index)
{
  return localInt(c, index);
}

Operand*
localDouble(Context* c, unsigned index)
{
  return localInt(c, index);
}

void
setLocalInt(Context* c, unsigned index, Operand* value)
{
  if ((c->frameMask & DirtyLocals) == 0) {
    c->frame = new (c->allocator) Frame
      (copy(c, c->frame->locals, maxLocals(c)), c->frame->stack, c->frame->sp);

    c->frameMask |= DirtyLocals;
  }

  c->frame->locals[index] = value;
}

void
setLocalReference(Context* c, unsigned index, Operand* value)
{
  setLocalInt(c, index, value);
}

void
setLocalFloat(Context* c, unsigned index, Operand* value)
{
  setLocalInt(c, index, value);
}

void
setLocalLong(Context* c, unsigned index, Operand* value)
{
  setLocalInt(c, index, value);
  setLocalInt(c, index + 1, 0);
}

void
setLocalDouble(Context* c, unsigned index, Operand* value)
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

Operand*
referenceArrayLoad(Context* c, Operand* array, Operand* index)
{
  object type = array->value->type;
  Thread* t = c->t;

  if (type != vm::type(t, Machine::JobjectType)) {
    assert(t, classArrayElementSize(t, type) == BytesPerWord);

    type = classStaticTable(t, type);
  }

  return arrayLoad(c, array, index, type, BytesPerWord);
}

Operand*
byteArrayLoad(Context* c, Operand* array, Operand* index)
{
  return arrayLoad(c, array, index, type(c->t, Machine::JintType), 1);
}

Operand*
charArrayLoad(Context* c, Operand* array, Operand* index)
{
  return arrayLoad(c, array, index, type(c->t, Machine::JintType), 2, false);
}

Operand*
doubleArrayLoad(Context* c, Operand* array, Operand* index)
{
  return arrayLoad(c, array, index, type(c->t, Machine::JdoubleType), 8);
}

Operand*
longArrayLoad(Context* c, Operand* array, Operand* index)
{
  return arrayLoad(c, array, index, type(c->t, Machine::JlongType), 8);
}

Operand*
floatArrayLoad(Context* c, Operand* array, Operand* index)
{
  return arrayLoad(c, array, index, type(c->t, Machine::JfloatType), 4);
}

Operand*
intArrayLoad(Context* c, Operand* array, Operand* index)
{
  return arrayLoad(c, array, index, type(c->t, Machine::JintType), 4);
}

void
referenceArrayStore(Context* c, Operand* array, Operand* index, Operand* value)
{
  appendEvent(c, Event::ReferenceArrayStore, 0, 0, 3, array, index, value);
}

void
arrayStore(Context* c, Operand* array, Operand* index, Operand* value,
           unsigned size)
{
  appendEvent
    (c, Event::ArrayLoad, new (c->allocator) Event::ArrayStoreContext(size),
     0, 3, array, index, value);
}

void
byteArrayStore(Context* c, Operand* array, Operand* index, Operand* value)
{
  return arrayStore(c, array, index, value, 1);
}

void
charArrayStore(Context* c, Operand* array, Operand* index, Operand* value)
{
  return arrayStore(c, array, index, value, 2);
}

void
doubleArrayStore(Context* c, Operand* array, Operand* index, Operand* value)
{
  return arrayStore(c, array, index, value, 8);
}

void
longArrayStore(Context* c, Operand* array, Operand* index, Operand* value)
{
  return arrayStore(c, array, index, value, 8);
}

void
floatArrayStore(Context* c, Operand* array, Operand* index, Operand* value)
{
  return arrayStore(c, array, index, value, 4);
}

void
intArrayStore(Context* c, Operand* array, Operand* index, Operand* value)
{
  return arrayStore(c, array, index, value, 4);
}

Operand*
intConstant(Context* c, int32_t value)
{
  return operand(c, type(c->t, Machine::JintType), true, value);
}

Operand*
longConstant(Context* c, int64_t value)
{
  return operand(c, type(c->t, Machine::JlongType), true, value);
}

Operand*
referenceConstant(Context* c, object value)
{
  return operand
    (c, value ? objectClass(c->t, value) : type(c->t, Machine::JobjectType),
     value != 0, value);
}

Operand*
doubleConstant(Context* c, double value)
{
  return operand
    (c, type(c->t, Machine::JdoubleType), true, doubleToBits(value));
}

Operand*
floatConstant(Context* c, float value)
{
  return operand
    (c, type(c->t, Machine::JfloatType), true, floatToBits(value));
}

Operand*
makeObjectArray(Context* c, object elementType, Operand* size)
{
  return appendEvent
    (c, Event::MakeObjectArray, 0, resolveObjectArrayClass
     (c->t, classLoader(c->t, elementType), elementType), 1, size);
}

Operand*
loadInt(Context* c, Operand* target, unsigned offset, unsigned size = 4,
        bool signExtend = true)
{
  return appendEvent
    (c, Event::Load, new (c->allocator) Event::LoadContext
     (offset, size, 4, signExtend), type(c->t, Machine::JintType), 1, target);
}

Operand*
loadByte(Context* c, Operand* target, unsigned offset)
{
  return loadInt(c, target, offset, 1);
}

Operand*
loadChar(Context* c, Operand* target, unsigned offset)
{
  return loadInt(c, target, offset, 2, false);
}

Operand*
loadShort(Context* c, Operand* target, unsigned offset)
{
  return loadInt(c, target, offset, 2);
}

Operand*
loadFloat(Context* c, Operand* target, unsigned offset)
{
  return appendEvent
    (c, Event::Load, new (c->allocator) Event::LoadContext(offset, 4, 4),
     type(c->t, Machine::JfloatType), 1, target);
}

Operand*
loadDouble(Context* c, Operand* target, unsigned offset)
{
  return appendEvent
    (c, Event::Load, new (c->allocator) Event::LoadContext(offset, 8, 8),
     type(c->t, Machine::JdoubleType), 1, target);
}

Operand*
loadLong(Context* c, Operand* target, unsigned offset)
{
  return appendEvent
    (c, Event::Load, new (c->allocator) Event::LoadContext(offset, 8, 8),
     type(c->t, Machine::JlongType), 1, target);
}

Operand*
loadReference(Context* c, Operand* target, unsigned offset)
{
  return appendEvent
    (c, Event::Load, new (c->allocator) Event::LoadContext
     (offset, BytesPerWord, BytesPerWord), type(c->t, Machine::JobjectType),
     1, target);
}

void
throwException(Context* c, Operand* exception)
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
checkCast(Context* c, object type, Operand* instance)
{
  appendEvent(c, Event::CheckCast, objectContext(c, type), 0, 1, instance);
}

Operand*
instanceOf(Context* c, object type, Operand* instance)
{
  return appendEvent
    (c, Event::InstanceOf, objectContext(c, type),
     vm::type(c->t, Machine::JintType), 1, instance);
}

Operand*
intToByte(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::IntToByte, 0, type(c->t, Machine::JintType), 1, v);
}

Operand*
intToChar(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::IntToChar, 0, type(c->t, Machine::JintType), 1, v);
}

Operand*
intToShort(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::IntToShort, 0, type(c->t, Machine::JintType), 1, v);
}

Operand*
intToDouble(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::IntToDouble, 0, type(c->t, Machine::JdoubleType), 1, v);
}

Operand*
intToFloat(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::IntToFloat, 0, type(c->t, Machine::JfloatType), 1, v);
}

Operand*
intToLong(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::IntToLong, 0, type(c->t, Machine::JlongType), 1, v);
}

Operand*
longToDouble(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::LongToDouble, 0, type(c->t, Machine::JdoubleType), 1, v);
}

Operand*
longToFloat(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::LongToFloat, 0, type(c->t, Machine::JfloatType), 1, v);
}

Operand*
longToInt(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::LongToInt, 0, type(c->t, Machine::JintType), 1, v);
}

Operand*
intAdd(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Add, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
intSubtract(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Subtract, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
intAnd(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::And, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
intOr(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Or, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
intXor(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Xor, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
intMultiply(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Multiply, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
intDivide(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Divide, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
intRemainder(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Remainder, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
intShiftLeft(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::ShiftLeft, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
intShiftRight(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::ShiftRight, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
intUnsignedShiftRight(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::UnsignedShiftRight, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
intNegate(Context* c, Operand* a)
{
  return appendEvent
    (c, Event::Negate, 0, type(c->t, Machine::JintType), 1, a);
}

Operand*
intEqual(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Equal, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
intGreater(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Greater, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
intLess(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Less, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
longAdd(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Add, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Operand*
longSubtract(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Subtract, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Operand*
longAnd(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::And, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Operand*
longOr(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Or, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Operand*
longXor(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Xor, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Operand*
longMultiply(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Multiply, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Operand*
longDivide(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Divide, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Operand*
longRemainder(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Remainder, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Operand*
longShiftLeft(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::ShiftLeft, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Operand*
longShiftRight(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::ShiftRight, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Operand*
longUnsignedShiftRight(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::UnsignedShiftRight, 0, type(c->t, Machine::JlongType), 2, a, b);
}

Operand*
longCompare(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Compare, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
longNegate(Context* c, Operand* a)
{
  return appendEvent
    (c, Event::Negate, 0, type(c->t, Machine::JlongType), 1, a);
}

Operand*
notNull(Context* c, Operand* a)
{
  return appendEvent
    (c, Event::NotNull, 0, type(c->t, Machine::JintType), 1, a);
}

Operand*
referenceEqual(Context* c, Operand* a, Operand* b)
{
  return intEqual(c, a, b);
}

void
branch(Context* c, Operand* condition, unsigned ifTrue, unsigned ifFalse)
{
  // todo: trace case

  appendEvent
    (c, Event::Branch, new (c->allocator) Event::BranchContext
     (ifTrue, ifFalse), 0, 1, condition);

  new (&(c->states)) State(c->state.method, c->state.instruction, ifFalse);
  
  c->state.ip = ifTrue;
}

Operand*
doubleToFloat(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::DoubleToFloat, 0, type(c->t, Machine::JfloatType), 1, v);
}

Operand*
doubleToInt(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::DoubleToInt, 0, type(c->t, Machine::JintType), 1, v);
}

Operand*
doubleToLong(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::DoubleToLong, 0, type(c->t, Machine::JlongType), 1, v);
}

Operand*
doubleAdd(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Add, 0, type(c->t, Machine::JdoubleType), 2, a, b);
}

Operand*
doubleSubtract(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Subtract, 0, type(c->t, Machine::JdoubleType), 2, a, b);
}

Operand*
doubleDivide(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Divide, 0, type(c->t, Machine::JdoubleType), 2, a, b);
}

Operand*
doubleMultiply(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Multiply, 0, type(c->t, Machine::JdoubleType), 2, a, b);
}

Operand*
doubleRemainder(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Remainder, 0, type(c->t, Machine::JdoubleType), 2, a, b);
}

Operand*
doubleNegate(Context* c, Operand* a)
{
  return appendEvent
    (c, Event::Negate, 0, type(c->t, Machine::JdoubleType), 1, a);
}

Operand*
doubleCompareGreater(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::DoubleCompareGreater, 0, type(c->t, Machine::JintType), 2, a,
     b);
}

Operand*
doubleCompareLess(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::DoubleCompareLess, 0, type(c->t, Machine::JintType), 2, a, b);
}

Operand*
floatToDouble(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::FloatToDouble, 0, type(c->t, Machine::JdoubleType), 1, v);
}

Operand*
floatToInt(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::FloatToInt, 0, type(c->t, Machine::JintType), 1, v);
}

Operand*
floatToLong(Context* c, Operand* v)
{
  return appendEvent
    (c, Event::FloatToLong, 0, type(c->t, Machine::JlongType), 1, v);
}

Operand*
floatAdd(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Add, 0, type(c->t, Machine::JfloatType), 2, a, b);
}

Operand*
floatSubtract(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Subtract, 0, type(c->t, Machine::JfloatType), 2, a, b);
}

Operand*
floatDivide(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Divide, 0, type(c->t, Machine::JfloatType), 2, a, b);
}

Operand*
floatMultiply(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Multiply, 0, type(c->t, Machine::JfloatType), 2, a, b);
}

Operand*
floatRemainder(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::Remainder, 0, type(c->t, Machine::JfloatType), 2, a, b);
}

Operand*
floatNegate(Context* c, Operand* a)
{
  return appendEvent
    (c, Event::Negate, 0, type(c->t, Machine::JfloatType), 1, a);
}

Operand*
floatCompareGreater(Context* c, Operand* a, Operand* b)
{
  return appendEvent
    (c, Event::FloatCompareGreater, 0, type(c->t, Machine::JintType), 2, a,
     b);
}

Operand*
floatCompareLess(Context* c, Operand* a, Operand* b)
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
invokeDirect(Context* c, object method, bool isStatic)
{
  invoke(c, method, Event::InvokeDirect, isStatic);
}

void
acquire(Context* c, Operand* instance)
{
  appendEvent(c, Event::Acquire, 0, 0, 1, instance);
}

void
release(Context* c, Operand* instance)
{
  appendEvent(c, Event::Release, 0, 0, 1, instance);
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

  return context.graph;
}

} // namespace dataflow

} // namespace vm
