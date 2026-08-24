/* Regression (adc consumer form of the inc/CF bug): the pending carry from
   cmpq must survive the instrumented incq so that adcq $0, %rax materializes
   exactly the cmp's carry. adc is a flag USE (carry-in), inc is flag-neutral
   (preserves CF), so the flag save/restore must wrap the inserted check. */
	.text
	.globl	incadc
	.type	incadc, @function
incadc:                         ;! long(ptr, ptr)
	movq	$0, %rax
	cmpq	%rdi, %rsi
	incq	(%rdi)
	adcq	$0, %rax
	ret
	.size	incadc, .-incadc
	.section	.note.GNU-stack,"",@progbits
