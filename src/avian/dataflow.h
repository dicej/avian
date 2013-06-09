/* Copyright (c) 2013, Avian Contributors

   Permission to use, copy, modify, and/or distribute this software
   for any purpose with or without fee is hereby granted, provided
   that the above copyright notice and this permission notice appear
   in all copies.

   There is NO WARRANTY for this software.  See license.txt for
   details. */

#include "avian/machine.h"

namespace vm {

namespace dataflow {

template <class T>
class List {
 public:
  List(T* value, List<T>* next):
    value(value),
    next(next)
  { }

  T* value;
  List<T>* next;
};

class Read;
class Value;

class Event {
 public:
  enum Type {
    Add,
    Subtract,
    ArrayLoad,
    ReferenceArrayStore,
    Load,
    MakeObjectArray,
    Throw,
    // etc
  };

  class ArrayLoadContext {
   public:
    ArrayLoadContext(unsigned size): size(size) { }

    unsigned size;
  };

  class LoadContext {
   public:
    LoadContext(unsigned offset, unsigned sourceSize, unsigned size):
      offset(offset),
      sourceSize(sourceSize),
      size(size)
    { }

    unsigned offset;
    unsigned sourceSize;
    unsigned size;
  };

  Event(Type type, void* context, Value* result, unsigned readCount):
    type(type),
    context(context),
    result(result),
    readCount(readCount)
  { }

  Type type;
  void* context;
  Value* result;
  unsigned readCount;
  Read* reads[0];
};

class Value {
 public:
  Value(object type, bool exactType, object value, Value* next):
    type(type),
    value(value),
    referencers(0),
    next(next),
    escaped(false),
    exactType(exactType)
  { }

  object type;
  object value;
  List<Value>* referencers;
  Value* next;
  bool escaped;
  bool exactType;
};

class Operand {
 public:
  Operand(Value* value, Read* read):
    value(value),
    read(read)
  { }

  Value* value;
  Read* read;
};

class Read {
 public:
  Read(Event* event, unsigned position, Operand* next):
    event(event),
    next(next),
    position(position)
  { }

  Event* event;
  Operand* next;
  unsigned position;
};

class Frame {
 public:
  Frame(Operand** locals, Operand** stack, unsigned sp):
    locals(locals),
    stack(stack),
    sp(sp)
  { }

  Operand** locals;
  Operand** stack;
  unsigned sp;
};

class Instruction {
 public:
  Instruction(Frame* entry):
    entry(entry),
    exit(0)
  { }

  Frame* entry;
  Frame* exit;
};

class Graph {
 public:
  Graph() { }

  Instruction* instructions[0];
};

Graph*
makeGraph(Thread* t, Allocator* allocator, object method, object trace);

} // namespace dataflow

} // namespace vm
