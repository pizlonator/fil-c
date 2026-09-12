	.file	"hidesig.s"
	.text
	# A signature hidden inside a `#` comment is not a signature: `#` starts
	# a comment that runs to end of line, so `f` below has no annotation and
	# sarcasm must reject it (missing signature), not honor the hidden sig.
	.globl	f
	.type	f, @function
f: # the signature #! ptr(ptr)
	movq	(%rdi), %rax #! load ptr
	ret
	.size	f, .-f
