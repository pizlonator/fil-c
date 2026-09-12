	.text
	.globl	flagdf_copy
	.type	flagdf_copy, @function
# cld/std write only the direction flag (DF=0/DF=1) and rep movs*/stos*
# read it: DF is live from the cld into the rep below, across the checked
# load (whose injected sequence must save/restore the flags — the
# pushfq/popfq round-trip preserves DF along with the other six, so the
# copy still runs forward). Both DF writers appear (std then cld, ending
# DF=0) so the test exercises each modeled mnemonic; the std is DCE-kept
# (a DF write with a forward-reachable DF consumer) even though the cld
# overwrites DF, and the checked load proves the DF-liveness save fires
# on the cld/std path exactly as on the stc/adc path. The loaded word is
# returned so every value is observed.
flagdf_copy:                    ;! long(ptr, ptr, long)
	std
	cld
	movq	(%rsi), %r8
	movq	%rdx, %rcx
	rep movsq
	movq	%r8, %rax
	ret
	.size	flagdf_copy, .-flagdf_copy
	.section	.note.GNU-stack,"",@progbits
