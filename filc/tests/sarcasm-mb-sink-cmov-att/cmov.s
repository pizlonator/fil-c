	.text
# Unit proof for the aesni-mb 4x/8x input/output sink-cancel conditions
# (projects/sarcastic-openssl/crypto/aes/asm/aesni-mb-x86_64.pl), in the exact
# AT&T operand order the .pl emits (`cmpl counter,one` computes one-counter,
# so cmovge fires for counter<=1 and cmovg for counter==0 on non-negative
# counts):
#
# - loop input cancel  (`cmovge sink,inptr`): sunk while counter<=1, i.e. on
#   the last live block already, so the next-block tail load
#   ((@inptr,$offset)) never reads one-past-the-end live input;
# - loop output cancel (`cmovg sink,outptr`): sunk only once counter==0, so
#   the last live store (-16(@outptr,$offset)) still lands in the live
#   output;
# - prologue empty-stream cancel (`test count,count; cmovle sink,inptr`):
#   sunk only for count==0.
#
# The last-block asymmetry (input a block early, output exactly at
# exhaustion) is what makes sharing one sink object for inputs and outputs
# safe: a dummy load never touches a live output.
	.globl	mb_cmov_in_sunk
	.type	mb_cmov_in_sunk, @function
mb_cmov_in_sunk:                ;! long(long)
	# %rdi = counter. Returns 1 if the loop input cmov would sink it.
	leaq	mb_cmov_sink(%rip), %rax
	leaq	mb_cmov_live(%rip), %rcx
	movl	%edi, %edx
	movl	$1, %esi
	cmpl	%edx, %esi
	cmovge	%rax, %rcx
	cmpq	%rax, %rcx
	sete	%al
	movzbq	%al, %rax
	ret
	.size	mb_cmov_in_sunk, .-mb_cmov_in_sunk
	.globl	mb_cmov_out_sunk
	.type	mb_cmov_out_sunk, @function
mb_cmov_out_sunk:               ;! long(long)
	# %rdi = counter. Returns 1 if the loop output cmov would sink it.
	leaq	mb_cmov_sink(%rip), %rax
	leaq	mb_cmov_live(%rip), %rcx
	movl	%edi, %edx
	movl	$1, %esi
	cmpl	%edx, %esi
	cmovg	%rax, %rcx
	cmpq	%rax, %rcx
	sete	%al
	movzbq	%al, %rax
	ret
	.size	mb_cmov_out_sunk, .-mb_cmov_out_sunk
	.globl	mb_cmov_prologue_sunk
	.type	mb_cmov_prologue_sunk, @function
mb_cmov_prologue_sunk:          ;! long(long)
	# %rdi = block count. Returns 1 if the prologue cancel sinks it.
	leaq	mb_cmov_sink(%rip), %rax
	leaq	mb_cmov_live(%rip), %rcx
	movl	%edi, %edx
	testl	%edx, %edx
	cmovle	%rax, %rcx
	cmpq	%rax, %rcx
	sete	%al
	movzbq	%al, %rax
	ret
	.size	mb_cmov_prologue_sunk, .-mb_cmov_prologue_sunk
	.comm	mb_cmov_sink,32,16
	.comm	mb_cmov_live,32,16
	.section	.note.GNU-stack,"",@progbits
