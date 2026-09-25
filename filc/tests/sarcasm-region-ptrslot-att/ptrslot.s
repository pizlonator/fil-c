# The Whirlpool parameter-block shape (projects/openssl-3.6.4's
# wp-x86_64.pl): a mid-function `and $-64, %rsp` alignment makes the frame
# depth dynamic, so the `leaq 128(%rsp), %r10` carrier is a frame escape.
# This used to be accepted by the D9 fixed-frame escape promotion, which
# materialized the whole frame as a GC region (REAL heap memory, allocated
# with filc_allocate); a pointer round-trip through that memory then REQUIRED
# `#! store ptr` / `#! load ptr` (a plain store wrote only the 8 raw bytes
# and lost the invisible-cap sidecar, so the plain reload trapped). That
# promotion was removed: sarcasm now REJECTS taking the address of the stack
# frame at compile time, so this file fails with "taking address of stack
# frame is not supported (cannot prove safety)" on the escaping lea — with
# or without the ptr annotations.
	.text
	.globl	region_roundtrip
	.type	region_roundtrip, @function
region_roundtrip:               #! long(ptr)
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	subq	$168, %rsp
	andq	$-64, %rsp

	leaq	128(%rsp), %r10
	movq	%r10, %rbx              # carrier copy (wp's `mov %r10,%rbx`)
	movq	%rdi, 0(%rbx)   #! store ptr # save the argument pointer
	movq	$0, 24(%rbx)            # scalar slot (the round counter), plain
	xorq	%rsi, %rsi              # kill the argument copy, like the loop body
	jmp	.Lround
.align	16
.Lround:
	leaq	128(%rsp), %rbx         # re-derived carrier mid-loop (like wp)
	movq	24(%rbx), %rsi          # plain scalar round-trip through the region
	addq	$1, %rsi
	cmpq	$3, %rsi
	je	.Lroundsdone
	movq	%rsi, 24(%rbx)          # plain scalar update
	jmp	.Lround
.align	16
.Lroundsdone:
	movq	0(%rbx), %rax   #! load ptr # reload the pointer from the region
	movq	(%rax), %rdx            # dereference: *p -- traps if the capability
	                                # was lost on the plain round-trip
	movq	24(%rbx), %rcx          # scalar readback: 2
	addq	%rcx, %rdx              # *p + 2
	leaq	64(%rax), %rax          # advance the pointer (wp's inp += 64)
	movq	%rax, 0(%rbx)   #! store ptr # update the parameter block
	movq	0(%rbx), %rax   #! load ptr # reload the updated pointer
	movq	(%rax), %rax            # dereference the advanced pointer: p[8]
	addq	%rdx, %rax              # p[0] + 2 + p[8]
	leaq	216(%rsp), %rsp
.Lepilogue:
	ret
	.size	region_roundtrip, .-region_roundtrip
	.section	.note.GNU-stack,"",@progbits