# A PARTIALLY decodable run: `48 89 c3` (movq %rbx,%rax) followed by `ff`
# — a prefix byte with no opcode after it. The run cannot be FULLY decoded,
# and a partial decode is never emitted: the whole run keeps its original
# data spelling and the body is rejected.
	.text
	.globl	partialb
	.type	partialb, @function
partialb:                       ;! long(long)
	movq	%rdi, %rax
	.byte	0x48,0x89,0xc3,0xff
	ret
	.size	partialb, .-partialb
	.section	.note.GNU-stack,"",@progbits
