	.text

# ---------------------------------------------------------------------------
# pushfq/popfq interleaved with ORDINARY modeled pushes and pops, and nested
# pushfq pairs. The flags word is an ordinary 8-byte stack word in the model
# (a spill-class entry at its depth), so register pushes below it, pops of it,
# and nesting all key the right slot webs.

# pushfq; push %r10; pop %r10; popfq: the flags word sits BELOW %r10's word
# while both are pushed. The restored ZF decides which of two distinct values
# the function returns.
	.globl	pi_interleave
	.type	pi_interleave, @function
pi_interleave:                  ;! long(long)
	movq	%rdi, %rax
	cmpq	$0, %rax
	pushfq
	pushq	%r10
	movq	$55, %r10        # clobber flags; %r10's word is pushed above the flags
	popq	%r10
	popfq
	je	.Lint_zero
	movq	$1, %rax
	ret
.Lint_zero:
	movq	$2, %rax
	ret
	.size	pi_interleave, .-pi_interleave

# Nested pushfq pairs with a flag-writing instruction BETWEEN them, so the
# inner and outer words hold DIFFERENT conditions: the first popfq must
# restore the INNER word (ZF=0) and the second the OUTER word (ZF <- (a==0)).
# If the two words came back swapped, .Lnest_inner reports it.
	.globl	pi_nested
	.type	pi_nested, @function
pi_nested:                      ;! long(long)
	movq	%rdi, %rax
	xorq	%r11, %r11       # r11 = 0 (flags clobbered, dead writes)
	cmpq	$0, %rax
	pushfq                    # outer word: ZF <- (a == 0)
	addq	$5, %r11         # ZF=0, r11=5 (kept: returned)
	pushfq                    # inner word: ZF=0
	addq	$7, %r11         # ZF=0, r11=12 (kept: returned)
	popfq                     # restores the INNER word: ZF=0
	je	.Lnest_inner      # taken only if the WRONG word was restored
	popfq                     # restores the OUTER word: ZF <- (a == 0)
	je	.Lnest_zero
	movq	%r11, %rax
	ret
.Lnest_inner:
	popfq                     # still pop the outer word: the stack stays balanced
	movq	%r11, %rax
	addq	$100, %rax
	ret
.Lnest_zero:
	movq	%r11, %rax
	addq	$200, %rax
	ret
	.size	pi_nested, .-pi_nested

# A deeper interleave: two register spill words pushed OVER the flags word,
# with register-redefining flag clobbers in between; the pops restore the
# registers from their own words (hardware-faithful slot loads) and the popfq
# restores the flags word underneath them.
	.globl	pi_deep
	.type	pi_deep, @function
pi_deep:                        ;! long(long, ptr)
	movq	%rdi, %rax
	cmpq	$0, %rax
	pushfq                    # the flags word
	movq	$77, %r10        # flags clobber (kept: %r10's words hold it)
	pushq	%r10             # word holds 77
	pushq	%r10             # second word holds 77
	popq	%r11             # %r11 <- 77 (an ordinary spill-pop load)
	popq	%r10             # %r10 <- 77
	popfq
	jne	.Ldeep_nonzero
	movq	%r11, %rax
	ret
.Ldeep_nonzero:
	movq	%r11, %rax
	addq	$1000, %rax
	ret
	.size	pi_deep, .-pi_deep

# A pop of the flags word ITSELF into a register (pushfq; popq %r10): the word
# is an ordinary stack word, so popping it into a caller-saved register reads
# exactly the materialized flags; the branch after it reads the still-live
# hardware flags (the pop does not write flags).
	.globl	pi_word_below
	.type	pi_word_below, @function
pi_word_below:                  ;! long(long)
	movq	%rdi, %rax
	cmpq	$0, %rax
	pushfq
	popq	%r10             # %r10 <- the EFLAGS word; the stack is balanced
	andq	$64, %r10        # isolate ZF
	movq	%r10, %rax
	ret
	.size	pi_word_below, .-pi_word_below
	.section	.note.GNU-stack,"",@progbits
