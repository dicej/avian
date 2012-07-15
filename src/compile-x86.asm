 .586
 .model FLAT, C


include <types.inc>
include <target-fields.inc>

ifdef AVIAN_USE_FRAME_POINTER
  ALIGNMENT_ADJUSTMENT equ 0
else
  ALIGNMENT_ADJUSTMENT equ 12
endif

CALLEE_SAVED_REGISTER_FOOTPRINT equ 16 + ALIGNMENT_ADJUSTMENT

_TEXT SEGMENT

 public C vmInvoke
vmInvoke:
 push ebp
 mov ebp,esp
 
 ; 8(%ebp): thread
 ; 12(%ebp): function
 ; 16(%ebp): arguments
 ; 20(%ebp): argumentFootprint
 ; 24(%ebp): frameSize
 ; 28(%ebp): returnType
 
 ; allocate stack space for callee-saved registers
 sub esp,offset CALLEE_SAVED_REGISTER_FOOTPRINT
 
 ; remember this stack position, since we won't be able to rely on 
 ; %rbp being restored when the call returns 
 mov eax,ds:dword ptr[8+ebp]
 mov ds:dword ptr[TARGET_THREAD_SCRATCH+eax],esp
 
 mov ds:dword ptr[0+esp],ebx
 mov ds:dword ptr[4+esp],esi
 mov ds:dword ptr[8+esp],edi
 
 ; allocate stack space for arguments
 sub esp,ds:dword ptr[24+ebp]
 
 ; we use ebx to hold the thread pointer, by convention
 mov ebx,eax
 
 ; copy arguments into place
 mov ecx,0
 mov edx,ds:dword ptr[16+ebp]
 jmp LvmInvoke_argumentTest
 
LvmInvoke_argumentLoop:
 mov eax,ds:dword ptr[edx+ecx*1]
 mov ds:dword ptr[esp+ecx*1],eax
 add ecx,4
 
LvmInvoke_argumentTest:
 cmp ecx,ds:dword ptr[20+ebp]
 jb LvmInvoke_argumentLoop
 
 ; call function
 call dword ptr[12+ebp]
 
 public vmInvoke_returnAddress
vmInvoke_returnAddress:
 ; restore stack pointer
 mov esp,ds:dword ptr[TARGET_THREAD_SCRATCH+ebx]

 ; clear MyThread::stack to avoid confusing another thread calling
 ; java.lang.Thread.getStackTrace on this one.  See
 ; MyProcess::getStackTrace in compile.cpp for details on how we get
 ; a reliable stack trace from a thread that might be interrupted at
 ; any point in its execution. 
 mov ds:dword ptr[TARGET_THREAD_STACK+ebx],0
 
 public vmInvoke_safeStack
vmInvoke_safeStack:

 ; restore callee-saved registers
 mov ebx,ds:dword ptr[0+esp]
 mov esi,ds:dword ptr[4+esp]
 mov edi,ds:dword ptr[8+esp]
 
 add esp,offset CALLEE_SAVED_REGISTER_FOOTPRINT
 
 mov ecx,ds:dword ptr[28+esp]
 
 pop ebp
 ret
 
LgetPC:
 mov esi,ds:dword ptr[esp]
 ret
 
 public vmJumpAndInvoke
vmJumpAndInvoke:
 ; vmJumpAndInvoke should only be called when continuations are
 ; enabled
 int 3
_TEXT ENDS
 END
