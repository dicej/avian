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
    Multiply,
    Divide,
    Remainder,
    Negate,
    And,
    Or,
    Xor,
    DoubleCompareGreater,
    DoubleCompareLess,
    FloatToDouble,
    FloatToInt,
    FloatToLong,
    FloatCompareGreater,
    FloatCompareLess,
    ArrayLoad,
    ReferenceArrayStore,
    Load,
    MakeObjectArray,
    Throw,
    CheckCast,
    DoubleToFloat,
    DoubleToInt,
    DoubleToLong,
    IntToByte,
    IntToChar,
    IntToShort,
    IntToDouble,
    IntToFloat,
    IntToLong,
    AcquireForRead,
    ReleaseForRead,
    AcquireForWrite,
    ReleaseForWrite,
    Jump,
    Branch,
    Equal,
    Greater,
    Less,
    NotNull,
    InstanceOf,
    InvokeVirtual,
    InvokeDirect,
    LongToDouble,
    LongToFloat,
    LongToInt,
    ShiftLeft,
    ShiftRight,
    UnsignedShiftRight,
    JumpToSubroutine,
    Compare,
    Acquire,
    Release
    // etc
  };

  class ArrayLoadContext {
   public:
    ArrayLoadContext(unsigned size, bool signExtend):
      size(size),
      signExtend(signExtend)
    { }

    unsigned size;
    unsigned signExtend;
  };

  class ArrayStoreContext {
   public:
    ArrayStoreContext(unsigned size): size(size) { }

    unsigned size;
  };

  class LoadContext {
   public:
    LoadContext(unsigned offset, unsigned sourceSize, unsigned size,
                bool signExtend = true):
      offset(offset),
      sourceSize(sourceSize),
      size(size),
      signExtend(signExtend)
    { }

    unsigned offset;
    unsigned sourceSize;
    unsigned size;
    bool signExtend;
  };

  class ObjectContext {
   public:
    ObjectContext(object value): value(value) { }

    object value;
  };

  class JumpContext {
   public:
    JumpContext(unsigned ip): ip(ip) { }

    unsigned ip;
  };

  class BranchContext {
   public:
    BranchContext(unsigned ifTrue, unsigned ifFalse):
      ifTrue(ifTrue), ifFalse(ifFalse)
    { }

    unsigned ifTrue;
    unsigned ifFalse;
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
  union V {
    V(int64_t integer): integer(integer) { }
    V(object reference): reference(reference) { }

    int64_t integer;
    object reference;
  };

  Value(object type, bool exactType, V value):
    type(type),
    value(value),
    referencers(0),
    escaped(false),
    exactType(exactType)
  { }

  object type;
  V value;
  List<Value>* referencers;
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
