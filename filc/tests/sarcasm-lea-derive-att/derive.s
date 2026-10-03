# Feature (derived stack carriers): a `leaq K(%carrier), %reg` whose base
# register provably holds an unredefined saved-rsp carrier derives a NEW
# carrier parked at the base's depth minus K. The perlasm rolling-cursor
# shape (the sha1-avx2 X[]+K[] window): park an anchor with `leaq N(%rsp)`,
# derive deeper cursors with `leaq M(%anchor)`, and address the frame's
# slots through each derived register. All carriers are dropped and their
# accesses key the same slots the rsp-relative spellings of the same bytes
# would. The epilogue recovers %rsp from the parked pointer.
	.text
	.globl	leaderive
	.type	leaderive, @function
leaderive:                      ;! long(long,long)
	movq	%rsp, %rax		# park the entry rsp
	pushq	%rbx
	pushq	%rbp
	subq	$160, %rsp
	movq	%rax, 144(%rsp)		# slot carrier (the epilogue reloads it)
	movq	%rdi, %rbx
	movq	%rsi, %rbp
	leaq	32(%rsp), %r10		# anchor carrier: entry - (d - 32)
	movq	%rbx, 0(%r10)		# store x through the anchor
	leaq	48(%r10), %r11		# DERIVED carrier: entry - (d - 80)
	movq	%rbp, 16(%r11)		# store y through the derived carrier
	leaq	64(%r11), %r12		# derive again: entry - (d - 144)
	movq	%rbx, 8(%r12)		# store x again through the twice-derived carrier
	movq	0(%r10), %r9		# x (through the anchor)
	addq	16(%r11), %r9		# + y (through the derived carrier)
	addq	8(%r12), %r9		# + x again (through the twice-derived carrier)
	movq	144(%rsp), %rax		# reload the parked rsp (deep depth, same key)
	movq	-16(%rax), %rbp		# restore the pushed saves through the carrier
	movq	-8(%rax), %rbx
	leaq	(%rax), %rsp		# %rsp recovery (dropped)
	movq	%r9, %rax		# redefine %rax with the return value
	ret
	.size	leaderive, .-leaderive
	.section	.note.GNU-stack,"",@progbits
