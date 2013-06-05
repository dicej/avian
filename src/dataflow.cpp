/* Copyright (c) 2013, Avian Contributors

   Permission to use, copy, modify, and/or distribute this software
   for any purpose with or without fee is hereby granted, provided
   that the above copyright notice and this permission notice appear
   in all copies.

   There is NO WARRANTY for this software.  See license.txt for
   details. */

#include "avian/dataflow.h"

using namespace vm;
using namespace vm::dataflow;

namespace {

namespace local {

class Context {
 public:
  class MyProtector: public Thread::Protector {
   public:
    MyProtector(Context* context):
      Protector(context->t), context(context)
    { }

    virtual void visit(Heap::Visitor* v) {
      v->visit(&(context->method));
      v->visit(&(context->trace));
      
      for (Value* value = graph->values; value; value = value->next) {
        v->visit(&(value->type));
        v->visit(&(value->value));      
      }
    }

    Context* context;
  };

  Context(Thread* t, Allocator* allocator, object method, object trace):
    t(t),
    allocator(allocator),
    method(method),
    trace(trace),
    graph(allocator->allocate
          (sizeof(Graph)
           + ((trace ? traceLength(t, trace)
               : codeLength(t, methodCode(t, method))) * BytesPerWord))),
    ip(trace ? traceStart(t, trace) : 0),
    protector(this)
  { }

  Thread* t;
  Allocator* allocator;
  object method;
  object trace;
  Graph* graph;
  unsigned ip;
  MyProtector protector;
};

typedef Value* Integer;
typedef Value* Reference;
typedef Value* Long;
typedef Value* Double;
typedef Value* Float;

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

// tbc

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

  return context->result;
}

} // namespace dataflow

} // namespace vm
