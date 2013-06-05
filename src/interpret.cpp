/* Copyright (c) 2008-2012, Avian Contributors

   Permission to use, copy, modify, and/or distribute this software
   for any purpose with or without fee is hereby granted, provided
   that the above copyright notice and this permission notice appear
   in all copies.

   There is NO WARRANTY for this software.  See license.txt for
   details. */

#include "avian/common.h"
#include <avian/vm/system/system.h>
#include "avian/constants.h"
#include "avian/machine.h"
#include "avian/processor.h"
#include "avian/process.h"
#include "avian/arch.h"

#include <avian/util/runtime-array.h>

using namespace vm;

namespace local {

const unsigned FrameBaseOffset = 0;
const unsigned FrameNextOffset = 1;
const unsigned FrameMethodOffset = 2;
const unsigned FrameIpOffset = 3;
const unsigned FrameFootprint = 4;

class Context: public Thread {
 public:
  class ReferenceFrame {
   public:
    ReferenceFrame(ReferenceFrame* next, unsigned sp):
      next(next),
      sp(sp)
    { }

    ReferenceFrame* next;
    unsigned sp;
  };

  Context(Machine* m, object javaThread, Thread* parent):
    Thread(m, javaThread, parent),
    ip(0),
    sp(0),
    frame(-1),
    code(0),
    referenceFrame(0)
  { }

  unsigned ip;
  unsigned sp;
  int frame;
  int base;
  object code;
  ReferenceFrame* referenceFrame;
  uintptr_t stack[0];
};

typedef object Reference;
typedef int32_t Integer;
typedef int64_t Long;
typedef float Float;
typedef double Double;

void
pushReference(Context* t, object o)
{
  if (DebugStack) {
    fprintf(stderr, "push object %p at %d\n", o, t->sp);
  }

  assert(t, t->sp < stackSizeInWords(t));
  t->stack[t->sp ++] = reinterpret_cast<uintptr_t>(o);
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

  assert(t, t->sp + (8 / BytesPerWord) <= stackSizeInWords(t));
  memcpy(t->stack + t->sp, &v, 8);
  t->sp += 8 / BytesPerWord;
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
  assert(t, t->sp >= 8 / BytesPerWord);

  t->sp -= 8 / BytesPerWord;

  uint64_t v; memcpy(&v, t->stack + t->sp);

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

object
peekObject(Context* t, unsigned index)
{
  assert(t, index < t->sp);

  if (DebugStack) {
    fprintf(stderr, "peek object %p at %d\n",
            reinterpret_cast<object>(t->stack[index]),
            index);
  }

  return reinterpret_cast<object>(t->stack[index]);
}

object
peekReference(Context* t, unsigned offset)
{
  return peekObject(t, t->sp - 1 - offset);
}

uint32_t
peekInt(Context* t, unsigned index)
{
  assert(t, index < t->sp);

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
  assert(t, index <= t->sp - (8 / BytesPerWord));

  uint64_t v; memcpy(&v, t->stack + index, 8);

  if (DebugStack) {
    fprintf(stderr, "peek long %" LLD " at %d\n", v, index);
  }
  
  return v;
}

void
pokeObject(Context* t, unsigned index, object value)
{
  assert(t, index < t->sp);

  if (DebugStack) {
    fprintf(stderr, "poke object %p at %d\n", value, index);
  }

  t->stack[index] = reinterpret_cast<uintptr_t>(value);
}

void
pokeInt(Context* t, unsigned index, uint32_t value)
{
  assert(t, index < t->sp);

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

  assert(t, index <= t->sp - (8 / BytesPerWord));

  memcpy(t->stack + index, &v, 8);
}

object*
pushLocalReference(Context* t, object o)
{
  if (o) {
    pushObject(t, o);
    return reinterpret_cast<object*>(t->stack + (t->sp - 1));
  } else {
    return 0;
  }
}

int
frameNext(Context* t, int frame)
{
  return peekInt(t, frame + FrameNextOffset);
}

object
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
localObject(Context* t, unsigned index)
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

void
setLocalObject(Context* t, unsigned index, object value)
{
  pokeObject(t, frameBase(t, t->frame) + index, value);
}

void
setLocalInt(Context* t, unsigned index, uint32_t value)
{
  pokeInt(t, frameBase(t, t->frame) + index, value);
}

void
setLocalLong(Context* t, unsigned index, uint64_t value)
{
  pokeLong(t, frameBase(t, t->frame) + index, value);
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

    memset(t->stack + ((base + parameterFootprint) * 2), 0,
           (locals - parameterFootprint) * BytesPerWord * 2);
  }

  unsigned frame = base + locals;
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
  object method = frameMethod(t, t->frame);

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
      pushObject(t, static_cast<uintptr_t>(result) == 0 ? 0 :
                 *reinterpret_cast<object*>(static_cast<uintptr_t>(result)));
    } else {
      if (DebugRun) {
        fprintf(stderr, "result: %p\n", reinterpret_cast<object>(result));
      }
      pushObject(t, reinterpret_cast<object>(result));
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
        object* v = reinterpret_cast<object*>(t->stack + ((sp++) * 2) + 1);
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

    result = t->m->system->call
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
store(Context* t, unsigned index)
{
  memcpy(t->stack + ((frameBase(t, t->frame) + index) * 2),
         t->stack + ((-- t->sp) * 2),
         BytesPerWord * 2);
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

#include "bytecode.cpp"

object
interpret3(Context* t, const int base)
{
  t->base = base;

  unsigned returnCode = methodReturnCode(t, t->frame);

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
    pushObject(t, this_);
  }

  for (MethodSpecIterator it(t, spec); it.hasNext();) {
    switch (*it.next()) {
    case 'L':
    case '[':
      if (indirectObjects) {
        object* v = va_arg(a, object*);
        pushObject(t, v ? *v : 0);
      } else {
        pushObject(t, va_arg(a, object));
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
    pushObject(t, this_);
  }

  unsigned index = 0;
  for (MethodSpecIterator it(t, spec); it.hasNext();) {
    switch (*it.next()) {
    case 'L':
    case '[': {
      jobject v = arguments[index++].l;
      pushObject(t, v ? *v : 0);
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
    pushObject(t, this_);
  }

  unsigned index = 0;
  for (MethodSpecIterator it(t, spec); it.hasNext();) {
    switch (*it.next()) {
    case 'L':
    case '[':
      pushObject(t, objectArrayBody(t, a, index++));
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
      result = popObject(t);
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

    if (LIKELY(t->exception == 0)) {
      popFrame(t);
    } else {
      object exception = t->exception;
      t->exception = 0;
      throw_(t, exception);
    }
  }

  return result;
}

class MyProcessor: public Processor {
 public:
  MyProcessor(System* s, Allocator* allocator):
    s(s), allocator(allocator)
  { }

  virtual Thread*
  makeThread(Machine* m, object javaThread, Thread* parent)
  {
    Context* t = new (m->heap->allocate(sizeof(Thread) + m->stackSizeInBytes))
      Thread(m, javaThread, parent);
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
    Context* t = static_cast<Thread*>(vmt);

    v->visit(&(t->code));

    class MyStackVisitor: public StackVisitor {
     public:
      MyStackVisitor(Context* t) t(t) { }

      virtual bool visit(StackWalker* vmw) {
        MyStackWalker* walker = static_cast<MyStackWalker*>(vmw);

        class MyTypeVisitor: public TypeVisitor {
         public:
          MyTypeVisitor(MyStackWalker* walker):
          t(walker->t),
          walker(walker),
          parameterFootprint
          (codeParameterFootprint(t, methodCode(t, walker->method()))),
          index(walker->frame - parameterFootprint)
          { }

          virtual void visit(object type) {
            if (isReferenceType(t, type)) {
              v->visit(t->stack + index);
            }
            
            index += stackFootprint(t, type);

            if (index == parameterFootprint) {
              index += FrameFootprint;
            }
          }

          Context* t;
          MyStackWalker* walker;
          unsigned parameterFootprint;
          unsigned index;
        } tv(walker);

        object code = methodCode(t, walker->method());
        unsigned mapSize = ceilingDivide
          (codeMaxStack(t, code) + codeMaxLocals(t, code), 32);

        THREAD_RUNTIME_ARRAY(t, uint32_t, map, mapSize);

        getFrameMap
          (t, walker->method(), walker->ip(), RUNTIME_ARRAY_BODY(map));
      }

      Context* t;
    } sv(t);

    walkStack(t, &sv);
  }

  virtual void
  walkStack(Thread* vmt, StackVisitor* v)
  {
    Context* t = static_cast<Thread*>(vmt);

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
      t->referenceFrame = new
        (t->m->heap->allocate(sizeof(Thread::ReferenceFrame)))
        Thread::ReferenceFrame(t->referenceFrame, t->sp);
    
      return true;
    } else {
      return false;
    }
  }

  virtual void
  popLocalFrame(Thread* vmt)
  {
    Context* t = static_cast<Context*>(vmt);

    Thread::ReferenceFrame* f = t->referenceFrame;
    t->referenceFrame = f->next;
    t->sp = f->sp;

    t->m->heap->free(f, sizeof(Thread::ReferenceFrame));
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

  virtual void initialize(BootImage*, uint8_t*, unsigned) {
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
  }
  
  System* s;
  Allocator* allocator;
};

} // namespace

namespace vm {

Processor*
makeProcessor(System* system, Allocator* allocator, bool)
{
  return new (allocator->allocate(sizeof(local::MyProcessor)))
    local::MyProcessor(system, allocator);
}

} // namespace vm
