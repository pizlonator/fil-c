/* bswap r/m is a unary read-modify-write on ONE register (like not/neg), so
   it is modeled in DST_ONLY: the input web flows into the bswap register and
   the result web flows out (previously it hit the unknown-mnemonic fallback,
   which modeled a bare def and disconnected the input web). f byte-swaps the
   64-bit argument, g byte-swaps the low 32 bits (a 32-bit op zero-extends
   into rax), h byte-swaps a loaded qword (an instrumented load composing
   with bswap). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movq	%rdi, %rax
	bswapq	%rax
	ret
	.size	f, .-f
	.globl	g
	.type	g, @function
g:                              ;! long(long)
	movl	%edi, %eax
	bswapl	%eax
	ret
	.size	g, .-g
	.globl	h
	.type	h, @function
h:                              ;! long(ptr)
	movq	(%rdi), %rax
	bswapq	%rax
	ret
	.size	h, .-h
	.section	.note.GNU-stack,"",@progbits
