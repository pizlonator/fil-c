	.file	"cmll-df.c"
	.text

	# Self-contained DF torture for sarcasm's checked rep lowering:
	# cmll_df1_copy models the residue-tail shape of a block
	# cipher CBC tail — full 16-byte blocks are copied with DF-insensitive
	# movdqu, and only the length residue (len & 15) goes through a
	# CLD-less `rep movsb`. The helper enters with DF=1 set by the `std`
	# below, standing in for an ABI-violating caller (System V requires
	# DF=0 across calls and sarcasm never emits std). The checked rep
	# traps DF=1 with an illegal instruction when the count is nonzero.
	# The `cld` on the surviving paths is test hygiene so the harness
	# keeps running with DF=0 after a surviving call.
	#
	# Residue analysis (which lengths trap — pinned by cmll-df-main.c):
	# - len == 0 takes the abort branch up front and never reaches a rep:
	#   no trap even with DF=1, dst untouched.
	# - len == 16: residue = 16&15 = 0, so the block loop runs once and
	#   the tail is skipped: no rep executes, no trap, bytes copied.
	#   (Only a NONZERO residue reaches a rep; multiples of 16 never
	#   trap, e.g. len=32.)
	# - len == 8: full = 0, so the block loop is skipped and the tail is
	#   entered with residue == len == 8 (nonzero): the checked rep
	#   observes DF=1 and traps. Any len with len&15 != 0 traps the same
	#   way.
	# The DF=0 forward-copy path is covered by cmll_df0_copy below (same
	# cld-less rep shape as sarcasm-rep-movs-att) and by the DF=0
	# copy round-trip in cmll-df-main.c.
	.globl	cmll_df0_copy
	.type	cmll_df0_copy, @function
cmll_df0_copy:                  ;! void(ptr, ptr, size_t)
	# %rdi=dst %rsi=src %rdx=len: plain forward copy, DF=0.
	movq	%rdx, %rcx
	rep movsb
	ret
	.size	cmll_df0_copy, .-cmll_df0_copy
	.globl	cmll_df1_copy
	.type	cmll_df1_copy, @function
cmll_df1_copy:                  ;! void(ptr, ptr, size_t)
	# %rdi=dst %rsi=src %rdx=len. The address marshalling below (plain
	# moves) preserves DF from the std to the checked rep.
	std
	testq	%rdx, %rdx
	jz	.Ldf1_abort
	movq	%rdx, %r8
	andq	$-16, %r8
	movq	%rdx, %rcx
	andq	$15, %rcx
	testq	%r8, %r8
	jz	.Ldf1_tail
	xorq	%r9, %r9
.Ldf1_loop:
	movdqu	(%rsi,%r9), %xmm0
	movdqu	%xmm0, (%rdi,%r9)
	addq	$16, %r9
	cmpq	%r8, %r9
	jb	.Ldf1_loop
.Ldf1_tail:
	testq	%rcx, %rcx
	jz	.Ldf1_done
	addq	%r8, %rsi
	addq	%r8, %rdi
	rep movsb
.Ldf1_done:
	cld
	ret
.Ldf1_abort:
	cld
	ret
	.size	cmll_df1_copy, .-cmll_df1_copy
	.section	.note.GNU-stack,"",@progbits
