# Clone-ret dynamic-depth rejection (fail-closed): the jumper's paths merge
# at different depths, so the B2 jump fires at an unprovable ("dyn") depth;
# the clone (balanced in itself) inherits the dyn entry depth and its `ret`
# sits at dyn depth. The clone-balance check runs on both region-carrying
# analysis runs (the alloca-regions run and the preprocess run whose marks the
# rewrite consumes), so this is a static error even with no alloca regions
# anywhere. It is deliberately skipped on the region-less early geometry run
# — whose marks are all discarded and whose frameSize is superseded (see
# transform.luau) — because that run cannot record the header caller-saved
# saves a carrier-recovery epilogue needs; checking there would reject shapes
# the later runs legalize. (With no alloca regions anywhere, the old
# regions-gated check never ran and this compiled.)
	.text
	.globl	dyn_jump
	.type	dyn_jump, @function
dyn_jump:                       ;! long(long,long)
	subq	$8, %rsp
	testl	%edi, %edi
	jz	.Lok
	addq	$8, %rsp
.Lok:
	jmp	.Ldyn_tail
	.size	dyn_jump, .-dyn_jump
	.globl	dyn_owner
	.type	dyn_owner, @function
dyn_owner:                      ;! long(long,long)
	movq	%rdi, %rax
	addq	%rsi, %rax
	nop
.Ldyn_tail:
	addq	$1, %rax
	ret
	.size	dyn_owner, .-dyn_owner
	.section	.note.GNU-stack,"",@progbits
