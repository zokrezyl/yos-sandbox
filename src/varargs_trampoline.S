// x86_64 varargs trampoline - calls any function with args from memory
//
// void* call_native_varargs(void* func, void* args, int arg_count);
//
// Args are packed as uint64_t array (each arg promoted to 64-bit)
// First 6 go to registers RDI,RSI,RDX,RCX,R8,R9
// Rest pushed to stack
//
// Returns result in RAX

.globl call_native_varargs
.type call_native_varargs, @function

call_native_varargs:
    // func in %rdi, args in %rsi, arg_count in %edx
    push %rbp
    mov %rsp, %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15

    // Save func pointer
    mov %rdi, %r12          // r12 = func
    mov %rsi, %r13          // r13 = args array
    mov %edx, %r14d         // r14 = arg_count

    // Calculate stack space needed for overflow args (args beyond 6)
    xor %r15d, %r15d        // r15 = stack args count
    cmp $6, %r14d
    jle .no_stack_args
    mov %r14d, %r15d
    sub $6, %r15d           // r15 = arg_count - 6

.no_stack_args:
    // Align stack to 16 bytes before the call
    // After 6 initial pushes, RSP % 16 = 8
    // If overflow_count is even, total pushes is even, RSP stays at 8 - need adjustment
    // If overflow_count is odd, total pushes is odd, RSP becomes 0 - no adjustment needed
    mov %r15d, %eax
    and $1, %eax            // 1 if odd, 0 if even
    xor $1, %eax            // invert: 0 if odd, 1 if even
    shl $3, %eax            // * 8 bytes
    sub %rax, %rsp          // adjust if even overflow count

    // Push overflow args in reverse order (arg[n-1] first)
    test %r15d, %r15d
    jz .load_reg_args

    mov %r14d, %ecx         // ecx = total arg count
    dec %ecx                // start from last arg
.push_loop:
    cmp $6, %ecx
    jl .load_reg_args
    mov (%r13, %rcx, 8), %rax
    push %rax
    dec %ecx
    jmp .push_loop

.load_reg_args:
    // Load first 6 args into registers
    // arg[0] -> RDI, arg[1] -> RSI, etc.

    cmp $0, %r14d
    jle .do_call
    mov 0(%r13), %rdi       // arg[0]

    cmp $1, %r14d
    jle .do_call
    mov 8(%r13), %rsi       // arg[1]

    cmp $2, %r14d
    jle .do_call
    mov 16(%r13), %rdx      // arg[2]

    cmp $3, %r14d
    jle .do_call
    mov 24(%r13), %rcx      // arg[3]

    cmp $4, %r14d
    jle .do_call
    mov 32(%r13), %r8       // arg[4]

    cmp $5, %r14d
    jle .do_call
    mov 40(%r13), %r9       // arg[5]

.do_call:
    // AL = number of vector registers used (0 for non-variadic)
    xor %eax, %eax

    // Call the function
    call *%r12

    // Result is in RAX, return it

    // Restore stack
    mov %rbp, %rsp
    sub $40, %rsp           // account for saved registers
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    ret

.size call_native_varargs, .-call_native_varargs

// Mark stack as non-executable (suppress linker warning)
.section .note.GNU-stack,"",@progbits
