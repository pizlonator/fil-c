	.text

# ---------------------------------------------------------------------------
# The flags word lives ACROSS a call: the callee allocates/frees in a loop so
# a GC pass is plausible while the word is parked, and the callee clobbers
# every caller-saved register and flag. The popfq must restore the caller's
# pre-call condition — which also proves the GC never scanned the parked word
# as a pointer (a garbage pointer in a scanned slot would crash or corrupt the
# heap; here it is never scanned at all: the word is typed integer).

	.globl	pg_churn
	.type	pg_churn, @function
pg_churn:                       ;! void(long)
	# allocate and drop n blocks to give the GC a chance to run (n parks in
	# the callee-saved %rbx: the calls clobber every caller-saved register)
	movq	%rdi, %rbx
	xorq	%r10, %r10
.Lpg_loop:
	cmpq	%rbx, %r10
	jae	.Lpg_done
	movq	$8192, %rdi
	call	malloc            ;! ptr(long)
	movq	%rax, %rdi
	call	free              ;! void(ptr)
	addq	$1, %r10
	jmp	.Lpg_loop
.Lpg_done:
	ret
	.size	pg_churn, .-pg_churn

	.globl	pg_clobber
	.type	pg_clobber, @function
pg_clobber:                     ;! void()
	# clobber every caller-saved GPR and the flags (kept: %rax returned to an
	# annotated caller is not modeled here, so the stores make the defs live)
	movq	$-1, %rax
	movq	$-2, %rcx
	movq	$-3, %rdx
	movq	$-4, %rsi
	movq	$-5, %rdi
	movq	$-6, %r8
	movq	$-7, %r9
	movq	$-8, %r10
	movq	$-9, %r11
	ret
	.size	pg_clobber, .-pg_clobber

# pushfq; call pg_churn; popfq; jcc — the word survives the call (and any GC
# it triggers), and the flags the branch reads are the caller's pre-call ones.
	.globl	pg_caller
	.type	pg_caller, @function
pg_caller:                      ;! long(long, long)
	movq	%rdi, %rax
	cmpq	$0, %rax
	pushfq
	movq	%rsi, %rdi
	call	pg_churn          ;! void(long)
	popfq
	je	.Lcg_zero
	movq	$1, %rax
	ret
.Lcg_zero:
	movq	$2, %rax
	ret
	.size	pg_caller, .-pg_caller

# Same shape with a register-carrying round trip: the word parks in a
# caller-saved register that the callee clobbers, so the model must keep it
# somewhere the callee cannot touch (a callee-saved register or a spill slot).
	.globl	pg_caller_reg
	.type	pg_caller_reg, @function
pg_caller_reg:                  ;! long(long, long)
	movq	%rdi, %rax
	cmpq	$0, %rax
	pushfq
	popq	%r10
	movq	%rsi, %rdi
	call	pg_clobber        ;! void()
	call	pg_churn          ;! void(long)
	pushq	%r10
	popfq
	je	.Lcgr_zero
	movq	$1, %rax
	ret
.Lcgr_zero:
	movq	$2, %rax
	ret
	.size	pg_caller_reg, .-pg_caller_reg

# Two pushed words in one frame with DIFFERENT conditions: the first popfq
# must restore the inner word (ZF <- (a == 5)) and the second the outer word
# (ZF <- (a == 0)).
	.globl	pg_nested_call
	.type	pg_nested_call, @function
pg_nested_call:                 ;! long(long)
	movq	%rdi, %rax
	cmpq	$0, %rax
	pushfq                    # word 1: ZF <- (a == 0)
	cmpq	$5, %rax
	pushfq                    # word 2: ZF <- (a == 5)
	popfq                     # restores word 2: ZF <- (a == 5)
	je	.Lnc_five
	popfq                     # restores word 1: ZF <- (a == 0)
	je	.Lnc_zero
	movq	$1, %rax
	ret
.Lnc_five:
	popfq                     # pop word 1 so the stack stays balanced
	movq	$3, %rax
	ret
.Lnc_zero:
	movq	$2, %rax
	ret
	.size	pg_nested_call, .-pg_nested_call
	.section	.note.GNU-stack,"",@progbits
