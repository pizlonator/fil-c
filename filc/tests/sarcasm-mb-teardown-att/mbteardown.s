# MB-geometry teardown proof: the aesni-mb enc8x frame shape (six pushes +
# `sub $192, %rsp` + `and $-128, %rsp`, entry rsp parked in %rax and spilled
# to 16(%rsp)) with a body that reuses the carrier (like $rounds in %eax) and
# clobbers every pushed register (so the restores are load-bearing, not
# vacuous), torn down by carrier recovery (`mov 16(%rsp),%rax` to
# re-materialize the carrier, `mov -K(%rax),%reg` restores, `lea (%rax),%rsp`
# recovery). A slot/add teardown cannot work here: `add` cannot invert the
# `and` slack (0..127), and with the sub/and frame dropped the slot offsets
# would point into the caller. The C harness sweeps 128 entry depths (every
# mod-128 residue) with live keepers in the caller's callee-saved registers,
# so wrong restores, a wrong rsp recovery, or miscompiled post-and slots fail
# deterministically.
	.text
	.globl	mbteardown_test
	.type	mbteardown_test, @function
mbteardown_test:                ;! long(long,long)
	movq	%rsp, %rax
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$192, %rsp
	and	$-128, %rsp
	movq	%rax, 16(%rsp)
	movq	%rdi, 32(%rsp)
	movq	%rsi, 40(%rsp)
	movl	$0x4252, %eax
	movq	$0xdead1, %rbx
	movq	$0xdead2, %rbp
	movq	$0xdead3, %r12
	movq	$0xdead4, %r13
	movq	$0xdead5, %r14
	movq	$0xdead6, %r15
	movq	32(%rsp), %r10
	addq	40(%rsp), %r10
	movq	16(%rsp), %rax
	movq	-48(%rax), %r15
	movq	-40(%rax), %r14
	movq	-32(%rax), %r13
	movq	-24(%rax), %r12
	movq	-16(%rax), %rbp
	movq	-8(%rax), %rbx
	leaq	(%rax), %rsp
	movq	%r10, %rax
	ret
	.size	mbteardown_test, .-mbteardown_test
	.section	.note.GNU-stack,"",@progbits
