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

class Event {
 public:
  enum Type {
    Add,
    Subtract,
    // etc
  };

  Event(Type type, unsigned readCount):
    type(type),
    readCount(readCount)
  { }

  Type type;
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

class ValueState {
 public:
  ValueState(Value* value, Read* read):
    value(value),
    read(read)
  { }

  Value* value;
  Read* read;
};

class Read {
 public:
  Read(Event* event, unsigned position, ValueState* next):
    event(event),
    next(next),
    position(position)
  { }

  Event* event;
  ValueState* next;
  unsigned position;
};

class Frame {
 public:
  Frame(ValueState** locals, ValueState** stack, unsigned sp):
    locals(locals),
    stack(stack),
    sp(sp)
  { }

  ValueState** locals;
  ValueState** stack;
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
