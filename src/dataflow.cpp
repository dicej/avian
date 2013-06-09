/* Copyright (c) 2013, Avian Contributors

   Permission to use, copy, modify, and/or distribute this software
   for any purpose with or without fee is hereby granted, provided
   that the above copyright notice and this permission notice appear
   in all copies.

   There is NO WARRANTY for this software.  See license.txt for
   details. */

#include "avian/dataflow.h"
#include "avian/process.h"

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

class Context;

Operand*
operand(Context* c, object type, bool exactType = false, object value = 0);

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
      
      for (Value* value = context->values; value; value = value->next) {
        v->visit(&(value->type));
        v->visit(&(value->value));      
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
    values(0),
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
  Value* values;
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
popFrame(Context* c)
{
  next(c);
}

Operand*
popInt(Context* c)
{
  if (c->frameMask == 0) {
    c->frame = new (c->allocator) Frame(*(c->frame));

    c->frameMask |= DirtyStackPointer;
  }

  return c->frame->stack[-- c->frame->sp];
}

Operand*
popReference(Context* c)
{
  return popInt(c);
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

Operand*
operand(Context* c, object type, bool exactType, object value)
{
  return new (c->allocator) Operand
    (new (c->allocator) Value(type, exactType, value, c->values), 0);
}

Operand*
appendEvent(Context* c, Event::Type eventType, void* context,
            object resultType, unsigned readCount, ...)
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

  va_list a; va_start(a, readCount);
  for (unsigned i = 0; i < readCount; ++i) {
    Operand* current = va_arg(a, Operand*);
    followReads(&current);

    Operand* next = new (c->allocator) Operand(current->value, 0);
    Read* read = new (c->allocator) Read(event, i, next);
    current->read = read;
    event->reads[i] = read;
    
    c->frameMask |= DirtyReads;
  }
  va_end(a);

  return result;
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

  return appendEvent
    (c, Event::ArrayLoad, new (c->allocator) Event::ArrayLoadContext
     (BytesPerWord), type, 2, array, index);
}

void
referenceArrayStore(Context* c, Operand* array, Operand* index, Operand* value)
{
  appendEvent(c, Event::ReferenceArrayStore, 0, 0, 3, array, index, value);
}

Operand*
makeObjectArray(Context* c, object elementType, Operand* size)
{
  return appendEvent
    (c, Event::MakeObjectArray, 0, resolveObjectArrayClass
     (c->t, classLoader(c->t, elementType), elementType), 1, size);
}

Operand*
loadInt(Context* c, Operand* target, unsigned offset, unsigned size = 4)
{
  return appendEvent
    (c, Event::Load, new (c->allocator) Event::LoadContext
     (offset, size, 4), type(c->t, Machine::JintType), 1, target);
}

void
throwException(Context* c, Operand* exception)
{
  appendEvent(c, Event::Throw, 0, 0, 1, exception);
  next(c);
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
