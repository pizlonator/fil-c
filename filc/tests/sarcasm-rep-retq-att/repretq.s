# AT&T generators that always emit explicit suffixes spell the AMD
# branch-prediction hint `repz retq`/`rep retq` (GNU as accepts both, plus
# `repe retq`) where Intel-flavored output says `rep ret`. Sarcasm accepts the
# retq spellings exactly like `rep ret`: the return transform re-emits a fresh
# prefix-less return, so each of these functions returns identically to a
# plain `retq` (issue #370).
	.text
	.globl	repz_retq_fn
	.type	repz_retq_fn, @function
repz_retq_fn:                   ;! long(long)
	movq	%rdi, %rax
	addq	$17, %rax
	repz	retq
	.size	repz_retq_fn, .-repz_retq_fn
	.globl	rep_retq_fn
	.type	rep_retq_fn, @function
rep_retq_fn:                    ;! long(long)
	movq	%rdi, %rax
	addq	$29, %rax
	rep	retq
	.size	rep_retq_fn, .-rep_retq_fn
	.globl	repe_retq_fn
	.type	repe_retq_fn, @function
repe_retq_fn:                   ;! long(long)
	movq	%rdi, %rax
	addq	$101, %rax
	repe	retq
	.size	repe_retq_fn, .-repe_retq_fn
	.section	.note.GNU-stack,"",@progbits