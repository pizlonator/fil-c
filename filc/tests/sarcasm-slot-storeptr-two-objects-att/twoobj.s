	.text
	# Two DIFFERENT objects stored into the SAME frame slot at different times:
	# the capability must track whichever object the slot currently holds. The
	# conflicting stores give the slot web two pointer origins, which widens it
	# to a dynamic (lockstep) lower -- each def carries its own source's
	# capability. The big-object derefs sit at offsets a small object's
	# capability could never cover, so a stale capability would trap.
	.globl	two_objects
	.type	two_objects, @function
two_objects:                    ;! long(ptr, ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$32, %rsp
	movq	%rdi, %r11         # small object (global, 16 bytes)
	movq	%rsi, %r10         # big object (heap, 64 bytes)
	# store the BIG object, deref deep inside it (offset 40)
	movq	%r10, -8(%rbp)     ;! store ptr
	movq	-8(%rbp), %rax     ;! load ptr
	movq	40(%rax), %rdi
	# store the SMALL object in the SAME slot, deref it
	movq	%r11, -8(%rbp)     ;! store ptr
	movq	-8(%rbp), %rax     ;! load ptr
	movq	(%rax), %rsi
	# store the BIG object again, deref even deeper (offset 56)
	movq	%r10, -8(%rbp)     ;! store ptr
	movq	-8(%rbp), %rax     ;! load ptr
	movq	56(%rax), %rdx
	addq	%rsi, %rdi
	addq	%rdx, %rdi
	movq	%rdi, %rax
	leave
	ret
	.size	two_objects, .-two_objects

	# Loop-carried variant: the slot holds a different element pointer every
	# iteration (the aesni-mb output-position shape).
	.globl	two_objects_loop
	.type	two_objects_loop, @function
two_objects_loop:               ;! long(ptr, long)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$32, %rsp
	movq	%rdi, %r10
	movl	%esi, %r11d
	xorl	%r8d, %r8d
	xorl	%r9d, %r9d
.Lloop:
	movl	%r8d, %eax
	shlq	$3, %rax
	leaq	(%r10,%rax), %rdx  # &a[i]
	movq	%rdx, -8(%rbp)     ;! store ptr
	movq	-8(%rbp), %rax     ;! load ptr
	movq	(%rax), %rsi
	addq	%rsi, %r9
	addl	$1, %r8d
	cmpl	%r11d, %r8d
	jl	.Lloop
	movq	%r9, %rax
	leave
	ret
	.size	two_objects_loop, .-two_objects_loop
	.section	.note.GNU-stack,"",@progbits
