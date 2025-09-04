# Testcase with a variety of "change of flow instructions"
#
# This test does not have much going on wrt synthesis of CFI;
# it just aims to ensure x8_64 -> ginsn decoding behaves
# gracefully for these "change of flow instructions"
	.text
	.globl  foo
	.type   foo, @function
foo:
	addq    %rdx, %rax
	loop    foo
	notrack jmp     *%rax
	call    *%r8
	jecxz   .L179
	jmp     *48(%rdi)
	jo      .L179
.L179:
	ret
.LFE0:
	.size   foo, .-foo
