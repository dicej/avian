     	AREA text, CODE, ALIGN=2

		EXPORT vmNativeCall
vmNativeCall
  
		;  arguments:
		;  r0 -> r4       : function
		;  r1 -> r5       : stackTotal
		;  r2             : memoryTable
		;  r3             : memoryCount
		;  [sp, #0] -> r6 : gprTable
  
		mov   ip, sp           ; save stack frame
		stmfd sp!, {r4-r6, lr} ; save clobbered non-volatile regs

		; mv args into non-volatile regs
		mov   r4, r0
		mov   r5, r1
		ldr   r6, [ip]

		; setup stack arguments if necessary
		sub   sp, sp, r5 ; allocate stack
		mov   ip, sp
loop
		tst   r3, r3
		ldrne r0, [r2], #4
		strne r0, [ip], #4
		subne r3, r3, #4
		bne   loop

		; setup argument registers if necessary
		tst     r6, r6
		ldmneia r6, {r0-r3}

		blx   r4         ; call function
		add   sp, sp, r5 ; deallocate stack

		ldmfd sp!, {r4-r6, pc} ; restore non-volatile regs and return

		EXPORT vmJump
vmJump
		mov   lr, r0
		ldr   r0, [sp]
		ldr   r1, [sp, #4]
		mov   sp, r2
		mov   r8, r3
		bx    lr

CHECKPOINT_THREAD EQU 4
CHECKPOINT_STACK EQU 24

		EXPORT vmRun1
vmRun1
		; r0: function
		; r1: arguments
		; r2: checkpoint
		stmfd sp!, {r4-r11, lr}
		; align stack
		sub   sp, sp, #12
   
		str   sp, [r2, #CHECKPOINT_STACK]

		mov   r12, r0
		ldr   r0, [r2, #CHECKPOINT_THREAD]

		blx   r12

		EXPORT vmRun_returnAddress
vmRun_returnAddress
		add   sp, sp, #12
		ldmfd sp!, {r4-r11, lr}
		bx    lr

		EXPORT vmTrap		
vmTrap
		bkpt 3
		
		END