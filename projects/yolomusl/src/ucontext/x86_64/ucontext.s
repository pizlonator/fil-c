/* x86_64 Linux ucontext implementation for yolomusl.
 *
 * Field offsets match arch/x86_64/bits/signal.h (verified at compile time
 * in src/ucontext/x86_64/makecontext.c):
 *   gregs[REG_R8..REG_RIP]: 40..168
 *   uc_mcontext.fpregs:     224
 *   uc_sigmask:             296
 *   __fpregs_mem:           424
 *   __fpregs_mem.mxcsr:     448
 */

.text

.global getcontext
.type getcontext,@function
getcontext:
	movq %rbx, 128(%rdi)
	movq %rbp, 120(%rdi)
	movq %r12, 72(%rdi)
	movq %r13, 80(%rdi)
	movq %r14, 88(%rdi)
	movq %r15, 96(%rdi)
	movq %rdi, 104(%rdi)
	movq %rsi, 112(%rdi)
	movq %rdx, 136(%rdi)
	movq %rcx, 152(%rdi)
	movq %r8,  40(%rdi)
	movq %r9,  48(%rdi)

	movq (%rsp), %rcx
	movq %rcx, 168(%rdi)
	leaq 8(%rsp), %rcx
	movq %rcx, 160(%rdi)

	leaq 424(%rdi), %rcx
	movq %rcx, 224(%rdi)
	fnstenv (%rcx)
	fldenv (%rcx)
	stmxcsr 448(%rdi)

	/* rt_sigprocmask(SIG_BLOCK=0, NULL, &ucp->uc_sigmask, 8) */
	leaq 296(%rdi), %rdx
	xorl %esi, %esi
	xorl %edi, %edi
	movl $8, %r10d
	movl $14, %eax
	syscall
	cmpq $-4095, %rax
	jae 1f
	xorl %eax, %eax
	ret
1:
	movq $-1, %rax
	ret
.size getcontext,.-getcontext

.global setcontext
.type setcontext,@function
setcontext:
	pushq %rdi

	/* rt_sigprocmask(SIG_SETMASK=2, &ucp->uc_sigmask, NULL, 8) */
	leaq 296(%rdi), %rsi
	xorl %edx, %edx
	movl $2, %edi
	movl $8, %r10d
	movl $14, %eax
	syscall
	popq %rdx
	cmpq $-4095, %rax
	jae 1f

	movq 224(%rdx), %rcx
	fldenv (%rcx)
	ldmxcsr 448(%rdx)

	movq 160(%rdx), %rsp
	movq 128(%rdx), %rbx
	movq 120(%rdx), %rbp
	movq 72(%rdx),  %r12
	movq 80(%rdx),  %r13
	movq 88(%rdx),  %r14
	movq 96(%rdx),  %r15

	movq 168(%rdx), %rcx
	pushq %rcx

	movq 112(%rdx), %rsi
	movq 104(%rdx), %rdi
	movq 152(%rdx), %rcx
	movq 40(%rdx),  %r8
	movq 48(%rdx),  %r9
	movq 136(%rdx), %rdx

	xorl %eax, %eax
	ret
1:
	movq $-1, %rax
	ret
.size setcontext,.-setcontext

.global swapcontext
.type swapcontext,@function
swapcontext:
	movq %rbx, 128(%rdi)
	movq %rbp, 120(%rdi)
	movq %r12, 72(%rdi)
	movq %r13, 80(%rdi)
	movq %r14, 88(%rdi)
	movq %r15, 96(%rdi)
	movq %rdi, 104(%rdi)
	movq %rsi, 112(%rdi)
	movq %rdx, 136(%rdi)
	movq %rcx, 152(%rdi)
	movq %r8,  40(%rdi)
	movq %r9,  48(%rdi)

	movq (%rsp), %rcx
	movq %rcx, 168(%rdi)
	leaq 8(%rsp), %rcx
	movq %rcx, 160(%rdi)

	leaq 424(%rdi), %rcx
	movq %rcx, 224(%rdi)
	fnstenv (%rcx)
	stmxcsr 448(%rdi)

	movq %rsi, %r12
	movq %rdi, %r13

	/* rt_sigprocmask(SIG_SETMASK=2, &ucp->uc_sigmask, &oucp->uc_sigmask, 8) */
	leaq 296(%rdi), %rdx
	leaq 296(%rsi), %rsi
	movl $2, %edi
	movl $8, %r10d
	movl $14, %eax
	syscall
	cmpq $-4095, %rax
	jae 1f

	movq %r12, %rdx

	movq 224(%rdx), %rcx
	fldenv (%rcx)
	ldmxcsr 448(%rdx)

	movq 160(%rdx), %rsp
	movq 128(%rdx), %rbx
	movq 120(%rdx), %rbp
	movq 72(%rdx),  %r12
	movq 80(%rdx),  %r13
	movq 88(%rdx),  %r14
	movq 96(%rdx),  %r15

	movq 168(%rdx), %rcx
	pushq %rcx

	movq 112(%rdx), %rsi
	movq 104(%rdx), %rdi
	movq 152(%rdx), %rcx
	movq 40(%rdx),  %r8
	movq 48(%rdx),  %r9
	movq 136(%rdx), %rdx

	xorl %eax, %eax
	ret
1:
	movq 72(%r13), %r12
	movq 80(%r13), %r13
	movq $-1, %rax
	ret
.size swapcontext,.-swapcontext

.hidden __yolomusl_start_context
.global __yolomusl_start_context
.type __yolomusl_start_context,@function
__yolomusl_start_context:
	movq (%rbx), %rdi
	movq %rbx, %rsp
	andq $-16, %rsp
	testq %rdi, %rdi
	je 1f
	call setcontext
1:
	xorl %edi, %edi
	movl $231, %eax
	syscall
	hlt
.size __yolomusl_start_context,.-__yolomusl_start_context

.section .note.GNU-stack,"",@progbits
