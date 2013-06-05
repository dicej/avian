/* Copyright (c) 2013, Avian Contributors

   Permission to use, copy, modify, and/or distribute this software
   for any purpose with or without fee is hereby granted, provided
   that the above copyright notice and this permission notice appear
   in all copies.

   There is NO WARRANTY for this software.  See license.txt for
   details. */

namespace vm {

namespace dataflow {

template <class F, class S>
class Pair {
 public:
  Pair(F* first, S* second):
    first(first),
    second(second)
  { }

  F* first;
  S* second;
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
    type(type)
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
    next(next),
    referencers(0),
    escaped(false),
    exactType(exactType)
  { }

  object type;
  object value;
  Cell<Value>* referencers;
  Value* next;
  bool escaped;
  bool exactType;
};

class Read {
 public:
  Read(Event* event, unsigned position, Pair<Value,Read>* next):
    event(event),
    next(next),
    position(position),
  { }

  Event* event;
  Pair<Value,Read> next;
  unsigned position;
};

class Graph {
 public:
  Graph() { }

  Pair<Value,Read>** reads[0];
};

Graph*
makeGraph(Thread* t, Allocator* allocator, object method, object trace);

} // namespace dataflow

} // namespace vm
