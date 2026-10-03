# The Whirlpool parameter-block shape (projects/openssl-3.6.4's
# wp-x86_64.pl): a mid-function `and $-64, %rsp` alignment makes the frame
# depth dynamic, so the `leaq 128(%rsp), %r10` carrier is a frame escape --
# D9 materializes the whole frame as a GC region (REAL heap memory, allocated
# with filc_allocate).  A pointer round-trip through that memory therefore
# REQUIRES `#! store ptr` / `#! load ptr`: a plain store into region memory
# writes only the 8 raw bytes, and the capability (the invisible-cap sidecar
# entry) is lost, so the plain reload hands back a capability-less pointer and
# the first dereference traps ("cannot read pointer with null object").
# With the annotations the capability rides the region's sidecar across the
# store/reload, and the reloaded pointer dereferences like the original.
# Scalar slots (the round counter) ride plain in both spellings: raw 8-byte
# round-trips through region memory are lossless for non-pointer values.
# The carrier itself (`leaq 128(%rsp), %r10` and its `mov %r10, %rbx` copy,
# re-derived mid-loop) keeps the region pointer's capability through the
# register moves, exactly like the openssl code.
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