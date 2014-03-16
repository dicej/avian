/* Copyright (c) 2008-2013, Avian Contributors

   Permission to use, copy, modify, and/or distribute this software
   for any purpose with or without fee is hereby granted, provided
   that the above copyright notice and this permission notice appear
   in all copies.

   There is NO WARRANTY for this software.  See license.txt for
   details. */

#include "avian/common.h"
#include <avian/system/system.h>
#include <avian/system/signal.h>
#include "avian/constants.h"
#include "avian/machine.h"
#include "avian/processor.h"
#include "avian/process.h"
#include "avian/arch.h"
#include "avian/dataflow.h"

#include <avian/util/runtime-array.h>
#include <avian/util/list.h>
#include <avian/util/slice.h>

using namespace vm;
using namespace avian::system;

namespace local {

const unsigned FrameBaseOffset = 0;
const unsigned FrameNextOffset = 1;
const unsigned FrameMethodOffset = 2;
const unsigned FrameIpOffset = 3;
const unsigned FrameFootprint = 4;

#define CONTEXT_ACQUIRE_FIELD_FOR_READ ACQUIRE_FIELD_FOR_READ
#define CONTEXT_ACQUIRE_FIELD_FOR_WRITE ACQUIRE_FIELD_FOR_WRITE

class Context: public Thread {
 public:

  Context(Machine* m, object javaThread, Thread* parent):
    Thread(m, javaThread, parent),
    ip(0),
    sp(0),
    frame(-1),
    code(0),
    stackPointers(0)
  { }

  unsigned ip;
  unsigned sp;
  int frame;
  int base;
  object code;
  List<unsigned>* stackPointers;
  uintptr_t stack[0];
};

typedef object Reference;
typedef int32_t Integer;
typedef int64_t Long;
typedef float Float;
typedef double Double;

Thread*
contextThread(Context* t)
{
  return t;
}

unsigned&
contextIp(Context* t)
{
  return t->ip;
}

ResolveStrategy
contextResolveStrategy(Context*)
{
  return ResolveOrThrow;
}

bool
visitInstruction(Context*)
{
  return true;
}

#define ARRAY_OPERATION(action)                                         \
  if (LIKELY(array)) {                                                  \
    if (LIKELY(index >= 0                                               \
               and static_cast<uintptr_t>(index)                        \
               < fieldAtOffset<uintptr_t>(array, BytesPerWord)))        \
    {                                                                   \
      action;                                                           \
    } else {                                                            \
      throwNew(t, Machine::ArrayIndexOutOfBoundsExceptionType,          \
               "%d not in [0,%d)", index, fieldAtOffset<uintptr_t>      \
               (array, BytesPerWord));                                  \
    }                                                                   \
  } else {                                                              \
    throwNew(t, Machine::NullPointerExceptionType);                     \
  }

Reference
referenceArrayLoad(Context* t, Reference array, Integer index)
{
  ARRAY_OPERATION(return objectArrayBody(t, array, index));
}

void
referenceArrayStore(Context* t, Reference array, Integer index,
                    Reference value)
{
  ARRAY_OPERATION(set(t, array, ArrayBody + (index * BytesPerWord), value));
}

Reference
nullCheck(Context* t, Reference o)
{
  if (LIKELY(o)) {
    return o;
  } else {
    throwNew(t, Machine::NullPointerExceptionType);
  }
}

Integer
loadWord(Context*, Reference target, unsigned offset)
{
  return fieldAtOffset<intptr_t>(target, offset);
}

Integer
loadInt(Context*, Reference target, unsigned offset)
{
  return fieldAtOffset<Integer>(target, offset);
}

Integer
loadByte(Context*, Reference target, unsigned offset)
{
  return fieldAtOffset<int8_t>(target, offset);
}

Integer
loadChar(Context*, Reference target, unsigned offset)
{
  return fieldAtOffset<uint16_t>(target, offset);
}

Integer
loadShort(Context*, Reference target, unsigned offset)
{
  return fieldAtOffset<int16_t>(target, offset);
}

Float
loadFloat(Context*, Reference target, unsigned offset)
{
  return fieldAtOffset<float>(target, offset);
}

Double
loadDouble(Context*, Reference target, unsigned offset)
{
  return fieldAtOffset<double>(target, offset);
}

Long
loadLong(Context*, Reference target, unsigned offset)
{
  return fieldAtOffset<int64_t>(target, offset);
}

Reference
loadReference(Context*, Reference target, unsigned offset)
{
  return fieldAtOffset<object>(target, offset);
}

void
throwException(Context* t, Reference exception)
{
  throw_(t, exception);
}

Integer
intArrayLoad(Context* t, Reference array, Integer index)
{
  ARRAY_OPERATION
    (return fieldAtOffset<int32_t>(array, (BytesPerWord * 2) + (index * 4)));
}

void
intArrayStore(Context* t, Reference array, Integer index, Integer value)
{
  ARRAY_OPERATION
    (fieldAtOffset<int32_t>(array, (BytesPerWord * 2) + (index * 4)) = value);
}

Long
longArrayLoad(Context* t, Reference array, Integer index)
{
  ARRAY_OPERATION
    (return fieldAtOffset<int64_t>(array, (BytesPerWord * 2) + (index * 8)));
}

void
longArrayStore(Context* t, Reference array, Integer index, Long value)
{
  ARRAY_OPERATION
    (fieldAtOffset<int64_t>(array, (BytesPerWord * 2) + (index * 8)) = value);
}

Integer
byteArrayLoad(Context* t, Reference array, Integer index)
{
  ARRAY_OPERATION
    (return fieldAtOffset<int8_t>(array, (BytesPerWord * 2) + index));
}

void
byteArrayStore(Context* t, Reference array, Integer index, Integer value)
{
  ARRAY_OPERATION
    (fieldAtOffset<int8_t>(array, (BytesPerWord * 2) + index) = value);
}

Integer
shortArrayLoad(Context* t, Reference array, Integer index)
{
  ARRAY_OPERATION
    (return fieldAtOffset<int16_t>(array, (BytesPerWord * 2) + (index * 2)));
}

void
shortArrayStore(Context* t, Reference array, Integer index, Integer value)
{
  ARRAY_OPERATION
    (fieldAtOffset<int16_t>(array, (BytesPerWord * 2) + (index * 2)) = value);
}

Integer
charArrayLoad(Context* t, Reference array, Integer index)
{
  ARRAY_OPERATION
    (return fieldAtOffset<uint16_t>(array, (BytesPerWord * 2) + (index * 2)));
}

void
charArrayStore(Context* t, Reference array, Integer index, Integer value)
{
  ARRAY_OPERATION
    (fieldAtOffset<uint16_t>(array, (BytesPerWord * 2) + (index * 2)) = value);
}

Double
doubleArrayLoad(Context* t, Reference array, Integer index)
{
  ARRAY_OPERATION
    (return fieldAtOffset<double>(array, (BytesPerWord * 2) + (index * 8)));
}

void
doubleArrayStore(Context* t, Reference array, Integer index, Double value)
{
  ARRAY_OPERATION
    (fieldAtOffset<double>(array, (BytesPerWord * 2) + (index * 8)) = value);
}

Float
floatArrayLoad(Context* t, Reference array, Integer index)
{
  ARRAY_OPERATION
    (return fieldAtOffset<float>(array, (BytesPerWord * 2) + (index * 4)));
}

void
floatArrayStore(Context* t, Reference array, Integer index, Float value)
{
  ARRAY_OPERATION
    (fieldAtOffset<float>(array, (BytesPerWord * 2) + (index * 4)) = value);
}

bool
isNaN(double v)
{
  return fpclassify(v) == FP_NAN;
}

bool
isNaN(float v)
{
  return fpclassify(v) == FP_NAN;
}

Integer
doubleCompareGreater(Context*, Double a, Double b)
{
  if (isNaN(a) or isNaN(b)) {
    return 1;
  } if (a < b) {
    return -1;
  } else if (a > b) {
    return 1;
  } else if (a == b) {
    return 0;
  } else {
    return 1;
  }
}

Integer
doubleCompareLess(Context*, Double a, Double b)
{
  if (isNaN(a) or isNaN(b)) {
    return -1;
  } if (a < b) {
    return -1;
  } else if (a > b) {
    return 1;
  } else if (a == b) {
    return 0;
  } else {
    return -1;
  }
}

Integer
floatCompareGreater(Context*, Float a, Float b)
{
  if (isNaN(a) or isNaN(b)) {
    return 1;
  } if (a < b) {
    return -1;
  } else if (a > b) {
    return 1;
  } else if (a == b) {
    return 0;
  } else {
    return 1;
  }
}

Integer
floatCompareLess(Context*, Float a, Float b)
{
  if (isNaN(a) or isNaN(b)) {
    return -1;
  } if (a < b) {
    return -1;
  } else if (a > b) {
    return 1;
  } else if (a == b) {
    return 0;
  } else {
    return -1;
  }
}

Integer
intConstant(Context*, int32_t v)
{
  return v;
}

Reference
referenceConstant(Context*, object v)
{
  return v;
}

Long
longConstant(Context*, int64_t v)
{
  return v;
}

Double
doubleConstant(Context*, double v)
{
  return v;
}

Float
floatConstant(Context*, float v)
{
  return v;
}

void
checkCast(Context* t, object type, Reference instance)
{
  if (UNLIKELY(instance and (not instanceOf(t, type, instance)))) {
    throwNew(t, Machine::ClassCastExceptionType, "%s as %s",
             &byteArrayBody
             (t, className(t, objectClass(t, instance)), 0),
             &byteArrayBody(t, className(t, type), 0));
  }
}

Float
doubleToFloat(Context*, Double v)
{
  return v;
}

Integer
doubleToInt(Context*, Double v)
{
  switch (fpclassify(v)) {
  case      FP_NAN: return 0;
  case FP_INFINITE: return signbit(v) ? INT32_MIN : INT32_MAX;
  default         : return v >= INT32_MAX ? INT32_MAX
      : (v <= INT32_MIN ? INT32_MIN : static_cast<int32_t>(v));
  }
}

Long
doubleToLong(Context*, Double v)
{
  switch (fpclassify(v)) {
  case      FP_NAN: return 0;
  case FP_INFINITE: return signbit(v) ? INT64_MIN : INT64_MAX;
  default         : return v >= INT64_MAX ? INT64_MAX
      : (v <= INT64_MIN ? INT64_MIN : static_cast<int64_t>(v));
  }
}

Double
doubleAdd(Context*, Double a, Double b)
{
  return a + b;
}

Double
doubleSubtract(Context*, Double a, Double b)
{
  return a - b;
}

Double
doubleDivide(Context*, Double a, Double b)
{
  return a / b;
}

Double
doubleRemainder(Context*, Double a, Double b)
{
  return fmod(a, b);
}

Double
doubleMultiply(Context*, Double a, Double b)
{
  return a * b;
}

Double
doubleNegate(Context*, Double a)
{
  return - a;
}

Double
floatToDouble(Context*, Float v)
{
  return v;
}

Integer
floatToInt(Context*, Float v)
{
  switch (fpclassify(v)) {
  case      FP_NAN: return 0;
  case FP_INFINITE: return signbit(v) ? INT32_MIN : INT32_MAX;
  default         : return v >= INT32_MAX ? INT32_MAX
      : (v <= INT32_MIN ? INT32_MIN : static_cast<int32_t>(v));
  }
}

Long
floatToLong(Context*, Float v)
{
  switch (fpclassify(v)) {
  case      FP_NAN: return 0;
  case FP_INFINITE: return signbit(v) ? INT64_MIN : INT64_MAX;
  default         : return v >= INT64_MAX ? INT64_MAX
      : (v <= INT64_MIN ? INT64_MIN : static_cast<int64_t>(v));
  }
}

Float
floatAdd(Context*, Float a, Float b)
{
  return a + b;
}

Float
floatSubtract(Context*, Float a, Float b)
{
  return a - b;
}

Float
floatDivide(Context*, Float a, Float b)
{
  return a / b;
}

Float
floatRemainder(Context*, Float a, Float b)
{
  return fmod(a, b);
}

Float
floatMultiply(Context*, Float a, Float b)
{
  return a * b;
}

Float
floatNegate(Context*, Float a)
{
  return - a;
}

void
duplicate(Context* t, unsigned instruction)
{
  unsigned& sp = t->sp;
  uintptr_t* stack = t->stack;

  switch (instruction) {
  case dup:
    memcpy(stack + (sp    ), stack + (sp - 1), BytesPerWord);
    ++ sp;
    break;

  case dup_x1: {
    memcpy(stack + (sp    ), stack + (sp - 1), BytesPerWord);
    memcpy(stack + (sp - 1), stack + (sp - 2), BytesPerWord);
    memcpy(stack + (sp - 2), stack + (sp    ), BytesPerWord);
    ++ sp;
  } break;

  case dup_x2: {
    memcpy(stack + (sp    ), stack + (sp - 1), BytesPerWord);
    memcpy(stack + (sp - 1), stack + (sp - 2), BytesPerWord);
    memcpy(stack + (sp - 2), stack + (sp - 3), BytesPerWord);
    memcpy(stack + (sp - 3), stack + (sp    ), BytesPerWord);
    ++ sp;
  } break;

  case dup2: {
    memcpy(stack + (sp    ), stack + (sp - 2), BytesPerWord * 2);
    sp += 2;
  } break;

  case dup2_x1: {
    memcpy(stack + (sp + 1), stack + (sp - 1), BytesPerWord);
    memcpy(stack + (sp    ), stack + (sp - 2), BytesPerWord);
    memcpy(stack + (sp - 1), stack + (sp - 3), BytesPerWord);
    memcpy(stack + (sp - 3), stack + (sp    ), BytesPerWord * 2);
    sp += 2;
  } break;

  case dup2_x2: {
    memcpy(stack + (sp + 1), stack + (sp - 1), BytesPerWord);
    memcpy(stack + (sp    ), stack + (sp - 2), BytesPerWord);
    memcpy(stack + (sp - 1), stack + (sp - 3), BytesPerWord);
    memcpy(stack + (sp - 2), stack + (sp - 4), BytesPerWord);
    memcpy(stack + (sp - 4), stack + (sp    ), BytesPerWord * 2);
    sp += 2;
  } break;

  default: abort(t);
  }
}

Integer
intToByte(Context*, Integer v)
{
  return static_cast<int8_t>(v);
}

Integer
intToShort(Context*, Integer v)
{
  return static_cast<int16_t>(v);
}

Integer
intToChar(Context*, Integer v)
{
  return static_cast<uint16_t>(v);
}

Double
intToDouble(Context*, Integer v)
{
  return v;
}

Float
intToFloat(Context*, Integer v)
{
  return v;
}

Long
intToLong(Context*, Integer v)
{
  return v;
}

Double
longToDouble(Context*, Long v)
{
  return v;
}

Float
longToFloat(Context*, Long v)
{
  return v;
}

Integer
longToInt(Context*, Long v)
{
  return v;
}

Integer
longCompare(Context*, Long a, Long b)
{
  return a > b ? 1 : a == b ? 0 : -1;
}

Long
longNegate(Context*, Long a)
{
  return - a;
}

Long
longAdd(Context*, Long a, Long b)
{
  return a + b;
}

Long
longShiftLeft(Context*, Long a, Integer b)
{
  return a << b;
}

Long
longShiftRight(Context*, Long a, Integer b)
{
  return a >> b;
}

Long
longUnsignedShiftRight(Context*, Long a, Integer b)
{
  return static_cast<uint64_t>(a) >> b;
}

Long
longSubtract(Context*, Long a, Long b)
{
  return a - b;
}

Long
longDivide(Context* t, Long a, Long b)
{
  if (UNLIKELY(b == 0)) {
    throwNew(t, Machine::ArithmeticExceptionType);
  }

  return a / b;
}

Long
longRemainder(Context* t, Long a, Long b)
{
  if (UNLIKELY(b == 0)) {
    throwNew(t, Machine::ArithmeticExceptionType);
  }

  return a % b;
}

Long
longMultiply(Context*, Long a, Long b)
{
  return a * b;
}

Long
longAnd(Context*, Long a, Long b)
{
  return a & b;
}

Long
longOr(Context*, Long a, Long b)
{
  return a | b;
}

Long
longXor(Context*, Long a, Long b)
{
  return a ^ b;
}

Integer
intNegate(Context*, Integer a)
{
  return - a;
}

Integer
intAdd(Context*, Integer a, Integer b)
{
  return a + b;
}

Integer
intShiftLeft(Context*, Integer a, Integer b)
{
  return a << b;
}

Integer
intShiftRight(Context*, Integer a, Integer b)
{
  return a >> b;
}

Integer
intUnsignedShiftRight(Context*, Integer a, Integer b)
{
  return static_cast<uint32_t>(a) >> b;
}

Integer
intSubtract(Context*, Integer a, Integer b)
{
  return a - b;
}

Integer
intDivide(Context* t, Integer a, Integer b)
{
  if (UNLIKELY(b == 0)) {
    throwNew(t, Machine::ArithmeticExceptionType);
  }

  return a / b;
}

Integer
intRemainder(Context* t, Integer a, Integer b)
{
  if (UNLIKELY(b == 0)) {
    throwNew(t, Machine::ArithmeticExceptionType);
  }

  return a % b;
}

Integer
intMultiply(Context*, Integer a, Integer b)
{
  return a * b;
}

Integer
intAnd(Context*, Integer a, Integer b)
{
  return a & b;
}

Integer
intOr(Context*, Integer a, Integer b)
{
  return a | b;
}

Integer
intXor(Context*, Integer a, Integer b)
{
  return a ^ b;
}

Integer
intEqual(Context*, Integer a, Integer b)
{
  return a == b;
}

Integer
referenceEqual(Context*, Reference a, Reference b)
{
  return a == b;
}

Integer
notNull(Context*, Reference a)
{
  return a != 0;
}

Integer
intGreater(Context*, Integer a, Integer b)
{
  return a > b;
}

Integer
intLess(Context*, Integer a, Integer b)
{
  return a < b;
}

void
jump(Context* t, unsigned ip)
{
  t->ip = ip;
}

void
branch(Context* t, Integer condition, unsigned ifTrue, unsigned ifFalse)
{
  t->ip = condition ? ifTrue : ifFalse;
}

object*
pushReference(Context* t, object o)
{
  if (DebugStack) {
    fprintf(stderr, "push object %p at %d\n", o, t->sp);
  }

  assert(t, t->sp < stackSizeInWords(t));
  object* p = reinterpret_cast<object*>(t->stack + (t->sp ++));
  *p = o;

  return p;
}

void
pushInt(Context* t, uint32_t v)
{
  if (DebugStack) {
    fprintf(stderr, "push int %d at %d\n", v, t->sp);
  }

  assert(t, t->sp < stackSizeInWords(t));
  t->stack[t->sp ++] = v;
}

void
pushFloat(Context* t, float v)
{
  pushInt(t, floatToBits(v));
}

void
pushLong(Context* t, uint64_t v)
{
  if (DebugStack) {
    fprintf(stderr, "push long %" LLD " at %d\n", v, t->sp);
  }

  assert(t, t->sp + 2 <= stackSizeInWords(t));
  memcpy(t->stack + t->sp, &v, 8);
  t->sp += 2;
}

void
pushDouble(Context* t, double v)
{
  pushLong(t, doubleToBits(v));
}

object
popReference(Context* t)
{
  assert(t, t->sp);

  if (DebugStack) {
    fprintf(stderr, "pop object %p at %d\n",
            reinterpret_cast<object>(t->stack[t->sp - 1]),
            t->sp - 1);
  }

  return reinterpret_cast<object>(t->stack[-- t->sp]);
}

uint32_t
popInt(Context* t)
{
  assert(t, t->sp);

  if (DebugStack) {
    fprintf(stderr, "pop int %" ULD " at %d\n",
            t->stack[t->sp - 1],
            t->sp - 1);
  }

  return t->stack[-- t->sp];
}

float
popFloat(Context* t)
{
  return bitsToFloat(popInt(t));
}

uint64_t
popLong(Context* t)
{
  assert(t, t->sp >= 2);

  t->sp -= 2;

  uint64_t v; memcpy(&v, t->stack + t->sp, 8);

  if (DebugStack) {
    fprintf(stderr, "pop long %" LLD " at %d\n", v, t->sp);
  }
  
  return v;
}

double
popDouble(Context* t)
{
  return bitsToDouble(popLong(t));
}

object&
peekObject(Context* t, unsigned index)
{
  if (DebugStack) {
    fprintf(stderr, "peek object %p at %d\n",
            reinterpret_cast<object>(t->stack[index]),
            index);
  }

  return *reinterpret_cast<object*>(t->stack + index);
}

object
peekReference(Context* t)
{
  return peekObject(t, t->sp - 1);
}

uint32_t
peekInt(Context* t, unsigned index)
{
  if (DebugStack) {
    fprintf(stderr, "peek int %" ULD " at %d\n",
            t->stack[index],
            index);
  }

  return t->stack[index];
}

uint64_t
peekLong(Context* t, unsigned index)
{
  uint64_t v; memcpy(&v, t->stack + index, 8);

  if (DebugStack) {
    fprintf(stderr, "peek long %" LLD " at %d\n", v, index);
  }
  
  return v;
}

void
pokeObject(Context* t, unsigned index, object value)
{
  if (DebugStack) {
    fprintf(stderr, "poke object %p at %d\n", value, index);
  }

  t->stack[index] = reinterpret_cast<uintptr_t>(value);
}

void
pokeInt(Context* t, unsigned index, uint32_t value)
{
  if (DebugStack) {
    fprintf(stderr, "poke int %d at %d\n", value, index);
  }

  t->stack[index] = value;
}

void
pokeLong(Context* t, unsigned index, uint64_t value)
{
  if (DebugStack) {
    fprintf(stderr, "poke long %" LLD " at %d\n", value, index);
  }

  memcpy(t->stack + index, &value, 8);
}

int
frameNext(Context* t, int frame)
{
  return peekInt(t, frame + FrameNextOffset);
}

object&
frameMethod(Context* t, int frame)
{
  return peekObject(t, frame + FrameMethodOffset);
}

unsigned
frameIp(Context* t, int frame)
{
  return peekInt(t, frame + FrameIpOffset);
}

unsigned
frameBase(Context* t, int frame)
{
  return peekInt(t, frame + FrameBaseOffset);
}

object
contextMethod(Context* t)
{
  return t->frame >= t->base ? frameMethod(t, t->frame) : 0;
}

object
localReference(Context* t, unsigned index)
{
  return peekObject(t, frameBase(t, t->frame) + index);
}

uint32_t
localInt(Context* t, unsigned index)
{
  return peekInt(t, frameBase(t, t->frame) + index);
}

uint64_t
localLong(Context* t, unsigned index)
{
  return peekLong(t, frameBase(t, t->frame) + index);
}

Float
localFloat(Context* t, unsigned index)
{
  return bitsToFloat(peekInt(t, frameBase(t, t->frame) + index));
}

Double
localDouble(Context* t, unsigned index)
{
  return bitsToDouble(peekLong(t, frameBase(t, t->frame) + index));
}

void
storeByte(Context*, Reference target, unsigned offset, Integer value)
{
  fieldAtOffset<int8_t>(target, offset) = value;
}

void
storeShort(Context*, Reference target, unsigned offset, Integer value)
{
  fieldAtOffset<int16_t>(target, offset) = value;
}

void
storeInt(Context*, Reference target, unsigned offset, Integer value)
{
  fieldAtOffset<int32_t>(target, offset) = value;
}

void
storeFloat(Context*, Reference target, unsigned offset, Float value)
{
  fieldAtOffset<float>(target, offset) = value;
}

void
storeLong(Context*, Reference target, unsigned offset, Long value)
{
  fieldAtOffset<int64_t>(target, offset) = value;
}

void
storeDouble(Context*, Reference target, unsigned offset, Double value)
{
  fieldAtOffset<double>(target, offset) = value;
}

void
storeReference(Context* t, Reference target, unsigned offset, Reference value)
{
  set(t, target, offset, value);
}

void
setLocalReference(Context* t, unsigned index, object value)
{
  pokeObject(t, frameBase(t, t->frame) + index, value);
}

void
setLocalInt(Context* t, unsigned index, uint32_t value)
{
  pokeInt(t, frameBase(t, t->frame) + index, value);
}

void
setLocalFloat(Context* t, unsigned index, float value)
{
  setLocalInt(t, index, floatToBits(value));
}

void
setLocalLong(Context* t, unsigned index, uint64_t value)
{
  pokeLong(t, frameBase(t, t->frame) + index, value);
}

void
setLocalDouble(Context* t, unsigned index, double value)
{
  setLocalLong(t, index, doubleToBits(value));
}

void
pushFrame(Context* t, object method)
{
  PROTECT(t, method);

  unsigned parameterFootprint = methodParameterFootprint(t, method);
  unsigned base = t->sp - parameterFootprint;
  unsigned locals = parameterFootprint;

  if (methodFlags(t, method) & ACC_SYNCHRONIZED) {
    // Try to acquire the monitor before doing anything else.
    // Otherwise, if we were to push the frame first, we risk trying
    // to release a monitor we never successfully acquired when we try
    // to pop the frame back off.
    if (methodFlags(t, method) & ACC_STATIC) {
      acquire(t, methodClass(t, method));
    } else {
      acquire(t, peekObject(t, base));
    }   
  }

  if (t->frame >= 0) {
    pokeInt(t, t->frame + FrameIpOffset, t->ip);
  }
  t->ip = 0;

  if ((methodFlags(t, method) & ACC_NATIVE) == 0) {
    t->code = methodCode(t, method);

    locals = codeMaxLocals(t, t->code);
  }

  unsigned frame = base + locals;

  // if (methodName(t, method)) {
  //   fprintf(stderr, "push %s.%s%s %d %d\n",
  //           &byteArrayBody(t, className(t, methodClass(t, method)), 0),
  //           &byteArrayBody(t, methodName(t, method), 0),
  //           &byteArrayBody(t, methodSpec(t, method), 0), t->frame, frame);
  // } else {
  //   fprintf(stderr, "push stub %d %d\n", t->frame, frame);
  // }
      
  pokeInt(t, frame + FrameNextOffset, t->frame);
  t->frame = frame;

  t->sp = frame + FrameFootprint;

  pokeInt(t, frame + FrameBaseOffset, base);
  pokeObject(t, frame + FrameMethodOffset, method);
  pokeInt(t, t->frame + FrameIpOffset, 0);
}

void
popFrame(Context* t)
{
  assert(t, t->frame >= 0);

  object method = frameMethod(t, t->frame);

  // if (methodName(t, method)) {
  //   fprintf(stderr, "pop %s.%s%s %d %d\n",
  //           &byteArrayBody(t, className(t, methodClass(t, method)), 0),
  //           &byteArrayBody(t, methodName(t, method), 0),
  //           &byteArrayBody(t, methodSpec(t, method), 0),
  //           t->frame,
  //           frameNext(t, t->frame));
  // } else {
  //   fprintf(stderr, "pop stub %d %d\n", t->frame, frameNext(t, t->frame));
  // }

  if (methodFlags(t, method) & ACC_SYNCHRONIZED) {
    if (methodFlags(t, method) & ACC_STATIC) {
      release(t, methodClass(t, method));
    } else {
      release(t, peekObject(t, frameBase(t, t->frame)));
    }   
  }

  t->sp = frameBase(t, t->frame);
  t->frame = frameNext(t, t->frame);
  if (t->frame >= 0) {
    t->code = methodCode(t, frameMethod(t, t->frame));
    t->ip = frameIp(t, t->frame);
  } else {
    t->code = 0;
    t->ip = 0;
  }
}

class MyStackWalker: public Processor::StackWalker {
 public:
  MyStackWalker(Context* t, int frame): t(t), frame(frame) { }

  virtual void walk(Processor::StackVisitor* v) {
    for (int frame = this->frame; frame >= 0; frame = frameNext(t, frame)) {
      MyStackWalker walker(t, frame);
      if (not v->visit(&walker)) {
        break;
      }
    }
  }

  virtual object method() {
    return frameMethod(t, frame);
  }

  virtual int ip() {
    return frameIp(t, frame);
  }

  virtual unsigned count() {
    unsigned count = 0;
    for (int frame = this->frame; frame >= 0; frame = frameNext(t, frame)) {
      ++ count;
    }
    return count;
  }

  Context* t;
  int frame;
};

void
pushResult(Context* t, unsigned returnCode, uint64_t result, bool indirect)
{
  switch (returnCode) {
  case ByteField:
  case BooleanField:
    if (DebugRun) {
      fprintf(stderr, "result: %d\n", static_cast<int8_t>(result));
    }
    pushInt(t, static_cast<int8_t>(result));
    break;

  case CharField:
    if (DebugRun) {
      fprintf(stderr, "result: %d\n", static_cast<uint16_t>(result));
    }
    pushInt(t, static_cast<uint16_t>(result));
    break;

  case ShortField:
    if (DebugRun) {
      fprintf(stderr, "result: %d\n", static_cast<int16_t>(result));
    }
    pushInt(t, static_cast<int16_t>(result));
    break;

  case FloatField:
  case IntField:
    if (DebugRun) {
      fprintf(stderr, "result: %d\n", static_cast<int32_t>(result));
    }
    pushInt(t, result);
    break;

  case DoubleField:
  case LongField:
    if (DebugRun) {
      fprintf(stderr, "result: %" LLD "\n", result);
    }
    pushLong(t, result);
    break;

  case ObjectField:
    if (indirect) {
      if (DebugRun) {
        fprintf(stderr, "result: %p at %p\n",
                static_cast<uintptr_t>(result) == 0 ? 0 :
                *reinterpret_cast<object*>(static_cast<uintptr_t>(result)),
                reinterpret_cast<object*>(static_cast<uintptr_t>(result)));
      }
      pushReference(t, static_cast<uintptr_t>(result) == 0 ? 0 :
                 *reinterpret_cast<object*>(static_cast<uintptr_t>(result)));
    } else {
      if (DebugRun) {
        fprintf(stderr, "result: %p\n", reinterpret_cast<object>(result));
      }
      pushReference(t, reinterpret_cast<object>(result));
    }
    break;

  case VoidField:
    break;

  default:
    abort(t);
  }
}

void
marshalArguments(Context* t, uintptr_t* args, uint8_t* types, unsigned sp,
                 object method, bool fastCallingConvention)
{
  MethodSpecIterator it
    (t, reinterpret_cast<const char*>
     (&byteArrayBody(t, methodSpec(t, method), 0)));
  
  unsigned argOffset = 0;
  unsigned typeOffset = 0;

  while (it.hasNext()) {
    unsigned type = fieldType(t, fieldCode(t, *it.next()));
    if (types) {
      types[typeOffset++] = type;
    }

    switch (type) {
    case INT8_TYPE:
    case INT16_TYPE:
    case INT32_TYPE:
    case FLOAT_TYPE:
      args[argOffset++] = peekInt(t, sp++);
      break;

    case DOUBLE_TYPE:
    case INT64_TYPE: {
      uint64_t v = peekLong(t, sp);
      memcpy(args + argOffset, &v, 8);
      argOffset += fastCallingConvention ? 2 : (8 / BytesPerWord);
      sp += 2;
    } break;

    case POINTER_TYPE: {
      if (fastCallingConvention) {
        args[argOffset++] = reinterpret_cast<uintptr_t>(peekObject(t, sp++));
      } else {
        object* v = reinterpret_cast<object*>(t->stack + (sp++));
        if (*v == 0) {
          v = 0;
        }
        args[argOffset++] = reinterpret_cast<uintptr_t>(v);
      }
    } break;

    default: abort(t);
    }
  }
}

unsigned
invokeNativeSlow(Context* t, object method, void* function)
{
  PROTECT(t, method);

  pushFrame(t, method);

  unsigned footprint = methodParameterFootprint(t, method) + 1;
  if (methodFlags(t, method) & ACC_STATIC) {
    ++ footprint;
  }
  unsigned count = methodParameterCount(t, method) + 2;

  THREAD_RUNTIME_ARRAY(t, uintptr_t, args, footprint);
  unsigned argOffset = 0;
  THREAD_RUNTIME_ARRAY(t, uint8_t, types, count);
  unsigned typeOffset = 0;

  RUNTIME_ARRAY_BODY(args)[argOffset++] = reinterpret_cast<uintptr_t>(t);
  RUNTIME_ARRAY_BODY(types)[typeOffset++] = POINTER_TYPE;

  object jclass = 0;
  PROTECT(t, jclass);

  unsigned sp;
  if (methodFlags(t, method) & ACC_STATIC) {
    sp = frameBase(t, t->frame);
    jclass = getJClass(t, methodClass(t, method));
    RUNTIME_ARRAY_BODY(args)[argOffset++]
      = reinterpret_cast<uintptr_t>(&jclass);
  } else {
    sp = frameBase(t, t->frame);
    object* v = reinterpret_cast<object*>(t->stack + ((sp++) * 2) + 1);
    if (*v == 0) {
      v = 0;
    }
    RUNTIME_ARRAY_BODY(args)[argOffset++] = reinterpret_cast<uintptr_t>(v);
  }
  RUNTIME_ARRAY_BODY(types)[typeOffset++] = POINTER_TYPE;

  marshalArguments
    (t, RUNTIME_ARRAY_BODY(args) + argOffset,
     RUNTIME_ARRAY_BODY(types) + typeOffset, sp, method, false);

  unsigned returnCode = methodReturnCode(t, method);
  unsigned returnType = fieldType(t, returnCode);
  uint64_t result;

  if (DebugRun) {
    fprintf(stderr, "invoke native method %s.%s\n",
            &byteArrayBody(t, className(t, methodClass(t, method)), 0),
            &byteArrayBody(t, methodName(t, method), 0));
  }
    
  { ENTER(t, Thread::IdleState);

    bool noThrow = t->checkpoint->noThrow;
    t->checkpoint->noThrow = true;
    THREAD_RESOURCE(t, bool, noThrow, t->checkpoint->noThrow = noThrow);

    result = vm::dynamicCall
      (function,
       RUNTIME_ARRAY_BODY(args),
       RUNTIME_ARRAY_BODY(types),
       count,
       footprint * BytesPerWord,
       returnType);
  }

  if (DebugRun) {
    fprintf(stderr, "return from native method %s.%s\n",
            &byteArrayBody
            (t, className(t, methodClass(t, frameMethod(t, t->frame))), 0),
            &byteArrayBody
            (t, methodName(t, frameMethod(t, t->frame)), 0));
  }

  popFrame(t);

  if (UNLIKELY(t->exception)) {
    object exception = t->exception;
    t->exception = 0;
    throw_(t, exception);
  }

  pushResult(t, returnCode, result, true);

  return returnCode;
}

unsigned
invokeNative(Context* t, object method)
{
  PROTECT(t, method);

  resolveNative(t, method);

  object native = methodRuntimeDataNative(t, getMethodRuntimeData(t, method));
  if (nativeFast(t, native)) {
    pushFrame(t, method);

    uint64_t result;
    { THREAD_RESOURCE0(t, popFrame(static_cast<Context*>(t)));

      unsigned footprint = methodParameterFootprint(t, method);
      THREAD_RUNTIME_ARRAY(t, uintptr_t, args, footprint);
      unsigned sp = frameBase(t, t->frame);
      unsigned argOffset = 0;
      if ((methodFlags(t, method) & ACC_STATIC) == 0) {
        RUNTIME_ARRAY_BODY(args)[argOffset++]
          = reinterpret_cast<uintptr_t>(peekObject(t, sp++));
      }

      marshalArguments
        (t, RUNTIME_ARRAY_BODY(args) + argOffset, 0, sp, method, true);

      result = reinterpret_cast<FastNativeFunction>
        (nativeFunction(t, native))(t, method, RUNTIME_ARRAY_BODY(args));
    }

    pushResult(t, methodReturnCode(t, method), result, false);

    return methodReturnCode(t, method);
  } else {
    return invokeNativeSlow(t, method, nativeFunction(t, native));
  }
}

void
checkStack(Context* t, object method)
{
  if (UNLIKELY(t->sp
               + methodParameterFootprint(t, method)
               + codeMaxLocals(t, methodCode(t, method))
               + FrameFootprint
               + codeMaxStack(t, methodCode(t, method))
               > stackSizeInWords(t)))
  {
    throwNew(t, Machine::StackOverflowErrorType);
  }
}

void
doInvoke(Context* t, object method)
{
  if (methodFlags(t, method) & ACC_NATIVE) {
    invokeNative(t, method);
  } else {
    checkStack(t, method);
    pushFrame(t, method);
  }
}

void
invokeInterface(Context* t, object method)
{
  unsigned parameterFootprint = methodParameterFootprint(t, method);
  object o = peekObject(t, t->sp - parameterFootprint);
  if (LIKELY(o)) {
    doInvoke(t, findInterfaceMethod(t, method, objectClass(t, o)));
  } else {
    throwNew(t, Machine::NullPointerExceptionType);
  }
}

void
invokeVirtual(Context* t, object method)
{
  unsigned parameterFootprint = methodParameterFootprint(t, method);
  object o = peekObject(t, t->sp - parameterFootprint);
  if (LIKELY(o)) {
    doInvoke(t, findVirtualMethod(t, method, objectClass(t, o)));
  } else {
    throwNew(t, Machine::NullPointerExceptionType);
  }
}

void
invokeDirect(Context* t, object method, bool)
{
  doInvoke(t, method);
}

void
jumpToSubroutine(Context* t, unsigned ip)
{
  t->ip = ip;
}

void
returnFromSubroutine(Context* t, unsigned ip)
{
  t->ip = ip;
}

void
storeStoreMemoryBarrier(Context*)
{
  vm::storeStoreMemoryBarrier();
}

void
lookupSwitch(Context* t, Integer key, object code, int32_t base,
             int32_t default_, int32_t pairCount)
{
  int32_t bottom = 0;
  int32_t top = pairCount;
  for (int32_t span = top - bottom; span; span = top - bottom) {
    int32_t middle = bottom + (span / 2);
    unsigned index = t->ip + (middle * 8);

    int32_t k = codeReadInt32(t, code, index);

    if (key < k) {
      top = middle;
    } else if (key > k) {
      bottom = middle + 1;
    } else {
      t->ip = base + codeReadInt32(t, code, index);
      return;
    }
  }

  t->ip = base + default_;
}

void
tableSwitch(Context* t, Integer key, object code, int32_t base,
            int32_t default_, int32_t bottom, int32_t top)
{
  if (key >= bottom and key <= top) {
    unsigned index = t->ip + ((key - bottom) * 4);
    t->ip = base + codeReadInt32(t, code, index);
  } else {
    t->ip = base + default_;
  }
}
void
resolveBootstrap(Context* t, object method)
{
  unsigned parameterFootprint = methodParameterFootprint(t, method);
  object class_ = objectClass(t, peekObject(t, t->sp - parameterFootprint));
  assert(t, classVmFlags(t, class_) & BootstrapFlag);
    
  resolveClass(t, classLoader(t, methodClass(t, frameMethod(t, t->frame))),
               className(t, class_));
}

Reference
makeMultiArray(Context* t, Integer* counts, unsigned dimensions, object type)
{
  object array = makeArray(t, counts[0]);
  setObjectClass(t, array, type);
  PROTECT(t, array);
  
  populateMultiArray(t, array, counts, 0, dimensions);

  return array;
}

Reference
makeArray(Context* t, unsigned type, Integer count)
{
  switch (type) {
  case T_BOOLEAN:
    return makeBooleanArray(t, count);

  case T_CHAR:
    return makeCharArray(t, count);

  case T_FLOAT:
    return makeFloatArray(t, count);

  case T_DOUBLE:
    return makeDoubleArray(t, count);

  case T_BYTE:
    return makeByteArray(t, count);

  case T_SHORT:
    return makeShortArray(t, count);

  case T_INT:
    return makeIntArray(t, count);

  case T_LONG:
    return makeLongArray(t, count);

  default: abort(t);
  }
}

void
pop(Context* t)
{
  popInt(t);
}

void
contextPop2(Context* t)
{
  popLong(t);
}

void
contextSwap(Context* t)
{
  uintptr_t tmp[2];
  memcpy(tmp                   , t->stack + (t->sp - 1), BytesPerWord);
  memcpy(t->stack + (t->sp - 1), t->stack + (t->sp - 2), BytesPerWord);
  memcpy(t->stack + (t->sp - 2), tmp                   , BytesPerWord);
}

void
store(Context* t, unsigned index)
{
  memcpy(t->stack + ((frameBase(t, t->frame) + index) * 2),
         t->stack + ((-- t->sp) * 2),
         BytesPerWord * 2);
}

uint64_t
findExceptionHandler(Context* t, object method, unsigned ip)
{
  PROTECT(t, method);

  object eht = codeExceptionHandlerTable(t, methodCode(t, method));
      
  if (eht) {
    for (unsigned i = 0; i < exceptionHandlerTableLength(t, eht); ++i) {
      uint64_t eh = exceptionHandlerTableBody(t, eht, i);

      if (ip - 1 >= exceptionHandlerStart(eh)
          and ip - 1 < exceptionHandlerEnd(eh))
      {
        object catchType = 0;
        if (exceptionHandlerCatchType(eh)) {
          object e = t->exception;
          t->exception = 0;
          PROTECT(t, e);

          PROTECT(t, eht);
          catchType = resolveClassInPool
            (t, method, exceptionHandlerCatchType(eh) - 1);

          if (catchType) {
            eh = exceptionHandlerTableBody(t, eht, i);
            t->exception = e;
          } else {
            // can't find what we're supposed to catch - move on.
            continue;
          }
        }

        if (exceptionMatch(t, catchType, t->exception)) {
          return eh;
        }
      }
    }
  }

  return 0;
}

uint64_t
findExceptionHandler(Context* t, int frame)
{
  return findExceptionHandler(t, frameMethod(t, frame), frameIp(t, frame));
}

void
safePoint(Context* t)
{
  ENTER(t, Thread::IdleState);
}

#include "bytecode.inc.cpp"

object
interpret3(Context* t, const int base)
{
  if (UNLIKELY(t->exception)) {
    pokeInt(t, t->frame + FrameIpOffset, t->ip);
    for (; t->frame >= base; popFrame(t)) {
      uint64_t eh = findExceptionHandler(t, t->frame);
      if (eh) {
        t->sp = t->frame + FrameFootprint;
        t->ip = exceptionHandlerIp(eh);
        pushReference(t, t->exception);
        t->exception = 0;
        break;
      }
    }

    if (t->exception) {
      return 0;
    }
  }

  int oldBase = t->base;
  t->base = base;
  THREAD_RESOURCE(t, int, oldBase, static_cast<Context*>(t)->base = oldBase);

  unsigned returnCode = methodReturnCode(t, contextMethod(t));

  parseBytecode(t);

  switch (returnCode) {
  case ByteField:
  case BooleanField:
  case CharField:
  case ShortField:
  case FloatField:
  case IntField:
    return makeInt(t, popInt(t));

  case DoubleField:
  case LongField:
    return makeLong(t, popLong(t));

  case ObjectField:
    return popReference(t);

  case VoidField:
    return 0;

  default:
    abort(t);
  }
}

uint64_t
interpret2(Thread* t, uintptr_t* arguments)
{
  int base = arguments[0];
  bool* success = reinterpret_cast<bool*>(arguments[1]);

  object r = interpret3(static_cast<Context*>(t), base);
  *success = true;
  return reinterpret_cast<uint64_t>(r);
}

object
interpret(Context* t)
{
  const int base = t->frame;

  while (true) {
    bool success = false;
    uintptr_t arguments[] = { static_cast<uintptr_t>(base),
                              reinterpret_cast<uintptr_t>(&success) };

    uint64_t r = run(t, interpret2, arguments);
    if (success) {
      if (t->exception) {
        object exception = t->exception;
        t->exception = 0;
        throw_(t, exception);
      } else {
        return reinterpret_cast<object>(r);
      }
    }
  }
}

void
pushArguments(Context* t, object this_, const char* spec, bool indirectObjects,
              va_list a)
{
  if (this_) {
    pushReference(t, this_);
  }

  for (MethodSpecIterator it(t, spec); it.hasNext();) {
    switch (*it.next()) {
    case 'L':
    case '[':
      if (indirectObjects) {
        object* v = va_arg(a, object*);
        pushReference(t, v ? *v : 0);
      } else {
        pushReference(t, va_arg(a, object));
      }
      break;
      
    case 'J':
    case 'D':
      pushLong(t, va_arg(a, uint64_t));
      break;

    case 'F': {
      pushFloat(t, va_arg(a, double));
    } break;

    default:
      pushInt(t, va_arg(a, uint32_t));
      break;        
    }
  }
}

void
pushArguments(Context* t, object this_, const char* spec,
              const jvalue* arguments)
{
  if (this_) {
    pushReference(t, this_);
  }

  unsigned index = 0;
  for (MethodSpecIterator it(t, spec); it.hasNext();) {
    switch (*it.next()) {
    case 'L':
    case '[': {
      jobject v = arguments[index++].l;
      pushReference(t, v ? *v : 0);
    } break;
      
    case 'J':
    case 'D':
      pushLong(t, arguments[index++].j);
      break;

    case 'F': {
      pushFloat(t, arguments[index++].f);
    } break;

    default:
      pushInt(t, arguments[index++].i);
      break;        
    }
  }
}

void
pushArguments(Context* t, object this_, const char* spec, object a)
{
  if (this_) {
    pushReference(t, this_);
  }

  unsigned index = 0;
  for (MethodSpecIterator it(t, spec); it.hasNext();) {
    switch (*it.next()) {
    case 'L':
    case '[':
      pushReference(t, objectArrayBody(t, a, index++));
      break;
      
    case 'J':
    case 'D':
      pushLong(t, fieldAtOffset<int64_t>(objectArrayBody(t, a, index++), 8));
      break;

    default:
      pushInt(t, fieldAtOffset<int32_t>(objectArrayBody(t, a, index++),
                               BytesPerWord));
      break;        
    }
  }
}

object
invoke(Context* t, object method)
{
  PROTECT(t, method);

  object class_;
  PROTECT(t, class_);

  if (methodVirtual(t, method)) {
    unsigned parameterFootprint = methodParameterFootprint(t, method);
    class_ = objectClass(t, peekObject(t, t->sp - parameterFootprint));

    if (classVmFlags(t, class_) & BootstrapFlag) {
      resolveClass(t, root(t, Machine::BootLoader), className(t, class_));
    }

    if (classFlags(t, methodClass(t, method)) & ACC_INTERFACE) {
      method = findInterfaceMethod(t, method, class_);
    } else {
      method = findVirtualMethod(t, method, class_);
    }
  } else {
    class_ = methodClass(t, method);
  }

  if (methodFlags(t, method) & ACC_STATIC) {
    initClass(t, class_);
  }

  object result = 0;

  if (methodFlags(t, method) & ACC_NATIVE) {
    unsigned returnCode = invokeNative(t, method);

    switch (returnCode) {
    case ByteField:
    case BooleanField:
    case CharField:
    case ShortField:
    case FloatField:
    case IntField:
      result = makeInt(t, popInt(t));
      break;

    case LongField:
    case DoubleField:
      result = makeLong(t, popLong(t));
      break;
        
    case ObjectField:
      result = popReference(t);
      break;

    case VoidField:
      result = 0;
      break;

    default:
      abort(t);
    };
  } else {
    checkStack(t, method);
    pushFrame(t, method);

    result = interpret(t);

    if (UNLIKELY(t->exception)) {
      object exception = t->exception;
      t->exception = 0;
      throw_(t, exception);
    }
  }

  return result;
}

class MyProcessor: public Processor {
 public:
  MyProcessor(System* s, Allocator* allocator, const char* crashDumpDirectory)
      : s(s), allocator(allocator)
  {
    signals.setCrashDumpDirectory(crashDumpDirectory);
  }

  virtual Thread*
  makeThread(Machine* m, object javaThread, Thread* parent)
  {
    Context* t = new (m->heap->allocate(sizeof(Thread) + m->stackSizeInBytes))
      Context(m, javaThread, parent);
    t->init();
    return t;
  }

  virtual object
  makeMethod(Thread* t,
             uint8_t vmFlags,
             uint8_t returnCode,
             uint8_t parameterCount,
             uint8_t parameterFootprint,
             uint16_t flags,
             uint16_t offset,
             object name,
             object spec,
             object addendum,
             object class_,
             object code)
  {
    return vm::makeMethod
      (t, vmFlags, returnCode, parameterCount, parameterFootprint, flags,
       offset, 0, 0, name, spec, addendum, class_, code);
  }

  virtual object
  makeClass(Thread* t,
            uint16_t flags,
            uint16_t vmFlags,
            uint16_t fixedSize,
            uint8_t arrayElementSize,
            uint8_t arrayDimensions,
            object objectMask,
            object name,
            object sourceFile,
            object super,
            object interfaceTable,
            object virtualTable,
            object fieldTable,
            object methodTable,
            object addendum,
            object staticTable,
            object loader,
            unsigned vtableLength UNUSED)
  {
    return vm::makeClass
      (t, flags, vmFlags, fixedSize, arrayElementSize, arrayDimensions, 0,
       objectMask, name, sourceFile, super, interfaceTable, virtualTable,
       fieldTable, methodTable, addendum, staticTable, loader, 0, 0);
  }

  virtual void
  initVtable(Thread*, object)
  {
    // ignore
  }

  virtual void
  visitObjects(Thread* vmt, Heap::Visitor* v)
  {
    Context* t = static_cast<Context*>(vmt);

    v->visit(&(t->code));

    class MyStackVisitor: public StackVisitor {
     public:
      MyStackVisitor(Context* t, Heap::Visitor* v): t(t), v(v) { }

      virtual bool visit(StackWalker* vmw) {
        MyStackWalker* walker = static_cast<MyStackWalker*>(vmw);

        v->visit(&frameMethod(t, walker->frame));

        using namespace vm::dataflow;

        if (methodCode(t, walker->method())) {
          fprintf(stderr, "visit %s.%s%s base %d\n",
                  &byteArrayBody(t, className(t, methodClass(t, walker->method())), 0),
                  &byteArrayBody(t, methodName(t, walker->method()), 0),
                  &byteArrayBody(t, methodSpec(t, walker->method()), 0),
                  frameBase(t, walker->frame));

          Zone zone(t->m->system, t->m->heap, 0, true);
          Frame* frame = makeGraph(t, &zone, walker->method(), 0)->instructions
            [walker->ip()]->entry;

          unsigned maxLocals = codeMaxLocals
            (t, methodCode(t, walker->method()));

          for (unsigned i = 0; i < maxLocals; ++i) {
            Operand* o = frame->locals[i];
            if (o and (classVmFlags(t, o->value->type) & PrimitiveFlag) == 0) {
              fprintf(stderr, "visit local %d at %d of type %s\n", i, frameBase(t, walker->frame) + i, &byteArrayBody(t, className(t, o->value->type), 0));
              v->visit(reinterpret_cast<object*>
                       (t->stack + frameBase(t, walker->frame) + i));
            }
          }

          // for (unsigned i = 0; i < frame->sp; ++i) {
          //   Operand* o = frame->locals[i];
          //   if (o and (classVmFlags(t, o->value->type) & PrimitiveFlag) == 0) {
          //     fprintf(stderr, "visit local %d at %d of type %s\n\n", i, frameBase(t, walker->frame) + i, &byteArrayBody(t, className(t, o->value->type), 0));
          //     v->visit
          //       (reinterpret_cast<object*>
          //        (t->stack + walker->frame + FrameFootprint + i));
          //   }
          // }
        } else {
          fprintf(stderr, "visit arguments %s.%s%s base %d\n",
                  &byteArrayBody(t, className(t, methodClass(t, walker->method())), 0),
                  &byteArrayBody(t, methodName(t, walker->method()), 0),
                  &byteArrayBody(t, methodSpec(t, walker->method()), 0),
                  frameBase(t, walker->frame));

          unsigned i = 0;
          if ((methodFlags(t, walker->method()) & ACC_STATIC) == 0) {
              fprintf(stderr, "visit local %d at %d\n", i, frameBase(t, walker->frame) + i);
              v->visit(reinterpret_cast<object*>
                       (t->stack + frameBase(t, walker->frame) + (i++)));
          }

          for (MethodSpecIterator it
                 (t, reinterpret_cast<const char*>
                  (&byteArrayBody(t, methodSpec(t, walker->method()), 0)));
               it.hasNext();)
          {
            switch (*it.next()) {
            case 'L':
            case '[':
              fprintf(stderr, "visit local %d at %d\n", i, frameBase(t, walker->frame) + i);
              v->visit(reinterpret_cast<object*>
                       (t->stack + frameBase(t, walker->frame) + (i++)));
              break;
      
            case 'J':
            case 'D':
              i += 2;
              break;

            default:
              ++ i;
              break;
            }
          }
        }

        return true;
      }

      Context* t;
      Heap::Visitor* v;
    } sv(t, v);

    walkStack(t, &sv);
  }

  virtual void
  walkStack(Thread* vmt, StackVisitor* v)
  {
    Context* t = static_cast<Context*>(vmt);

    if (t->frame >= 0) {
      pokeInt(t, t->frame + FrameIpOffset, t->ip);
    }

    MyStackWalker walker(t, t->frame);
    walker.walk(v);
  }

  virtual int
  lineNumber(Thread* t, object method, int ip)
  {
    return findLineNumber(static_cast<Context*>(t), method, ip);
  }

  virtual object*
  makeLocalReference(Thread* vmt, object o)
  {
    Context* t = static_cast<Context*>(vmt);

    return pushReference(t, o);
  }

  virtual void
  disposeLocalReference(Thread*, object* r)
  {
    if (r) {
      *r = 0;
    }
  }

  virtual bool
  pushLocalFrame(Thread* vmt, unsigned capacity)
  {
    Context* t = static_cast<Context*>(vmt);

    if (t->sp + capacity < stackSizeInWords(t) / 2) {
      t->stackPointers = new(t->m->heap)
        List<unsigned>(t->sp, t->stackPointers);
    
      return true;
    } else {
      return false;
    }
  }

  virtual void
  popLocalFrame(Thread* vmt)
  {
    Context* t = static_cast<Context*>(vmt);

    List<unsigned>* f = t->stackPointers;
    t->stackPointers = f->next;
    t->sp = f->item;

    t->m->heap->free(f, sizeof(List<unsigned>));
  }

  virtual object
  invokeArray(Thread* vmt, object method, object this_, object arguments)
  {
    Context* t = static_cast<Context*>(vmt);

    assert(t, t->state == Thread::ActiveState
           or t->state == Thread::ExclusiveState);

    assert(t, ((methodFlags(t, method) & ACC_STATIC) == 0) xor (this_ == 0));

    if (UNLIKELY(t->sp + methodParameterFootprint(t, method) + 1
                 > stackSizeInWords(t) / 2))
    {
      throwNew(t, Machine::StackOverflowErrorType);
    }

    const char* spec = reinterpret_cast<char*>
      (&byteArrayBody(t, methodSpec(t, method), 0));
    pushArguments(t, this_, spec, arguments);

    return local::invoke(t, method);
  }

  virtual object
  invokeArray(Thread* vmt, object method, object this_,
              const jvalue* arguments)
  {
    Context* t = static_cast<Context*>(vmt);

    assert(t, t->state == Thread::ActiveState
           or t->state == Thread::ExclusiveState);

    assert(t, ((methodFlags(t, method) & ACC_STATIC) == 0) xor (this_ == 0));

    if (UNLIKELY(t->sp + methodParameterFootprint(t, method) + 1
                 > stackSizeInWords(t) / 2))
    {
      throwNew(t, Machine::StackOverflowErrorType);
    }

    const char* spec = reinterpret_cast<char*>
      (&byteArrayBody(t, methodSpec(t, method), 0));
    pushArguments(t, this_, spec, arguments);

    return local::invoke(t, method);
  }

  virtual object
  invokeList(Thread* vmt, object method, object this_,
             bool indirectObjects, va_list arguments)
  {
    Context* t = static_cast<Context*>(vmt);

    assert(t, t->state == Thread::ActiveState
           or t->state == Thread::ExclusiveState);

    assert(t, ((methodFlags(t, method) & ACC_STATIC) == 0) xor (this_ == 0));

    if (UNLIKELY(t->sp + methodParameterFootprint(t, method) + 1
                 > stackSizeInWords(t) / 2))
    {
      throwNew(t, Machine::StackOverflowErrorType);
    }

    const char* spec = reinterpret_cast<char*>
      (&byteArrayBody(t, methodSpec(t, method), 0));
    pushArguments(t, this_, spec, indirectObjects, arguments);

    return local::invoke(t, method);
  }

  virtual object
  invokeList(Thread* vmt, object loader, const char* className,
             const char* methodName, const char* methodSpec, object this_,
             va_list arguments)
  {
    Context* t = static_cast<Context*>(vmt);

    assert(t, t->state == Thread::ActiveState
           or t->state == Thread::ExclusiveState);

    if (UNLIKELY(t->sp + parameterFootprint(vmt, methodSpec, false)
                 > stackSizeInWords(t) / 2))
    {
      throwNew(t, Machine::StackOverflowErrorType);
    }

    pushArguments(t, this_, methodSpec, false, arguments);

    object method = resolveMethod
      (t, loader, className, methodName, methodSpec);

    assert(t, ((methodFlags(t, method) & ACC_STATIC) == 0) xor (this_ == 0));

    return local::invoke(t, method);
  }

  virtual object getStackTrace(Thread* t, Thread*) {
    // not implemented
    return makeObjectArray(t, 0);
  }

  virtual void initialize(BootImage*, avian::util::Slice<uint8_t>)
  {
    abort(s);
  }

  virtual void addCompilationHandler(CompilationHandler*) {
    abort(s);
  }

  virtual void compileMethod(Thread*, Zone*, object*, object*,
                             avian::codegen::DelayedPromise**, object, OffsetResolver*)
  {
    abort(s);
  }

  virtual void visitRoots(Thread*, HeapWalker*) {
    abort(s);
  }

  virtual void normalizeVirtualThunks(Thread*) {
    abort(s);
  }

  virtual unsigned* makeCallTable(Thread*, HeapWalker*) {
    abort(s);
  }

  virtual void boot(Thread*, BootImage* image, uint8_t* code) {
    expect(s, image == 0 and code == 0);
  }

  virtual void callWithCurrentContinuation(Thread*, object) {
    abort(s);
  }

  virtual void dynamicWind(Thread*, object, object, object) {
    abort(s);
  }

  virtual void feedResultToContinuation(Thread*, object, object){
    abort(s);
  }

  virtual void feedExceptionToContinuation(Thread*, object, object) {
    abort(s);
  }

  virtual void walkContinuationBody(Thread*, Heap::Walker*, object,
                                    unsigned)
  {
    abort(s);
  }

  virtual void dispose(Thread* t) {
    t->m->heap->free(t, sizeof(Thread) + t->m->stackSizeInBytes);
  }

  virtual void dispose() {
    allocator->free(this, sizeof(*this));
    signals.setCrashDumpDirectory(0);
  }
  
  System* s;
  Allocator* allocator;
  SignalRegistrar signals;
};

} // namespace

namespace vm {

Processor* makeProcessor(System* system,
                         Allocator* allocator,
                         const char* crashDumpDirectory,
                         bool)
{
  return new (allocator->allocate(sizeof(local::MyProcessor)))
    local::MyProcessor(system, allocator, crashDumpDirectory);
}

} // namespace vm
