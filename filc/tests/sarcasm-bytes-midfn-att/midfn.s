# A data-directive run in the MIDDLE of a function body: the decoded
# instructions execute on fallthrough (one path) and a spelled branch skips
# the whole run (the other path) — both control-flow shapes are exercised.
	.text
	.globl	midfn_through
	.type	midfn_through, @function
midfn_through:                  ;! long(long)
	movq	%rdi, %rax
	addq	$1, %rax
	# the run, executed by fallthrough:
	#   48 05 05 00 00 00 = addq $5,%rax
	#   48 83 c0 02       = addq $2,%rax
	.long	0x00050548	# addq $5,%rax (bytes 48 05 05 00)
	.word	0x0000		# ... and its second half
	.long	0x02c08348	# addq $2,%rax (bytes 48 83 c0 02)
	ret
	.size	midfn_through, .-midfn_through
	.globl	midfn_around
	.type	midfn_around, @function
midfn_around:                   ;! long(long)
	movq	%rdi, %rax
	addq	$1, %rax
	jmp	.Lskip_run
	# the same run, SKIPPED by the spelled jmp above: it still must decode
	# (it is inside the body), but nothing executes it
	.long	0x00050548
	.word	0x0000
	.long	0x02c08348
.Lskip_run:
	ret
	.size	midfn_around, .-midfn_around
	.section	.note.GNU-stack,"",@progbits
