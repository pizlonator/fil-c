	# Test disassembly of illegal instructions.
	.text
	
	# ldpsw   x12, x6, [x6],#-8 ; illegal because one of the dest regs is also the address reg
	.inst 0x68ea18cc

	# illegal, resembles the opcode `ldapur' with invalid qualifier bits
	.inst 0x9dc39839

	# FIXME: Add more illegal instructions here.
