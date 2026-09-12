# B2 join into a looping tail (the mont5 .Lsqr4x_sub shape): the shared
# region contains a backward loop whose head precedes the join label, and the
# region's own branches cycle back to it. The clone mirrors the hardware
# control flow exactly (the tail loops in the clone as on hardware), so the
# loop executes in the jumper with shared register state (count in %rdi,
# base in %rsi, accumulator in %rax).
	.text
	.globl	loop_sum
	.type	loop_sum, @function
loop_sum:                       ;! long(long,long)
	# jumper: acc = base, then join the loop tail mid-body.
	movq	%rsi, %rax
	jmp	.Lloop_tail
	.size	loop_sum, .-loop_sum
	.globl	loop_owner
	.type	loop_owner, @function
loop_owner:                     ;! long(long,long)
	# owner entry: same setup, then fall into the shared tail.
	movq	%rsi, %rax
	nop
.Lloop_head:
	testq	%rdi, %rdi
	jz	.Lloop_done
	addq	%rdi, %rax
	decq	%rdi
	jmp	.Lloop_head
.Lloop_tail:
	testq	%rdi, %rdi
	jnz	.Lloop_head
	ret
.Lloop_done:
	ret
	.size	loop_owner, .-loop_owner
	.section	.note.GNU-stack,"",@progbits
