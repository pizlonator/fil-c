	.text

# The pristine wrap must round-trip the ENTRY DF: this helper is entered with
# DF=1 (the std below stands in for an ABI-violating caller), pushfq saves
# ALL the flags (DF=1 with them - BEFORE the cld runs), the cld makes the
# first checked rep movsb run forward (no trap, no backward copy), and the
# popfq restores DF=1. A SECOND checked rep after the restore then observes
# the restored DF=1 with a nonzero count, which the checked lowering traps
# with ud2 (SIGILL) - the observable proof that the word came back exactly as
# it was saved. (If the pushfq had captured the post-cld flags, or the popfq
# failed to restore them, the second rep would run forward and print NOT
# REACHED instead of trapping; the buffers are valid, so the only way to die
# here is the DF trap.)

	.globl	pd_wrap
	.type	pd_wrap, @function
pd_wrap:                        ;! void(ptr, ptr, size_t)
	std                       # DF=1 (the ABI-violating entry condition)
	pushfq                    # save ALL flags (DF=1) - before the cld
	cld                       # DF=0: the checked rep below must run forward
	movq	%rdx, %rcx
	rep movsb                 # forward copy: no trap
	popfq                     # restore DF=1
	movq	%rdx, %rcx
	rep movsb                 # DF=1 with a nonzero count: the checked rep traps
	ret
	.size	pd_wrap, .-pd_wrap
	.section	.note.GNU-stack,"",@progbits

