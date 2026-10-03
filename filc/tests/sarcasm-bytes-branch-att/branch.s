# A decoded BRANCH inside a data run: 74 05 (je +5) branches to a synthetic
# local label sarcasm plants at the target instruction boundary INSIDE the
# run; the fallthrough path executes the skipped instructions. eb = jmp +N.
	.text
	.globl	branch_je
	.type	branch_je, @function
branch_je:                      ;! long(long)
	movl	%edi, %eax
	testl	%eax, %eax
	# 74 05      je +5   -> lands on the in-run ret
	# 83 c0 0a   addl $10,%eax
	# ff c0      inc %eax
	# c3         ret
	.byte	0x74,0x05,0x83,0xc0,0x0a,0xff,0xc0,0xc3
	.size	branch_je, .-branch_je
	.globl	branch_jmp
	.type	branch_jmp, @function
branch_jmp:                     ;! long(long)
	movl	%edi, %eax
	# eb 05      jmp +5  -> unconditional: skips addl $10 and inc
	# 83 c0 0a   addl $10,%eax
	# ff c0      inc %eax
	# c3         ret
	.byte	0xeb,0x05,0x83,0xc0,0x0a,0xff,0xc0,0xc3
	.size	branch_jmp, .-branch_jmp
	.section	.note.GNU-stack,"",@progbits
