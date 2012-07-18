		; types.inc		
VOID_TYPE equ 0
INT8_TYPE equ 1
INT16_TYPE equ 2
INT32_TYPE equ 3
INT64_TYPE equ 4
FLOAT_TYPE equ 5
DOUBLE_TYPE equ 6
POINTER_TYPE equ 7
		
        ; target-fields.inc 
        ;TARGET_BYTES_PER_WORD = 4

TARGET_THREAD_EXCEPTION equ 44
TARGET_THREAD_EXCEPTIONSTACKADJUSTMENT equ 2164
TARGET_THREAD_EXCEPTIONOFFSET equ 2168
TARGET_THREAD_EXCEPTIONHANDLER equ 2172
		
TARGET_THREAD_IP equ 2144
TARGET_THREAD_STACK equ 2148
TARGET_THREAD_NEWSTACK equ 2152
TARGET_THREAD_SCRATCH equ 2156
TARGET_THREAD_CONTINUATION equ 2160
TARGET_THREAD_TAILADDRESS equ 2176
TARGET_THREAD_VIRTUALCALLTARGET equ 2180
TARGET_THREAD_VIRTUALCALLINDEX equ 2184
TARGET_THREAD_HEAPIMAGE equ 2188
TARGET_THREAD_CODEIMAGE equ 2192
TARGET_THREAD_THUNKTABLE equ 2196
TARGET_THREAD_STACKLIMIT equ 2220

		AREA text, CODE, ALIGN=2

BYTES_PER_WORD equ 4

CONTINUATION_NEXT equ 4
CONTINUATION_ADDRESS equ 16
CONTINUATION_RETURN_ADDRESS_OFFSET equ 20
CONTINUATION_FRAME_POINTER_OFFSET equ 24
CONTINUATION_LENGTH equ 28
CONTINUATION_BODY equ 32
   
		EXPORT vmInvoke
vmInvoke
  
		;  arguments
		;  r0       : thread
		;  r1       : function
		;  r2       : arguments
		;  r3       : argumentFootprint
		;  [sp, #0] : frameSize (not used)
		;  [sp, #4] : returnType
  

		; save all non-volatile registers
		stmfd sp!, {r4-r11, lr}

		; save return type
		ldr   r4, [sp, #4]
		str   r4, [sp, #-4]!

		str   sp, [r0, #TARGET_THREAD_SCRATCH]

		; align stack, if necessary
		eor   r4, sp, r3
		tst   r4, #4
		subne sp, sp, #4
	   
		; copy arguments into place
		sub   sp, sp, r3
		mov   r4, #0
		b     vmInvoke_argumentTest

vmInvoke_argumentLoop
		ldr   r5, [r2, r4]
		str   r5, [sp, r4]
		add   r4, r4, #BYTES_PER_WORD

vmInvoke_argumentTest
		cmp   r4, r3
		blt   vmInvoke_argumentLoop

		; we use r8 to hold the thread pointer, by convention
		mov   r8, r0

		; load and call function address
		blx   r1

		EXPORT vmInvoke_returnAddress
vmInvoke_returnAddress
		; restore stack pointer
		ldr   sp, [r8, #TARGET_THREAD_SCRATCH]
   
		; clear MyThread::stack to avoid confusing another thread calling
		; java.lang.Thread.getStackTrace on this one.  See
		; MyProcess::getStackTrace in compile.cpp for details on how we get
		; a reliable stack trace from a thread that might be interrupted at
		; any point in its execution.
		mov  r5, #0
		str  r5, [r8, #TARGET_THREAD_STACK]

		EXPORT vmInvoke_safeStack
vmInvoke_safeStack

;if :DEF:AVIAN_CONTINUATIONS
;		; call the next continuation, if any
;		ldr  r5,[r8,#TARGET_THREAD_CONTINUATION]
;		cmp  r5,#0
;		beq  vmInvoke_exit)
;
;		ldr  r6,[r5,#CONTINUATION_LENGTH]
;		lsl  r6,r6,#2
;		neg  r7,r6
;		add  r7,r7,#-80
;		mov  r4,sp
;		str  r4,[sp,r7]!
;
;		add  r7,r5,#CONTINUATION_BODY
;
;		mov  r11,#0
;		b    vmInvoke_continuationTest
;
;vmInvoke_continuationLoop
;		ldr  r9,[r7,r11]
;		str  r9,[sp,r11]
;		add  r11,r11,#4
;
;vmInvoke_continuationTest
;		cmp  r11,r6
;		ble  vmInvoke_continuationLoop)
;
;		ldr  r7,[r5,#CONTINUATION_RETURN_ADDRESS_OFFSET]
;		ldr  r10,vmInvoke_returnAddress_word
;		ldr  r11,vmInvoke_getAddress_word
;vmInvoke_getAddress
;		add  r11,pc,r11
;		ldr  r11,[r11,r10]
;		str  r11,[sp,r7]
;
;		ldr  r7,[r5,#CONTINUATION_NEXT]
;		str  r7,[r8,#TARGET_THREAD_CONTINUATION]
;
;		; call the continuation unless we're handling an exception
;		ldr  r7,[r8,#TARGET_THREAD_EXCEPTION]
;		cmp  r7,#0
;		bne  vmInvoke_handleException)
;		ldr  r7,[r5,#CONTINUATION_ADDRESS]
;		bx   r7
;
;vmInvoke_handleException
;		; we're handling an exception - call the exception handler instead
;		mov  r11,#0
;		str  r11,[r8,#TARGET_THREAD_EXCEPTION]
;		ldr  r11,[r8,#TARGET_THREAD_EXCEPTIONSTACKADJUSTMENT]
;		ldr  r9,[sp]
;		neg  r11,r11
;		str  r9,[sp,r11]!
;		ldr  r11,[r8,#TARGET_THREAD_EXCEPTIONOFFSET]
;		str  r7,[sp,r11]
;
;		ldr  r7,[r8,#TARGET_THREAD_EXCEPTIONHANDLER]
;		bx   r7
;
;vmInvoke_exit
;endif ; AVIAN_CONTINUATIONS

		mov   ip, #0
		str   ip, [r8, #TARGET_THREAD_STACK]

		; restore return type
		ldr   ip, [sp], #4

		; restore callee-saved registers
		ldmfd sp!, {r4-r11, lr}

vmInvoke_return
		bx    lr

		EXPORT vmJumpAndInvoke
vmJumpAndInvoke
;if :DEF:AVIAN_CONTINUATIONS
;		;      r0: thread
;		;      r1: address
;		;      r2: stack
;		;      r3: argumentFootprint
;		; [sp,#0]: arguments
;		; [sp,#4]: frameSize
;
;		ldr  r5,[sp,#0]
;		ldr  r6,[sp,#4]
;
;		; allocate new frame, adding room for callee-saved registers, plus
;		; 4 bytes of padding since the calculation of frameSize assumes 4
;		; bytes have already been allocated to save the return address,
;		; which is not true in this case
;		sub  r2,r2,r6
;		sub  r2,r2,#84
;   
;		mov  r8,r0
;
;		; copy arguments into place
;		mov  r6,#0
;		b    vmJumpAndInvoke_argumentTest
;
;vmJumpAndInvoke_argumentLoop
;		ldr  r12,[r5,r6]
;		str  r12,[r2,r6]
;		add  r6,r6,#4
;
;vmJumpAndInvoke_argumentTest
;		cmp  r6,r3
;		ble  vmJumpAndInvoke_argumentLoop
;
;		; the arguments have been copied, so we can set the real stack
;		; pointer now
;		mov  sp,r2
;   
;		; set return address to vmInvoke_returnAddress
;		ldr  r10,vmInvoke_returnAddress_word)
;		ldr  r11,vmJumpAndInvoke_getAddress_word)
;vmJumpAndInvoke_getAddress
;		add  r11,pc,r11
;		ldr  lr,[r11,r10]
;
;		bx   r1
;
;vmInvoke_returnAddress_word
;   .word GLOBAL(vmInvoke_returnAddress)(GOT)
;vmInvoke_getAddress_word
;   .word _GLOBAL_OFFSET_TABLE_-(vmInvoke_getAddress)+8)
;vmJumpAndInvoke_getAddress_word
;   .word _GLOBAL_OFFSET_TABLE_-(vmJumpAndInvoke_getAddress)+8)
;   
;else ; not AVIAN_CONTINUATIONS
		; vmJumpAndInvoke should only be called when continuations are
		; enabled
		bkpt 0	
;endif ; not AVIAN_CONTINUATIONS

		END