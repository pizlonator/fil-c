	.file	"order.s"
	.text
	# `#!` annotation followed by a `#` comment is annotation+comment: the
	# annotation body is the text between `#!` and the `#`, trimmed. (A `#`
	# comment BEFORE `#!` hides the annotation instead; see
	# sarcasm-comment-hides-annot-att.)
	.p2align 4
	.globl	order_store
	.type	order_store, @function
order_store:                    #! void(ptr, ptr)
	movq	%rsi, (%rdi)        #! store ptr # spilled value, trailing note
	ret
	.size	order_store, .-order_store
	.globl	order_load
	.type	order_load, @function
order_load:                     #! ptr(ptr)
	movq	(%rdi), %rax        #! load ptr # filled value, trailing note
	ret
	.size	order_load, .-order_load
	.globl	order_load_semi
	.type	order_load_semi, @function
order_load_semi:                ;! ptr(ptr)
	movq	(%rdi), %rax        ;! load ptr # universal marker, trailing note
	ret
	.size	order_load_semi, .-order_load_semi
	.section	.note.GNU-stack,"",@progbits
