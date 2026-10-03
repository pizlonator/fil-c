# UNSOUND: a mid-body read of a pushed register inside the save-slot alias
# model's POISONED WINDOW. `pushq %r12` parks the seed 900 and the call's
# outgoing word 0 shares that save slot; the outgoing store
# (`movq %rbx, (%rsp)`) writes a DIFFERENT source — the region pointer — so
# the alias model defines %r12's web with the STORED value (the dropped pop
# must restore what the slot holds, so the model lets each aliased full-width
# store redefine the pushed register's web). In hardware, though, neither the
# push nor any store to the slot ever modifies %r12: the register keeps the
# pushed 900 until the matching pop reloads the slot. So between the aliased
# different-source store and the pop, model and hardware DIVERGE, and the
# mid-body read `movq %r12, %r15` observes the wrong value (native returns the
# pushed 900; sarcasm served the stored region pointer — a pointer where the
# program has an integer). analyzeFrame rejects the shape at the store that
# opened the window, fail-closed (pop the pad first, place the outgoing store
# below the push, or don't touch %r12 until after the pop — reading %r15 after
# the pop is exact, which is the working sarcasm-frame-escape-push-alias-*-att
# discipline):
#   sarcasm: stack access overlapping a pushed register's save slot inside the
#   transient prologue pad stores a different value while the pushed register
#   %r12 is still read or written before the matching pop (the save-slot model
#   serves the stored value for register reads in that window, which does not
#   match hardware — the push and the slot stores never modify the register;
#   the statement `movq %r12, %r15` names %r12 inside that window); pop the
#   pad first, place the outgoing store below the push, or don't touch %r12
#   until after the pop: movq %rbx, (%rsp)
# (The same rejection fires WITHOUT the D9 promotion: the no-escape variant of
# this shape — no escaping lea, heap-only arguments, `movq %r13, (%rsp)` as
# the aliased store — rejects with the identical message, so the poisoned
# window is the alias model's own and not a region-promotion artifact.)
	.text
	.globl	pushread
	.type	pushread, @function
pushread:                       #! long(ptr,ptr,long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$104, %rsp          # D0 = 152; outgoing band [0,16), region [16,104)

	movq	%rdx, %r12          # park the seed 900
	movq	%rdi, %r13          # park bufA
	movq	%rsi, %r14          # park bufB

	leaq	16(%rsp), %rax      # region+0 — the ESCAPING lea
	movq	%rax, %rbx          # park region+0 (call-safe)

	movq	$940, 48(%rsp)      # region+32
	movq	$950, 56(%rsp)      # region+40

	pushq	%r12                # parks the seed; save slot IS outgoing word 0
	movq	%r14, %rdi          # arg1 = bufB
	movq	%r13, %rsi          # arg2 = bufA
	movq	%rbx, %rdx          # arg3 = region+0
	leaq	24(%rsp), %rcx      # arg4 = region+0 (shifted spelling)
	leaq	32(%rsp), %r8       # arg5 = region+8
	leaq	40(%rsp), %r9       # arg6 = region+16
	movq	%rbx, (%rsp)        # outgoing word 0 (arg7): the REGION POINTER —
	                            # a DIFFERENT source: the poisoned window opens
	movq	%r12, %r15          # MID-BODY READ of the pushed register — hw: 900
	movq	%r14, 8(%rsp)       # outgoing word 1 (arg8): bufB
	call	sink8 ;! long(ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr)

	popq	%r12                # dropped pop — the window closes here

	movq	%r15, %rax
	addq	$104, %rsp
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	pushread, .-pushread
	.section	.note.GNU-stack,"",@progbits
