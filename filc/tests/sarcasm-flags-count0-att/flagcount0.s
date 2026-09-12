	.text
	.globl	flagcount0
	.type	flagcount0, @function
# A shift/rotate with a $0 count writes NO flags (SDM: "if the count is 0,
# the flags are not affected") and leaves the destination unchanged. The
# flag model (x86_64_isa isShiftCountZero) treats these forms as
# flag-transparent; the shape below discriminates: `cmpq` sets the program
# condition, the checked load's injected sequence clobbers EFLAGS at
# runtime, and the jg must still see the cmp's flags. The $0 shift AND the
# $0 rotate sit between the check and the branch, so a model that kills
# flags unconditionally (the old behavior) computes the load live-out as
# dead, omits the save/restore, and the jg reads the check's residue (the
# n=10 case then wrongly takes the small path). The shifted register stays
# live into the return so DCE cannot delete the $0 forms and render the
# test vacuous (a deleted shift preserves flags trivially).
flagcount0:                     ;! long(ptr, long)
	cmpq	$5, %rsi
	movq	(%rdi), %rax
	shlq	$0, %rax
	rolq	$0, %rax
	jg	.Lbig
	movl	$0, %eax
	ret
.Lbig:
	addq	$100, %rax
	ret
	.size	flagcount0, .-flagcount0
	.section	.note.GNU-stack,"",@progbits
