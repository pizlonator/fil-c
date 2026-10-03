# UNSOUND: a mem-dest ALU RMW OPENER of the save-slot alias model's POISONED
# WINDOW, with a mid-window read of the pushed register. `pushq %r12` parks the
# seed 900 and the call's outgoing word 0 shares that save slot; the aliased
# access that defines the parked register's web is NOT a mov store this time
# but a READ-MODIFY-WRITE, `addq %rbx, (%rsp)` (rbx = the escaping region
# pointer): the alias model rewrites the access KEEPING ITS MNEMONIC onto the
# pushed register (`addq %rbx, %r12`), so it defines %r12's web with the value
# it computes — 900 + V — exactly like a different-source mov store does. In
# hardware, though, neither the push nor any store to the slot ever modifies
# %r12: the register keeps the pushed 900 until the matching pop reloads the
# slot. So between the aliased RMW and the pop, model and hardware DIVERGE,
# and the mid-body read `movq %r12, %r15` observes the wrong value (native
# returns the pushed 900; sarcasm served 900 + the region pointer — an integer
# where the program has the seed). analyzeFrame's poisoned-window pass opens a
# window on EVERY aliased web-defining access (the exact set the alias model
# rewrites onto the register: dest-first, slot-aligned, full-width, exactly
# classifying, DEFing the saved register — same-register mov stores are the
# one no-op; loads and compares never define the web), so this shape is
# rejected at the RMW that opened the window, fail-closed:
#   sarcasm: stack access overlapping a pushed register's save slot inside the
#   transient prologue pad stores a different value while the pushed register
#   %r12 is still read or written before the matching pop (the save-slot model
#   serves the stored value for register reads in that window, which does not
#   match hardware — the push and the slot stores never modify the register;
#   the statement `movq %r12, %r15` names %r12 inside that window); pop the
#   pad first, place the outgoing store below the push, or don't touch %r12
#   until after the pop: addq %rbx, (%rsp)
# (The message is the SAME one the mov-store opener fires — see
# sarcasm-frame-escape-push-alias-read-reject-att — with only the quoted
# opener at the end differing. The UNARY opener variant — `notq (%rsp)` in
# place of the addq, with a second `notq (%rsp)` restoring the slot for the
# marshal — rejects with the identical message (opener quoted `notq (%rsp)`),
# and so does the variant WITHOUT the D9 promotion (no escaping lea, heap-only
# arguments): the poisoned window is the alias model's own and not a
# region-promotion artifact.)
	.text
	.globl	rmwread
	.type	rmwread, @function
rmwread:                        #! long(ptr,ptr,long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$104, %rsp          # D0; region [16,104)

	movq	%rdx, %r12          # park the seed 900
	movq	%rdi, %r13          # bufA
	movq	%rsi, %r14          # bufB

	leaq	16(%rsp), %rax      # the ESCAPING lea (region+0)
	movq	%rax, %rbx          # rbx = region pointer V

	movq	$940, 48(%rsp)
	movq	$950, 56(%rsp)

	pushq	%r12                # save slot IS outgoing word 0; hw r12 = 900
	addq	%rbx, (%rsp)        # ALIASED RMW: slot = 900+V; model defines web(r12) = 900+V
	movq	%r12, %r15          # MID-WINDOW READ: native r15 = 900; model serves 900+V
	movq	%r14, %rdi          # arg1 = bufB
	movq	%r13, %rsi          # arg2 = bufA
	movq	%rbx, %rdx          # arg3 = region+0
	leaq	24(%rsp), %rcx      # arg4 = region+0
	leaq	32(%rsp), %r8       # arg5 = region+8
	leaq	40(%rsp), %r9       # arg6 = region+16
	movq	%r14, 8(%rsp)       # outgoing word 1 = bufB
	                   	    # outgoing word 0 (arg7) = the slot content itself (no store)
	call	sink8 ;! long(ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr)

	popq	%r12                # dropped pop

	movq	%r15, %rax          # return r15
	addq	$104, %rsp
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	rmwread, .-rmwread

	.globl	sink8
	.type	sink8, @function
sink8:                          #! long(ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr)
	movq	$604, %rax
	ret
	.size	sink8, .-sink8
	.section	.note.GNU-stack,"",@progbits
