/* Copyright (c) 2013, Avian Contributors

   Permission to use, copy, modify, and/or distribute this software
   for any purpose with or without fee is hereby granted, provided
   that the above copyright notice and this permission notice appear
   in all copies.

   There is NO WARRANTY for this software.  See license.txt for
   details. */

void
parseBytecode(Context* c)
{
  Thread* t = contextThread(c);
  unsigned& ip = contextIp(c);
  object code = 0;
  PROTECT(t, code);

  unsigned instruction;

 check: {
    object method = contextMethod(c);
    if (method == 0) {
      return;
    }

    code = methodCode(t, method);
  }

 loop:
  if (not visitInstruction(c)) {
    goto check;
  }

  instruction = codeBody(t, code, ip++);

  switch (instruction) {
  case aaload: {
    Integer index = popInt(c);
    Reference array = popReference(c);

    pushReference(c, referenceArrayLoad(c, array, index));
  } goto loop;

  case aastore: {
    Reference value = popReference(c);
    Integer index = popInt(c);
    Reference array = popReference(c);

    referenceArrayStore(c, array, index, value);
  } goto loop;

  case aconst_null: {
    pushReference(c, 0);
  } goto loop;

  case aload: {
    pushReference(c, localReference(c, codeBody(t, code, ip++)));
  } goto loop;

  case aload_0: {
    pushReference(c, localReference(c, 0));
  } goto loop;

  case aload_1: {
    pushReference(c, localReference(c, 1));
  } goto loop;

  case aload_2: {
    pushReference(c, localReference(c, 2));
  } goto loop;

  case aload_3: {
    pushReference(c, localReference(c, 3));
  } goto loop;

  case anewarray: {
    pushReference
      (c, makeObjectArray
       (c, resolveClassInPool
        (t, contextMethod(c), codeReadInt16(t, code, ip) - 1,
         contextResolveStrategy(c)), popInt(c)));
  } goto loop;

  case areturn: {
    Reference result = popReference(c);
    popFrame(c);
    pushReference(c, result);
  } goto check;

  case arraylength: {
    pushInt(c, loadWord(c, popReference(c), BytesPerWord));
  } goto loop;

  case astore: {
    setLocalReference(c, codeBody(t, code, ip++), popReference(c));
  } goto loop;

  case astore_0: {
    setLocalReference(c, 0, popReference(c));
  } goto loop;

  case astore_1: {
    setLocalReference(c, 1, popReference(c));
  } goto loop;

  case astore_2: {
    setLocalReference(c, 2, popReference(c));
  } goto loop;

  case astore_3: {
    setLocalReference(c, 3, popReference(c));
  } goto loop;

  case athrow: {
    throwException(c, popReference(c));
  } goto check;

  case baload: {
    Integer index = popInt(c);
    Reference array = popReference(c);

    pushInt(c, byteArrayLoad(c, array, index));
  } goto loop;

  case bastore: {
    Integer value = popInt(c);
    Integer index = popInt(c);
    Reference array = popReference(c);

    byteArrayStore(c, array, index, value);
  } goto loop;

  case bipush: {
    pushInt(c, intConstant(c, static_cast<int8_t>(codeBody(t, code, ip++))));
  } goto loop;

  case caload: {
    Integer index = popInt(c);
    Reference array = popReference(c);

    pushInt(c, charArrayLoad(c, array, index));
  } goto loop;

  case castore: {
    Integer value = popInt(c);
    Integer index = popInt(c);
    Reference array = popReference(c);

    charArrayStore(c, array, index, value);
  } goto loop;

  case checkcast: {
    checkCast
      (c, resolveClassInPool
       (t, contextMethod(c), codeReadInt16(t, code, ip) - 1,
        contextResolveStrategy(c)), peekReference(c));
  } goto loop;

  case d2f: {
    pushFloat(c, doubleToFloat(c, popDouble(c)));
  } goto loop;

  case d2i: {
    pushInt(c, doubleToInt(c, popDouble(c)));
  } goto loop;

  case d2l: {
    pushLong(c, doubleToLong(c, popDouble(c)));
  } goto loop;

  case dadd: {
    Double b = popDouble(c);
    Double a = popDouble(c);
    
    pushDouble(c, doubleAdd(c, a, b));
  } goto loop;

  case daload: {
    Integer index = popInt(c);
    Reference array = popReference(c);

    pushDouble(c, doubleArrayLoad(c, array, index));
  } goto loop;

  case dastore: {
    Double value = popInt(c);
    Integer index = popInt(c);
    Reference array = popReference(c);

    doubleArrayStore(c, array, index, value);
  } goto loop;

  case dcmpg: {
    Double b = popDouble(c);
    Double a = popDouble(c);
    
    pushInt(c, doubleCompareGreater(c, a, b));
  } goto loop;

  case dcmpl: {
    Double b = popDouble(c);
    Double a = popDouble(c);
    
    pushInt(c, doubleCompareLess(c, a, b));
  } goto loop;

  case dconst_0: {
    pushDouble(c, doubleConstant(c, 0));
  } goto loop;

  case dconst_1: {
    pushDouble(c, doubleConstant(c, 1));
  } goto loop;

  case ddiv: {
    Double b = popDouble(c);
    Double a = popDouble(c);
    
    pushDouble(c, doubleDivide(c, a, b));
  } goto loop;

  case dmul: {
    Double b = popDouble(c);
    Double a = popDouble(c);
    
    pushDouble(c, doubleMultiply(c, a, b));
  } goto loop;

  case dneg: {
    pushDouble(c, doubleNegate(c, popDouble(c)));
  } goto loop;

  case vm::drem: {
    Double b = popDouble(c);
    Double a = popDouble(c);
    
    pushDouble(c, doubleRemainder(c, a, b));
  } goto loop;

  case dsub: {
    Double b = popDouble(c);
    Double a = popDouble(c);
    
    pushDouble(c, doubleSubtract(c, a, b));
  } goto loop;

  case dup:
  case dup_x1:
  case dup_x2:
  case dup2:
  case dup2_x1:
  case dup2_x2: {
    duplicate(c, instruction);
  } goto loop;

  case f2d: {
    pushDouble(c, floatToDouble(c, popFloat(c)));
  } goto loop;

  case f2i: {
    pushInt(c, floatToInt(c, popFloat(c)));
  } goto loop;

  case f2l: {
    pushLong(c, floatToLong(c, popFloat(c)));
  } goto loop;

  case fadd: {
    Float b = popFloat(c);
    Float a = popFloat(c);
    
    pushFloat(c, floatAdd(c, a, b));
  } goto loop;

  case faload: {
    Integer index = popInt(c);
    Reference array = popReference(c);

    pushFloat(c, floatArrayLoad(c, array, index));
  } goto loop;

  case fastore: {
    Float value = popFloat(c);
    Integer index = popInt(c);
    Reference array = popReference(c);

    floatArrayStore(c, array, index, value);
  } goto loop;

  case fcmpg: {
    Float b = popFloat(c);
    Float a = popFloat(c);
    
    pushInt(c, floatCompareGreater(c, a, b));
  } goto loop;

  case fcmpl: {
    Float b = popFloat(c);
    Float a = popFloat(c);
    
    pushInt(c, floatCompareLess(c, a, b));
  } goto loop;

  case fconst_0: {
    pushFloat(c, floatConstant(c, 0));
  } goto loop;

  case fconst_1: {
    pushFloat(c, floatConstant(c, 1));
  } goto loop;

  case fconst_2: {
    pushFloat(c, floatConstant(c, 2));
  } goto loop;

  case fdiv: {
    Float b = popFloat(c);
    Float a = popFloat(c);
    
    pushFloat(c, floatDivide(c, a, b));
  } goto loop;

  case fmul: {
    Float b = popFloat(c);
    Float a = popFloat(c);
    
    pushFloat(c, floatMultiply(c, a, b));
  } goto loop;

  case fneg: {
    pushFloat(c, floatNegate(c, popFloat(c)));
  } goto loop;

  case frem: {
    Float b = popFloat(c);
    Float a = popFloat(c);
    
    pushFloat(c, floatRemainder(c, a, b));
  } goto loop;

  case fsub: {
    Float b = popFloat(c);
    Float a = popFloat(c);
    
    pushFloat(c, floatSubtract(c, a, b));
  } goto loop;

  case getfield:
  case getstatic: {
    ResolveStrategy strategy = contextResolveStrategy(c);

    object field = resolveField
      (t, contextMethod(c), codeReadInt16(t, code, ip) - 1, strategy);

    PROTECT(t, field);

    unsigned offset;
    unsigned code;
    Reference target;
    
    if (strategy != NoResolve
        or objectClass(t, field) != type(t, Machine::ReferenceType))
    {
      assert(t, (fieldFlags(t, field) & ACC_STATIC)
             xor (instruction == getfield));

      offset = fieldOffset(t, field);
      code = fieldCode(t, field);
      target = instruction == getfield ? nullCheck(c, popReference(c))
        : referenceConstant(c, classStaticTable(t, fieldClass(t, field)));
    } else {
      offset = 0x7FFFFFFF;
      code = fieldCode(t, byteArrayBody(t, referenceSpec(t, field), 0));
      target = instruction == getfield ? popReference(c)
        : referenceConstant(c, 0);
    }

    CONTEXT_ACQUIRE_FIELD_FOR_READ(c, field);

    switch (code) {
    case ByteField:
    case BooleanField:
      pushInt(c, loadByte(c, target, offset));
      break;

    case CharField:
      pushInt(c, loadChar(c, target, offset));
      break;

    case ShortField:
      pushInt(c, loadShort(c, target, offset));
      break;

    case FloatField:
      pushFloat(c, loadFloat(c, target, offset));
      break;

    case IntField:
      pushInt(c, loadInt(c, target, offset));
      break;

    case DoubleField:
      pushDouble(c, loadDouble(c, target, offset));
      break;

    case LongField:
      pushLong(c, loadLong(c, target, offset));
      break;

    case ObjectField:
      pushReference(c, loadReference(c, target, offset));
      break;

    default:
      abort(t);
    }
  } goto loop;

  case goto_: {
    int16_t offset = codeReadInt16(t, code, ip);
    jump(c, (ip - 3) + offset);
  } goto loop;
    
  case goto_w: {
    int32_t offset = codeReadInt32(t, code, ip);
    jump(c, (ip - 5) + offset);
  } goto loop;

  case i2b: {
    pushInt(c, intToByte(c, popInt(c)));
  } goto loop;

  case i2c: {
    pushInt(c, intToChar(c, popInt(c)));
  } goto loop;

  case i2d:{ 
    pushDouble(c, intToDouble(c, popInt(c)));
  } goto loop;

  case i2f: {
    pushFloat(c, intToFloat(c, popInt(c)));
  } goto loop;

  case i2l: {
    pushLong(c, intToLong(c, popInt(c)));
  } goto loop;

  case i2s: {
    pushInt(c, intToShort(c, popInt(c)));
  } goto loop;

  case iadd: {
    Integer b = popInt(c);
    Integer a = popInt(c);
    
    pushInt(c, intAdd(c, a, b));
  } goto loop;

  case iaload: {
    Integer index = popInt(c);
    Reference array = popReference(c);

    pushInt(c, intArrayLoad(c, array, index));
  } goto loop;

  case iand: {
    Integer b = popInt(c);
    Integer a = popInt(c);
    
    pushInt(c, intAnd(c, a, b));
  } goto loop;

  case iastore: {
    Integer value = popInt(c);
    Integer index = popInt(c);
    Reference array = popReference(c);

    intArrayStore(c, array, index, value);
  } goto loop;

  case iconst_m1: {
    pushInt(c, intConstant(c, -1));
  } goto loop;

  case iconst_0: {
    pushInt(c, intConstant(c, 0));
  } goto loop;

  case iconst_1: {
    pushInt(c, intConstant(c, 1));
  } goto loop;

  case iconst_2: {
    pushInt(c, intConstant(c, 2));
  } goto loop;

  case iconst_3: {
    pushInt(c, intConstant(c, 3));
  } goto loop;

  case iconst_4: {
    pushInt(c, intConstant(c, 4));
  } goto loop;

  case iconst_5: {
    pushInt(c, intConstant(c, 5));
  } goto loop;

  case idiv: {
    Integer b = popInt(c);
    Integer a = popInt(c);
    
    pushInt(c, intDivide(c, a, b));
  } goto loop;

  case if_acmpeq: {
    int16_t offset = codeReadInt16(t, code, ip);

    Reference b = popReference(c);
    Reference a = popReference(c);
    
    branch(c, referenceEqual(c, a, b), (ip - 3) + offset, ip);
  } goto loop;

  case if_acmpne: {
    int16_t offset = codeReadInt16(t, code, ip);

    Reference b = popReference(c);
    Reference a = popReference(c);
    
    branch(c, referenceEqual(c, a, b), ip, (ip - 3) + offset);
  } goto loop;

  case if_icmpeq: {
    int16_t offset = codeReadInt16(t, code, ip);

    Integer b = popInt(c);
    Integer a = popInt(c);
    
    branch(c, intEqual(c, a, b), (ip - 3) + offset, ip);
  } goto loop;

  case if_icmpne: {
    int16_t offset = codeReadInt16(t, code, ip);

    Integer b = popInt(c);
    Integer a = popInt(c);
    
    branch(c, intEqual(c, a, b), ip, (ip - 3) + offset);
  } goto loop;

  case if_icmpgt: {
    int16_t offset = codeReadInt16(t, code, ip);

    Integer b = popInt(c);
    Integer a = popInt(c);
    
    branch(c, intGreater(c, a, b), (ip - 3) + offset, ip);
  } goto loop;

  case if_icmpge: {
    int16_t offset = codeReadInt16(t, code, ip);

    Integer b = popInt(c);
    Integer a = popInt(c);
    
    branch(c, intLess(c, a, b), ip, (ip - 3) + offset);
  } goto loop;

  case if_icmplt: {
    int16_t offset = codeReadInt16(t, code, ip);

    Integer b = popInt(c);
    Integer a = popInt(c);
    
    branch(c, intLess(c, a, b), (ip - 3) + offset, ip);
  } goto loop;

  case if_icmple: {
    int16_t offset = codeReadInt16(t, code, ip);

    Integer b = popInt(c);
    Integer a = popInt(c);
    
    branch(c, intGreater(c, a, b), ip, (ip - 3) + offset);
  } goto loop;

  case ifeq: {
    int16_t offset = codeReadInt16(t, code, ip);

    branch(c, popInt(c), ip, (ip - 3) + offset);
  } goto loop;

  case ifne: {
    int16_t offset = codeReadInt16(t, code, ip);

    branch(c, popInt(c), (ip - 3) + offset, ip);
  } goto loop;

  case ifgt: {
    int16_t offset = codeReadInt16(t, code, ip);
    
    branch(c, intGreater(c, popInt(c), intConstant(c, 0)), (ip - 3) + offset,
           ip);
  } goto loop;

  case ifge: {
    int16_t offset = codeReadInt16(t, code, ip);
    
    branch(c, intLess(c, popInt(c), intConstant(c, 0)), ip, (ip - 3) + offset);
  } goto loop;

  case iflt: {
    int16_t offset = codeReadInt16(t, code, ip);
    
    branch(c, intLess(c, popInt(c), intConstant(c, 0)), (ip - 3) + offset, ip);
  } goto loop;

  case ifle: {
    int16_t offset = codeReadInt16(t, code, ip);
    
    branch(c, intGreater(c, popInt(c), intConstant(c, 0)), ip,
           (ip - 3) + offset);
  } goto loop;

  case ifnonnull: {
    int16_t offset = codeReadInt16(t, code, ip);

    branch(c, notNull(c, popReference(c)), (ip - 3) + offset, ip);
  } goto loop;

  case ifnull: {
    int16_t offset = codeReadInt16(t, code, ip);

    branch(c, notNull(c, popReference(c)), ip, (ip - 3) + offset);
  } goto loop;

  case iinc: {
    uint8_t index = codeBody(t, code, ip++);
    int8_t v = codeBody(t, code, ip++);
    
    setLocalInt(c, index, intAdd(c, localInt(c, index), intConstant(c, v)));
  } goto loop;

  case iload: {
    pushInt(c, localInt(c, codeBody(t, code, ip++)));
  } goto loop;

  case fload: {
    pushFloat(c, localFloat(c, codeBody(t, code, ip++)));
  } goto loop;

  case iload_0: {
    pushInt(c, localInt(c, 0));
  } goto loop;

  case fload_0: {
    pushFloat(c, localFloat(c, 0));
  } goto loop;

  case iload_1: {
    pushInt(c, localInt(c, 1));
  } goto loop;

  case fload_1: {
    pushFloat(c, localFloat(c, 1));
  } goto loop;

  case iload_2: {
    pushInt(c, localInt(c, 2));
  } goto loop;

  case fload_2: {
    pushFloat(c, localFloat(c, 2));
  } goto loop;

  case iload_3: {
    pushInt(c, localInt(c, 3));
  } goto loop;

  case fload_3: {
    pushFloat(c, localFloat(c, 3));
  } goto loop;

  case imul: {
    Integer b = popInt(c);
    Integer a = popInt(c);
    
    pushInt(c, intMultiply(c, a, b));
  } goto loop;

  case ineg: {
    pushInt(c, intNegate(c, popInt(c)));
  } goto loop;

  case instanceof: {
    pushInt
      (c, instanceOf
       (c, resolveClassInPool
        (t, contextMethod(c), codeReadInt16(t, code, ip) - 1,
         contextResolveStrategy(c)), popReference(c)));
  } goto loop;

  case invokeinterface: {
    uint16_t index = codeReadInt16(t, code, ip);
    
    ip += 2;

    invokeInterface
      (c, resolveMethod
       (t, contextMethod(c), index - 1, contextResolveStrategy(c)));
  } goto check;

  case invokespecial: {
    ResolveStrategy strategy = contextResolveStrategy(c);

    object method = resolveMethod
      (t, contextMethod(c), codeReadInt16(t, code, ip) - 1, strategy);

    if (strategy != NoResolve
        or objectClass(t, method) != type(t, Machine::ReferenceType))
    {
      object class_ = methodClass(t, method);
      if (isSpecialMethod(t, method, class_)) {
        class_ = classSuper(t, class_);
        PROTECT(t, method);
        PROTECT(t, class_);

        initClass(t, class_);

        method = findVirtualMethod(t, method, class_);
      }
    }

    invokeDirect(c, method, false);
  } goto check;

  case invokestatic: {    
    ResolveStrategy strategy = contextResolveStrategy(c);

    object method = resolveMethod
      (t, contextMethod(c), codeReadInt16(t, code, ip) - 1, strategy);

    if (strategy != NoResolve
        or objectClass(t, method) != type(t, Machine::ReferenceType))
    {
      PROTECT(t, method);
    
      initClass(t, methodClass(t, method));
    }

    invokeDirect(c, method, true);
  } goto check;

  case invokevirtual: {
    invokeVirtual(c, resolveMethod
                  (t, contextMethod(c), codeReadInt16(t, code, ip) - 1,
                   contextResolveStrategy(c)));
  } goto check;

  case ior: {
    Integer b = popInt(c);
    Integer a = popInt(c);
    
    pushInt(c, intOr(c, a, b));
  } goto loop;

  case irem: {
    Integer b = popInt(c);
    Integer a = popInt(c);
    
    pushInt(c, intRemainder(c, a, b));
  } goto loop;

  case ireturn: {
    Integer result = popInt(c);
    popFrame(c);
    pushInt(c, result);
  } goto check;

  case freturn: {
    Float result = popFloat(c);
    popFrame(c);
    pushFloat(c, result);
  } goto check;

  case ishl: {
    Integer b = popInt(c);
    Integer a = popInt(c);
    
    pushInt(c, intShiftLeft(c, a, b));
  } goto loop;

  case ishr: {
    Integer b = popInt(c);
    Integer a = popInt(c);
    
    pushInt(c, intShiftRight(c, a, b));
  } goto loop;

  case istore: {
    setLocalInt(c, codeBody(t, code, ip++), popInt(c));
  } goto loop;

  case fstore: {
    setLocalFloat(c, codeBody(t, code, ip++), popFloat(c));
  } goto loop;

  case istore_0: {
    setLocalInt(c, 0, popInt(c));
  } goto loop;

  case fstore_0: {
    setLocalFloat(c, 0, popFloat(c));
  } goto loop;

  case istore_1: {
    setLocalInt(c, 1, popInt(c));
  } goto loop;

  case fstore_1: {
    setLocalFloat(c, 1, popFloat(c));
  } goto loop;

  case istore_2: {
    setLocalInt(c, 2, popInt(c));
  } goto loop;

  case fstore_2: {
    setLocalFloat(c, 2, popFloat(c));
  } goto loop;

  case istore_3: {
    setLocalInt(c, 3, popInt(c));
  } goto loop;

  case fstore_3: {
    setLocalFloat(c, 3, popFloat(c));
  } goto loop;

  case isub: {
    Integer b = popInt(c);
    Integer a = popInt(c);
    
    pushInt(c, intSubtract(c, a, b));
  } goto loop;

  case iushr: {
    Integer b = popInt(c);
    Integer a = popInt(c);
    
    pushInt(c, intUnsignedShiftRight(c, a, b));
  } goto loop;

  case ixor: {
    Integer b = popInt(c);
    Integer a = popInt(c);
    
    pushInt(c, intXor(c, a, b));
  } goto loop;

  case jsr: {
    uint16_t offset = codeReadInt16(t, code, ip);

    pushInt(c, intConstant(c, ip));
    jumpToSubroutine(c, (ip - 3) + static_cast<int16_t>(offset));
  } goto loop;

  case jsr_w: {
    uint32_t offset = codeReadInt32(t, code, ip);

    pushInt(c, intConstant(c, ip));
    jumpToSubroutine(c, (ip - 5) + static_cast<int16_t>(offset));
  } goto loop;

  case l2d: {
    pushDouble(c, longToDouble(c, popLong(c)));
  } goto loop;

  case l2f: {
    pushFloat(c, longToFloat(c, popLong(c)));
  } goto loop;

  case l2i: {
    pushInt(c, longToInt(c, popLong(c)));
  } goto loop;

  case ladd: {
    Long b = popLong(c);
    Long a = popLong(c);
    
    pushLong(c, longAdd(c, a, b));
  } goto loop;

  case laload: {
    Integer index = popInt(c);
    Reference array = popReference(c);

    pushLong(c, longArrayLoad(c, array, index));
  } goto loop;

  case land: {
    Long b = popLong(c);
    Long a = popLong(c);
    
    pushLong(c, longAnd(c, a, b));
  } goto loop;

  case lastore: {
    Long value = popLong(c);
    Integer index = popInt(c);
    Reference array = popReference(c);

    longArrayStore(c, array, index, value);
  } goto loop;

  case lcmp: {
    Long b = popLong(c);
    Long a = popLong(c);
    
    pushInt(c, longCompare(c, a, b));
  } goto loop;

  case lconst_0: {
    pushLong(c, longConstant(c, 0));
  } goto loop;

  case lconst_1: {
    pushLong(c, longConstant(c, 1));
  } goto loop;

  case ldc:
  case ldc_w: {
    uint16_t index;

    if (instruction == ldc) {
      index = codeBody(t, code, ip++);
    } else {
      index = codeReadInt16(t, code, ip);
    }

    object pool = codePool(t, code);

    if (singletonIsObject(t, pool, index - 1)) {
      object v = singletonObject(t, pool, index - 1);

      loadMemoryBarrier();

      if (objectClass(t, v) == type(t, Machine::ReferenceType)) {
        pushReference
          (c, referenceConstant
           (c, getJClass
            (t, resolveClassInPool
             (t, contextMethod(c), index - 1,
              contextResolveStrategy(c)))));
      } else if (objectClass(t, v) == type(t, Machine::ClassType)) {
        pushReference(c, referenceConstant(c, getJClass(t, v)));
      } else {     
        pushReference(c, referenceConstant(c, v));
      }
    } else {
      pushInt(c, intConstant(c, singletonValue(t, pool, index - 1)));
    }
  } goto loop;

  case ldc2_w: {
    uint16_t index = codeReadInt16(t, code, ip);

    uint64_t v;
    memcpy(&v, &singletonValue(t, codePool(t, code), index - 1), 8);
    pushLong(c, longConstant(c, v));
  } goto loop;

  case ldiv_: {
    Long b = popLong(c);
    Long a = popLong(c);
    
    pushLong(c, longDivide(c, a, b));
  } goto loop;

  case lload: {
    pushLong(c, localLong(c, codeBody(t, code, ip++)));
  } goto loop;

  case dload: {
    pushDouble(c, localDouble(c, codeBody(t, code, ip++)));
  } goto loop;

  case lload_0: {
    pushLong(c, localLong(c, 0));
  } goto loop;

  case dload_0: {
    pushDouble(c, localDouble(c, 0));
  } goto loop;

  case lload_1: {
    pushLong(c, localLong(c, 1));
  } goto loop;

  case dload_1: {
    pushDouble(c, localDouble(c, 1));
  } goto loop;

  case lload_2: {
    pushLong(c, localLong(c, 2));
  } goto loop;

  case dload_2: {
    pushDouble(c, localDouble(c, 2));
  } goto loop;

  case lload_3: {
    pushLong(c, localLong(c, 3));
  } goto loop;

  case dload_3: {
    pushDouble(c, localDouble(c, 3));
  } goto loop;

  case lmul: {
    Long b = popLong(c);
    Long a = popLong(c);
    
    pushLong(c, longMultiply(c, a, b));
  } goto loop;

  case lneg: {
    pushLong(c, longNegate(c, popLong(c)));
  } goto loop;

  case lookupswitch: {
    int32_t base = ip - 1;

    ip += 3;
    ip -= (ip % 4);
    
    int32_t default_ = codeReadInt32(t, code, ip);
    int32_t pairCount = codeReadInt32(t, code, ip);
    
    lookupSwitch(c, popInt(c), code, base, default_, pairCount);
  } goto loop;

  case lor: {
    Long b = popLong(c);
    Long a = popLong(c);
    
    pushLong(c, longOr(c, a, b));
  } goto loop;

  case lrem: {
    Long b = popLong(c);
    Long a = popLong(c);
    
    pushLong(c, longRemainder(c, a, b));
  } goto loop;

  case lreturn: {
    Long result = popLong(c);
    popFrame(c);
    pushLong(c, result);
  } goto check;

  case dreturn: {
    Double result = popDouble(c);
    popFrame(c);
    pushDouble(c, result);
  } goto check;

  case lshl: {
    Integer b = popInt(c);
    Long a = popLong(c);
    
    pushLong(c, longShiftLeft(c, a, b));
  } goto loop;

  case lshr: {
    Integer b = popInt(c);
    Long a = popLong(c);
    
    pushLong(c, longShiftRight(c, a, b));
  } goto loop;

  case lstore: {
    setLocalLong(c, codeBody(t, code, ip++), popLong(c));
  } goto loop;

  case dstore: {
    setLocalDouble(c, codeBody(t, code, ip++), popDouble(c));
  } goto loop;

  case lstore_0: {
    setLocalLong(c, 0, popLong(c));
  } goto loop;

  case dstore_0: {
    setLocalDouble(c, 0, popDouble(c));
  } goto loop;

  case lstore_1: {
    setLocalLong(c, 1, popLong(c));
  } goto loop;

  case dstore_1: {
    setLocalDouble(c, 1, popDouble(c));
  } goto loop;

  case lstore_2: {
    setLocalLong(c, 2, popLong(c));
  } goto loop;

  case dstore_2: {
    setLocalDouble(c, 2, popDouble(c));
  } goto loop;

  case lstore_3: {
    setLocalLong(c, 3, popLong(c));
  } goto loop;

  case dstore_3: {
    setLocalDouble(c, 3, popDouble(c));
  } goto loop;

  case lsub: {
    Long b = popLong(c);
    Long a = popLong(c);
    
    pushLong(c, longSubtract(c, a, b));
  } goto loop;

  case lushr: {
    Integer b = popInt(c);
    Long a = popLong(c);
    
    pushLong(c, longUnsignedShiftRight(c, a, b));
  } goto loop;

  case lxor: {
    Long b = popLong(c);
    Long a = popLong(c);
    
    pushLong(c, longXor(c, a, b));
  } goto loop;

  case monitorenter: {
    acquire(c, popReference(c));
  } goto loop;

  case monitorexit: {
    release(c, popReference(c));
  } goto loop;

  case multianewarray: {
    uint16_t index = codeReadInt16(t, code, ip);
    uint8_t dimensions = codeBody(t, code, ip++);

    THREAD_RUNTIME_ARRAY(t, Integer, counts, dimensions);
    for (int i = dimensions - 1; i >= 0; --i) {
      RUNTIME_ARRAY_BODY(counts)[i] = popInt(c);
    }

    pushReference
      (c, makeMultiArray
       (c, RUNTIME_ARRAY_BODY(counts), dimensions, resolveClassInPool
        (t, contextMethod(c), index - 1, contextResolveStrategy(c))));
  } goto loop;

  case new_: {
    uint16_t index = codeReadInt16(t, code, ip);
    
    object class_ = resolveClassInPool
      (t, contextMethod(c), index - 1, contextResolveStrategy(c));

    PROTECT(t, class_);

    initClass(t, class_);

    pushReference(c, make(c, class_));
  } goto loop;

  case newarray: {
    pushReference(c, makeArray(c, codeBody(t, code, ip++), popInt(c)));
  } goto loop;

  case nop: goto loop;

  case pop_: {
    pop(c);
  } goto loop;

  case pop2: {
    contextPop2(c);
  } goto loop;

  case putfield:
  case putstatic: {    
    ResolveStrategy strategy = contextResolveStrategy(c);

    object field = resolveField
      (t, contextMethod(c), codeReadInt16(t, code, ip) - 1, strategy);

    PROTECT(t, field);

    unsigned offset;
    unsigned code;
    object table;
    
    if (strategy != NoResolve
        or objectClass(t, field) != type(t, Machine::ReferenceType))
    {
      assert(t, (fieldFlags(t, field) & ACC_STATIC)
             xor (instruction == putfield));

      offset = fieldOffset(t, field);
      code = fieldCode(t, field);
      table = classStaticTable(t, fieldClass(t, field));
    } else {
      offset = 0x7FFFFFFF;
      code = fieldCode(t, byteArrayBody(t, referenceSpec(t, field), 0));
      table = 0;
    }

    PROTECT(t, table);

    { CONTEXT_ACQUIRE_FIELD_FOR_WRITE(c, field);

      switch (code) {
      case ByteField:
      case BooleanField:
      case CharField:
      case ShortField:
      case IntField: {
        Integer value = popInt(c);
        Reference target = instruction == putfield
          ? nullCheck(c, popReference(c))
          : referenceConstant(c, table);
        
        switch (code) {
        case ByteField:
        case BooleanField:
          storeByte(c, target, offset, value);
          break;
            
        case CharField:
        case ShortField:
          storeShort(c, target, offset, value);
          break;
            
        case IntField:
          storeInt(c, target, offset, value);
          break;
        }
      } break;

      case FloatField: {
        Float value = popFloat(c);
        Reference target = instruction == putfield
          ? nullCheck(c, popReference(c))
          : referenceConstant(c, table);

        storeFloat(c, target, offset, value);
      } break;

      case DoubleField: {
        Double value = popDouble(c);
        Reference target = instruction == putfield
          ? nullCheck(c, popReference(c))
          : referenceConstant(c, table);

        storeDouble(c, target, offset, value);
      } break;

      case LongField: {
        Long value = popLong(c);
        Reference target = instruction == putfield
          ? nullCheck(c, popReference(c))
          : referenceConstant(c, table);

        storeLong(c, target, offset, value);
      } break;

      case ObjectField: {
        Reference value = popReference(c);
        Reference target = instruction == putfield
          ? nullCheck(c, popReference(c))
          : referenceConstant(c, table);

        storeReference(c, target, offset, value);
      } break;

      default: abort(t);
      }
    }
  } goto loop;

  case ret: {
    returnFromSubroutine(c, localInt(c, codeBody(t, code, ip)));
  } goto loop;

  case return_: {
    object method = contextMethod(c);
    if ((methodFlags(t, method) & ConstructorFlag)
        and (classVmFlags(t, methodClass(t, method)) & HasFinalMemberFlag))
    {
      storeStoreMemoryBarrier(c);
    }

    popFrame(c);
  } goto check;

  case saload: {
    Integer index = popInt(c);
    Reference array = popReference(c);

    pushInt(c, shortArrayLoad(c, array, index));
  } goto loop;

  case sastore: {
    Integer value = popInt(c);
    Integer index = popInt(c);
    Reference array = popReference(c);

    shortArrayStore(c, array, index, value);
  } goto loop;

  case sipush: {
    pushInt
      (c, intConstant(c, static_cast<int16_t>(codeReadInt16(t, code, ip))));
  } goto loop;

  case swap: {
    contextSwap(c);
  } goto loop;

  case tableswitch: {
    int32_t base = ip - 1;

    ip += 3;
    ip -= (ip % 4);
    
    int32_t default_ = codeReadInt32(t, code, ip);
    int32_t bottom = codeReadInt32(t, code, ip);
    int32_t top = codeReadInt32(t, code, ip);
    
    tableSwitch(c, popInt(c), code, base, default_, bottom, top);
  } goto loop;

  case wide: goto wide;

  case impdep1: {
    // this means we're invoking a virtual method on an instance of a
    // bootstrap class, so we need to load the real class to get the
    // real method and call it.

    popFrame(c);

    assert(t, codeBody(t, code, ip - 3) == invokevirtual);
    ip -= 2;

    resolveBootstrap
      (c, resolveMethod(t, contextMethod(c), codeReadInt16(t, code, ip) - 1,
                        contextResolveStrategy(c)));

    ip -= 3;
  } goto check;

  default: abort(t);
  }

 wide:
  switch (codeBody(t, code, ip++)) {
  case aload: {
    pushReference(c, localReference(c, codeReadInt16(t, code, ip)));
  } goto loop;

  case astore: {
    setLocalReference(c, codeReadInt16(t, code, ip), popReference(c));
  } goto loop;

  case iinc: {
    uint16_t index = codeReadInt16(t, code, ip);
    int16_t v = codeReadInt16(t, code, ip);
    
    setLocalInt(c, index, intAdd(c, localInt(c, index), intConstant(c, v)));
  } goto loop;

  case iload: {
    pushInt(c, localInt(c, codeReadInt16(t, code, ip)));
  } goto loop;

  case istore: {
    setLocalInt(c, codeReadInt16(t, code, ip), popInt(c));
  } goto loop;

  case lload: {
    pushLong(c, localLong(c, codeReadInt16(t, code, ip)));
  } goto loop;

  case lstore: {
    setLocalLong(c, codeReadInt16(t, code, ip),  popLong(c));
  } goto loop;

  case ret: {
    returnFromSubroutine(c, localInt(c, codeReadInt16(t, code, ip)));
  } goto loop;

  default: abort(t);
  }
}
