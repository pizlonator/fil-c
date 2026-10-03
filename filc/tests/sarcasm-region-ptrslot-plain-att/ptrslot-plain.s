# The same Whirlpool parameter-block shape as sarcasm-region-ptrslot-att,
# but WITHOUT the `#! store ptr` / `#! load ptr` annotations.  The frame
# escape (the mid-function `and $-64, %rsp` plus the `leaq 128(%rsp)`
# carrier) promotes the frame to a GC region -- real heap memory -- so the
# plain store of the argument pointer writes only the 8 raw bytes.  The
# plain reload hands back a capability-less pointer, and the first
# dereference traps.  This is the deterministic failure the unannotated
# wp-x86_64.pl restore hit at wp-x86_64.s:559 ("cannot read pointer with
# null object" out of whirlpool_block, called from WHIRLPOOL_Final); the
# sibling test with the annotations passes.
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