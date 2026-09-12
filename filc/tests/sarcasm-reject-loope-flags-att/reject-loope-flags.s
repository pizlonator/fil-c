# ORDERING PROOF (reject precedes DCE and the flag save-scan): the `loope`
# below READS ZF produced by the `cmpq`, so the flags are live across the
# checked `movq (%rdi), %r8` load between them — the injected bounds check
# for that load must be bracketed with a flag save/restore (the same
# coarse flagUse/flagDef + flagUseMask/flagDefMask predicates drive both
# DCE's FLAGS liveness and transform.luau's flagsLiveFrom save-scan).
#
# If rejection did NOT run first — inside x86.classify, which
# lift.liftFunction invokes for every instruction strictly before
# transform.transformFunction's DCE rounds and before any flagsLiveFrom
# save-scan use — then DCE and the save-scan would both have to process
# this loope: DCE through its FLAGS pseudo-temp liveness (loope is a
# flagUse without a flagDef, so it keeps the cmp's flag write alive and
# pins the save bracket), and the save-scan through the ZF read. Either a
# successful compilation (save elided or emitted around a surviving loope)
# or a backend crash would result. This test demands compileFailure, so a
# green run pins that the clean `sarcasm:` rejection fires before either
# flag consumer ever sees the instruction. The `cmc` up front pins the
# other half of the coarse-table fix: cmc reads CF (SUPPORTED, not
# rejected) and assembles fine — only the ZF-consuming loop variant fails.
	.text
	.globl	loope_flags_order
	.type	loope_flags_order, @function
loope_flags_order:              ;! long(ptr, long)
	endbr64
	cmc
	cmpq	$5, %rsi
	movq	(%rdi), %r8
	loope	.Ldone
	movq	%r8, %rax
	ret
.Ldone:
	movq	$1, %rax
	ret
	.size	loope_flags_order, .-loope_flags_order
	.section	.note.GNU-stack,"",@progbits
