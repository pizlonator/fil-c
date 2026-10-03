# Prefix combos on the SSE map: F3 0F 6F (movdqu load), 66 0F 7F (movdqa
# store), F2 0F 10 (movsd load) and F2 0F 11 (movsd store) — decoded from
# .byte runs and executed, with the 66/F2/F3 prefix semantics carried into
# the decoded mnemonic.
	.text
	.globl	sse_copy16
	.type	sse_copy16, @function
sse_copy16:                     ;! void(ptr, ptr)
	# f3 0f 6f 06           movdqu (%rsi),%xmm0
	# 66 0f 7f 07           movdqa %xmm0,(%rdi)
	.byte	0xf3,0x0f,0x6f,0x06
	.byte	0x66,0x0f,0x7f,0x07
	ret
	.size	sse_copy16, .-sse_copy16
	.globl	sse_copyd
	.type	sse_copyd, @function
sse_copyd:                      ;! void(ptr, ptr)
	# f2 0f 10 06           movsd (%rsi),%xmm0
	# f2 0f 11 07           movsd %xmm0,(%rdi)
	.byte	0xf2,0x0f,0x10,0x06
	.byte	0xf2,0x0f,0x11,0x07
	# 66 0f 28 c1           movapd %xmm1,%xmm0 (66-prefixed full move)
	.byte	0x66,0x0f,0x28,0xc1
	ret
	.size	sse_copyd, .-sse_copyd
	.globl	sse_add4
	.type	sse_add4, @function
sse_add4:                       ;! void(ptr, ptr)
	# f3 0f 6f 06           movdqu (%rsi),%xmm0
	# 66 0f fe 07           paddd (%rdi),%xmm0
	# 66 0f 7f 07           movdqa %xmm0,(%rdi)
	.byte	0xf3,0x0f,0x6f,0x06
	.byte	0x66,0x0f,0xfe,0x07
	.byte	0x66,0x0f,0x7f,0x07
	ret
	.size	sse_add4, .-sse_add4
	.section	.note.GNU-stack,"",@progbits
