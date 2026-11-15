/* x86 MSVC compiler-helper routines (same calling conventions as the MSVC CRT). */
        .text

/* void __fastcall __security_check_cookie(uintptr_t cookie) : cookie in ECX */
        .globl  @__security_check_cookie@4
@__security_check_cookie@4:
        cmpl    ___security_cookie, %ecx
        jne     1f
        ret
1:      jmp     _eg_security_failure

/* float -> int64 truncation, value in ST(0), result in EDX:EAX (same contract as _ftol) */
        .globl  __ftol2
        .globl  __ftol2_sse
__ftol2:
__ftol2_sse:
        subl    $12, %esp
        fnstcw  8(%esp)
        movw    8(%esp), %ax
        orw     $0x0C00, %ax            /* round toward zero */
        movw    %ax, 10(%esp)
        fldcw   10(%esp)
        fistpll (%esp)
        fldcw   8(%esp)
        movl    (%esp), %eax
        movl    4(%esp), %edx
        addl    $12, %esp
        ret

/* __chkstk / _alloca_probe (EAX = bytes, ESP moved down, page probing): libgcc's ___chkstk has the same contract */
        .globl  __chkstk
        .globl  __alloca_probe
__chkstk:
__alloca_probe:
        jmp     ___chkstk

/* 64-bit integer helpers: arguments on the stack (a at 4(%esp), b at 12(%esp)), callee pops 16 bytes */
        .globl  __allmul
__allmul:
        movl    8(%esp), %eax
        movl    16(%esp), %ecx
        orl     %eax, %ecx
        movl    12(%esp), %ecx
        jnz     4f
        movl    4(%esp), %eax
        mull    %ecx
        ret     $16
4:      pushl   %ebx
        mull    %ecx                    /* a.hi * b.lo */
        movl    %eax, %ebx
        movl    8(%esp), %eax           /* a.lo */
        mull    20(%esp)                /* a.lo * b.hi */
        addl    %eax, %ebx
        movl    8(%esp), %eax
        mull    %ecx                    /* a.lo * b.lo */
        addl    %ebx, %edx
        popl    %ebx
        ret     $16

/* signed / unsigned 64-bit division and remainder through libgcc */
        .globl  __alldiv
__alldiv:
        pushl   16(%esp)
        pushl   16(%esp)
        pushl   16(%esp)
        pushl   16(%esp)
        call    ___divdi3
        addl    $16, %esp
        ret     $16
        .globl  __allrem
__allrem:
        pushl   16(%esp)
        pushl   16(%esp)
        pushl   16(%esp)
        pushl   16(%esp)
        call    ___moddi3
        addl    $16, %esp
        ret     $16
        .globl  __aulldiv
__aulldiv:
        pushl   16(%esp)
        pushl   16(%esp)
        pushl   16(%esp)
        pushl   16(%esp)
        call    ___udivdi3
        addl    $16, %esp
        ret     $16
        .globl  __aullrem
__aullrem:
        pushl   16(%esp)
        pushl   16(%esp)
        pushl   16(%esp)
        pushl   16(%esp)
        call    ___umoddi3
        addl    $16, %esp
        ret     $16

/* shifts: value in EDX:EAX, count in CL */
        .globl  __aullshr
__aullshr:
        cmpb    $64, %cl
        jae     5f
        cmpb    $32, %cl
        jae     6f
        shrdl   %cl, %edx, %eax
        shrl    %cl, %edx
        ret
6:      movl    %edx, %eax
        xorl    %edx, %edx
        andb    $31, %cl
        shrl    %cl, %eax
        ret
5:      xorl    %eax, %eax
        xorl    %edx, %edx
        ret
        .globl  __allshl
__allshl:
        cmpb    $64, %cl
        jae     5b
        cmpb    $32, %cl
        jae     7f
        shldl   %cl, %eax, %edx
        shll    %cl, %eax
        ret
7:      movl    %eax, %edx
        xorl    %eax, %eax
        andb    $31, %cl
        shll    %cl, %edx
        ret
        .globl  __allshr
__allshr:
        cmpb    $64, %cl
        jae     8f
        cmpb    $32, %cl
        jae     9f
        shrdl   %cl, %edx, %eax
        sarl    %cl, %edx
        ret
9:      movl    %edx, %eax
        sarl    $31, %edx
        andb    $31, %cl
        sarl    %cl, %eax
        ret
8:      sarl    $31, %edx
        movl    %edx, %eax
        ret

/* MSVC-mangled global operator new / delete */
        .globl  "??2@YAPAXI@Z"
"??2@YAPAXI@Z":
        jmp     _eg_msvc_operator_new
        .globl  "??3@YAXPAX@Z"
"??3@YAXPAX@Z":
        jmp     _eg_msvc_operator_delete

/* MSVC absolute symbol: offset of the SEH chain head in the TIB (used as fs:__except_list) */
        .globl  __except_list
        .set    __except_list, 0
