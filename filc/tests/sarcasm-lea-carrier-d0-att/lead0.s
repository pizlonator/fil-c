	.text
	# Feature (D0 interior lea-save carriers): `leaq 32(%rsp), %rbx` at the
	# PROLOGUE depth (the `subq` is part of the prologue prefix, so d == D0)
	# parks a carrier instead of promoting the frame to a GC region, because
	# every use of %rbx between the lea and its redefinition is a single plain
	# memory-base use. Each access through the carrier resolves to the same
	# normalized slot the equivalent rsp-relative spelling keys (0(%rbx) with
	# K=32 at D0=96 is 32(%rsp); 16(%rbx) is 48(%rsp)), so the two spellings
	# share one slot web and the function runs with the carrier dropped.
	.globl	lead0
	.type	lead0, @function
lead0:                          ;! long(long)
	subq	$96, %rsp
	leaq	32(%rsp), %rbx     # D0 interior carrier (parked depth 64)
	movq	%rdi, 0(%rbx)      # carrier store -> slot 32
	movq	%rdi, 40(%rsp)     # direct rsp spelling -> slot 40
	addq	$5, 40(%rsp)
	movq	0(%rbx), %rax      # carrier load -> slot 32
	addq	40(%rsp), %rax     # direct rsp load -> slot 40
	movq	%rax, 16(%rbx)     # carrier store -> slot 48
	movq	16(%rbx), %rax     # carrier load -> slot 48
	addq	$96, %rsp
	ret
	.size	lead0, .-lead0
	.section	.note.GNU-stack,"",@progbits
