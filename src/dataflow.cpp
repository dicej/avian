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

typedef ValueState* Integer;
typedef ValueState* Reference;
typedef ValueState* Long;
typedef ValueState* Double;
typedef ValueState* Float;

class State {
 public:
  State(object method, unsigned ip):
    method(method),
    ip(ip)
  { }

  object method;
  unsigned ip;
};

class Context;

ValueState*
valueState(Context* c, object type, bool exactType = false, object value = 0);

class Context {
 public:
  class MyProtector: public Thread::Protector {
   public:
    MyProtector(Context* context):
      Protector(context->t), context(context)
    { }

    virtual void visit(Heap::Visitor* v) {
      v->visit(&(context->trace));
      v->visit(&(context->method));
      
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
    instruction(0),
    graph
    (static_cast<Graph*>
     (allocator->allocate
      (sizeof(Graph)
       + (((trace ? traceLength(t, trace)
            : codeLength(t, methodCode(t, method))) + 1) * BytesPerWord)))),
    states(t->m->system, allocator, 0),
    frame(new (allocator) Frame
          (static_cast<ValueState**>
           (allocator->allocate(codeMaxLocals(t, methodCode(t, method)))),
           static_cast<ValueState**>
           (allocator->allocate(codeMaxStack(t, methodCode(t, method)))), 0)),
    values(0),
    frameMask(0),
    ip(InvalidIp),
    protector(this)
  {
    if (trace) {
      for (unsigned i = 0; i < arrayLength(t, traceLocals(t, trace)); ++i) {
        object type = arrayBody(t, traceLocals(t, trace), i);

        frame->locals[i] = type ? valueState(this, type) : 0;
      }

      for (unsigned i = 0; i < arrayLength(t, traceStack(t, trace)); ++i) {
        object type = arrayBody(t, traceStack(t, trace), i);

        frame->stack[i] = type ? valueState(this, type) : 0;
      }

      frame->sp = arrayLength(t, traceStack(t, trace));
    } else {
      unsigned i = 0;
      if ((methodFlags(t, method) & ACC_STATIC) == 0) {
        frame->locals[i++] = valueState(this, methodClass(t, method));
      }

      for (MethodSpecIterator it
             (t, reinterpret_cast<const char*>
              (&byteArrayBody(t, methodSpec(t, method), 0)));
           it.hasNext();)
      {
        switch (*it.next()) {
        case 'L':
        case '[':
          frame->locals[i++] = valueState(this, type(t, Machine::JobjectType));
          break;
      
        case 'J':
          frame->locals[i++] = valueState(this, type(t, Machine::JlongType));
          frame->locals[i++] = 0;
          break;

        case 'D':
          frame->locals[i++] = valueState(this, type(t, Machine::JdoubleType));
          frame->locals[i++] = 0;
          break;
      
        case 'F':
          frame->locals[i++] = valueState(this, type(t, Machine::JfloatType));
          break;
      
        default:
          frame->locals[i++] = valueState(this, type(t, Machine::JintType));
          break;
        }
      }
    }
  }

  Thread* t;
  Allocator* allocator;
  object method;
  object trace;
  Instruction* instruction;
  Graph* graph;
  Zone states;
  Frame* frame;
  Value* values;
  unsigned frameMask;
  unsigned ip;
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
  return c->ip;
}

object
contextMethod(Context* c)
{
  return c->method;
}

ResolveStrategy
contextResolveStrategy(Context*)
{
  return NoResolve;
}

void
exitInstruction(Context* c)
{
  if (c->instruction) {
    c->instruction->exit = c->frame;
    c->instruction = 0;
    c->frameMask = 0;
  }
}

void
next(Context* c)
{
  if (c->states.isEmpty()) {
    c->method = 0;
    c->ip = InvalidIp;
  } else {
    State* s = static_cast<State*>(c->states.peek(sizeof(State)));
    c->method = s->method;
    c->ip = s->ip;
    c->states.pop(sizeof(State));
  }
}

bool
visitInstruction(Context* c)
{
  exitInstruction(c);

  Instruction* i = c->graph->instructions[c->ip];
  if (i) {
    if (i->exit) {
      // todo: append successor reads to predecessor
      next(c);
      return false;
    }
  } else {
    Instruction* i = new (c->allocator) Instruction(c->frame);
    c->instruction = i;
    c->graph->instructions[c->ip] = i;

    c->frameMask = 0;
  }

  return true;
}

ValueState**
copy(Context* c, ValueState** array, unsigned copyCount, unsigned size)
{
  ValueState** newArray = static_cast<ValueState**>
    (c->allocator->allocate(size * BytesPerWord));

  memcpy(newArray, array, copyCount * BytesPerWord);

  return newArray;
}

ValueState**
copy(Context* c, ValueState** array, unsigned size)
{
  return copy(c, array, size, size);
}

void
pushInt(Context* c, ValueState* v)
{
  if ((c->frameMask & DirtyStack) == 0) {
    c->frame = new (c->allocator) Frame
      (c->frame->locals, copy
       (c, c->frame->stack, c->frame->sp, codeMaxStack
        (c->t, methodCode(c->t, c->method))), c->frame->sp);

    c->frameMask |= DirtyStack;
  }

  c->frame->stack[c->frame->sp ++] = v;
}

void
pushReference(Context* c, ValueState* v)
{
  pushInt(c, v);
}

void
popFrame(Context* c)
{
  next(c);
}

ValueState*
popInt(Context* c)
{
  if (c->frameMask == 0) {
    c->frame = new (c->allocator) Frame(*(c->frame));

    c->frameMask |= DirtyStackPointer;
  }

  return c->frame->stack[-- c->frame->sp];
}

ValueState*
popReference(Context* c)
{
  return popInt(c);
}

ValueState*
localInt(Context* c, unsigned index)
{
  return c->frame->locals[index];
}

ValueState*
localReference(Context* c, unsigned index)
{
  return localInt(c, index);
}

void
setLocalInt(Context* c, unsigned index, ValueState* value)
{
  if ((c->frameMask & DirtyLocals) == 0) {
    c->frame = new (c->allocator) Frame
      (copy
       (c, c->frame->locals, codeMaxLocals
        (c->t, methodCode(c->t, c->method))), c->frame->stack, c->frame->sp);

    c->frameMask |= DirtyLocals;
  }

  c->frame->locals[index] = value;
}

void
setLocalReference(Context* c, unsigned index, ValueState* value)
{
  setLocalInt(c, index, value);
}

ValueState*
valueState(Context* c, object type, bool exactType, object value)
{
  return new (c->allocator) ValueState
    (new (c->allocator) Value(type, exactType, value, c->values), 0);
}

ValueState*
referenceArrayLoad(Context* c, ValueState* array, ValueState*)
{
  object type = array->value->type;
  Thread* t = c->t;

  if (type != vm::type(t, Machine::JobjectType)) {
    assert(t, classArrayElementSize(t, type) == BytesPerWord);

    type = classStaticTable(t, type);
  }

  return valueState(c, type);
}

void
referenceArrayStore(Context*, ValueState*, ValueState*, ValueState*)
{
  // ignore
}

ValueState*
makeObjectArray(Context* c, object elementType, ValueState*)
{
  return valueState
    (c, resolveObjectArrayClass
     (c->t, classLoader(c->t, elementType), elementType));
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
