/* rcr reads CF (rotate through carry), so the instrumented load between the
   program's cmp and the rcr must be bracketed by pushfq/popfq. The new MSB of
   the rcr result is the incoming CF, so shifting it down yields the cmp's
   carry: f(p, 3) == 1 (3 - 5 borrows), f(p, 9) == 0. Without the bracket the
   access check's testq would clobber CF first. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(ptr, long)
	cmpq	$5, %rsi
	movq	(%rdi), %rax
	rcrq	%rcx
	movq	%rcx, %rax
	shrq	$63, %rax
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
