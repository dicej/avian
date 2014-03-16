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
    ArrayStore,
    Load,
    Store,
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
    Release,
    LookupSwitch,
    TableSwitch,
    ReturnFromSubroutine,
    MakeMultiArray,
    MakeArray,
    Make,
    StoreStoreMemoryBarrier,
    SafePoint
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
    LoadContext(unsigned offset, unsigned size, bool signExtend = true):
      offset(offset),
      size(size),
      signExtend(signExtend)
    { }

    unsigned offset;
    unsigned size;
    bool signExtend;
  };

  class StoreContext {
   public:
    StoreContext(unsigned offset, unsigned size):
      offset(offset),
      size(size)
    { }

    unsigned offset;
    unsigned size;
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

  class LookupSwitchContext {
   public:
    LookupSwitchContext(object code, int32_t base, int32_t default_,
                        int32_t pairCount):
      code(code),
      base(base),
      default_(default_),
      pairCount(pairCount)
    { }

    object code;
    int32_t base;
    int32_t default_;
    int32_t pairCount;
  };

  class TableSwitchContext {
   public:
    TableSwitchContext(object code, int32_t base, int32_t default_,
                       int32_t bottom, int32_t top):
      code(code),
      base(base),
      default_(default_),
      bottom(bottom),
      top(top)
    { }

    object code;
    int32_t base;
    int32_t default_;
    int32_t bottom;
    int32_t top;
  };

  class MakeArrayContext {
   public:
    MakeArrayContext(unsigned type): type(type) { }

    unsigned type;
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
  Instruction(Frame* entry, unsigned ip):
    entry(entry),
    exit(0),
    ip(ip)
  { }

  Frame* entry;
  Frame* exit;
  unsigned ip;
};

class Graph {
 public:
  Graph(unsigned length)
  {
    memset(instructions, 0, length * BytesPerWord);
  }

  Instruction* instructions[0];
};

Graph*
makeGraph(Thread* t, Allocator* allocator, object method, object trace);

} // namespace dataflow

} // namespace vm
