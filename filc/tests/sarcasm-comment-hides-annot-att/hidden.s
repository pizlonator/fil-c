	.file	"hidden.s"
	.text
	# A `#!` annotation hidden inside a `#` comment is NOT an annotation:
	# `#` starts a comment that runs to end of line, so everything from the
	# first `#` -- including any later `#!` text -- is dropped. The store
	# below is therefore a plain integer store: it does not carry the
	# capability, so the round-tripped value cannot be dereferenced.
	.p2align 4
	.globl	hidden_roundtrip
	.type	hidden_roundtrip, @function
hidden_roundtrip:               ;! ptr(ptr, ptr)
	movq	%rsi, (%rdi)        # spill the value #! store ptr
	movq	(%rdi), %rax        #! load ptr
	ret
	.size	hidden_roundtrip, .-hidden_roundtrip
	.section	.note.GNU-stack,"",@progbits
