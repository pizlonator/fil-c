# The same Whirlpool parameter-block shape as sarcasm-region-ptrslot-att,
# but WITHOUT the `#! store ptr` / `#! load ptr` annotations.  The frame
# escape (the mid-function `and $-64, %rsp` plus the `leaq 128(%rsp)`
# carrier) used to be accepted by the D9 fixed-frame escape promotion, which
# made the frame real heap memory: the plain store of the argument pointer
# then wrote only the 8 raw bytes, the plain reload handed back a
# capability-less pointer, and the first dereference trapped ("cannot read
# pointer with null object" out of whirlpool_block, called from
# WHIRLPOOL_Final — the wp-x86_64.s:559 failure). That promotion was removed:
# sarcasm now REJECTS taking the address of the stack frame at compile time,
# so this file fails with "taking address of stack frame is not supported
# (cannot prove safety)" — before any code runs, annotations or not.
	.text
	.globl	region_roundtrip_plain
	.type	region_roundtrip_plain, @function
region_roundtrip_plain:         #! long(ptr)
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	subq	$168, %rsp
	andq	$-64, %rsp

	leaq	128(%rsp), %r10
	movq	%r10, %rbx              # carrier copy
	movq	%rdi, 0(%rbx)           # plain store: only the raw 8 bytes land
	movq	$0, 24(%rbx)            # scalar slot rides plain
	xorq	%rsi, %rsi
	jmp	.Lround
.align	16
.Lround:
	leaq	128(%rsp), %rbx
	movq	24(%rbx), %rsi
	addq	$1, %rsi
	cmpq	$3, %rsi
	je	.Lroundsdone
	movq	%rsi, 24(%rbx)
	jmp	.Lround
.align	16
.Lroundsdone:
	movq	0(%rbx), %rax           # plain reload: the capability is GONE
	movq	(%rax), %rdx            # dereference: TRAPS here (null object)
	movq	24(%rbx), %rcx
	addq	%rcx, %rdx
	leaq	216(%rsp), %rsp
.Lepilogue:
	movq	%rdx, %rax
	ret
	.size	region_roundtrip_plain, .-region_roundtrip_plain
	.section	.note.GNU-stack,"",@progbits