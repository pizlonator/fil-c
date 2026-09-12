# B2 mid-body shared-tail join with a POINTER out argument (the x25519
# .Lreduce64 shape): the jumper (3 pointer args) and the owner (2 pointer
# args) have identical prologues that offload the out pointer with push %rdi;
# both clobber %rdi as zero scratch, then join in a shared tail that reloads
# the out pointer from its push-spill slot and stores the results through it.
# The clone's reload/restores must share the jumper's slots (same address);
# banding them reads garbage with a null capability and panics.
	.text
	.globl	mul_ptr
	.type	mul_ptr,@function
	.align	32
mul_ptr: #! void(ptr,ptr,ptr)
	.cfi_startproc
	pushq	%rbp
	.cfi_adjust_cfa_offset	8
	pushq	%rbx
	.cfi_adjust_cfa_offset	8
	pushq	%r12
	.cfi_adjust_cfa_offset	8
	pushq	%r13
	.cfi_adjust_cfa_offset	8
	pushq	%r14
	.cfi_adjust_cfa_offset	8
	pushq	%r15
	.cfi_adjust_cfa_offset	8
	pushq	%rdi
	.cfi_adjust_cfa_offset	8
	leaq	-16(%rsp),%rsp
	.cfi_adjust_cfa_offset	16
.Lmul_ptr_body:
	movq	0(%rsi),%r8
	movq	0(%rdx),%r9
	xorl	%edi,%edi
	addq	%r9,%r8
	adcq	%rdi,%r8
	jmp	.Lreduce_ptr
	.cfi_endproc
	.size	mul_ptr,.-mul_ptr

	.globl	sqr_ptr
	.type	sqr_ptr,@function
	.align	32
sqr_ptr: #! void(ptr,ptr)
	.cfi_startproc
	pushq	%rbp
	.cfi_adjust_cfa_offset	8
	pushq	%rbx
	.cfi_adjust_cfa_offset	8
	pushq	%r12
	.cfi_adjust_cfa_offset	8
	pushq	%r13
	.cfi_adjust_cfa_offset	8
	pushq	%r14
	.cfi_adjust_cfa_offset	8
	pushq	%r15
	.cfi_adjust_cfa_offset	8
	pushq	%rdi
	.cfi_adjust_cfa_offset	8
	leaq	-16(%rsp),%rsp
	.cfi_adjust_cfa_offset	16
.Lsqr_ptr_body:
	movq	0(%rsi),%r8
	movq	8(%rsi),%r9
	xorl	%edi,%edi
	addq	%r9,%r8
	adcq	%rdi,%r8
	jmp	.Lreduce_ptr
	.align	32
.Lreduce_ptr:
	addq	%r8,%r9
	movq	16(%rsp),%rdi
	movq	%r9,8(%rdi)
	movq	%r8,0(%rdi)
	movq	24(%rsp),%r15
	.cfi_restore	%r15
	movq	32(%rsp),%r14
	.cfi_restore	%r14
	movq	40(%rsp),%r13
	.cfi_restore	%r13
	movq	48(%rsp),%r12
	.cfi_restore	%r12
	movq	56(%rsp),%rbx
	.cfi_restore	%rbx
	movq	64(%rsp),%rbp
	.cfi_restore	%rbp
	leaq	72(%rsp),%rsp
	.cfi_adjust_cfa_offset	88
	ret
	.cfi_endproc
	.size	sqr_ptr,.-sqr_ptr
	.section	.note.GNU-stack,"",@progbits
