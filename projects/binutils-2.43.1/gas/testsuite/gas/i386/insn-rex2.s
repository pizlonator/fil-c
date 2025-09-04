	.text
insn_rex2:
	.insn {rex2} 0x0f01/0, %eax
	.insn {rex2} 0x0f38/0, %eax
	.insn {rex2} 0x0f3801/0, %eax
	.insn {rex2} 0x0f3901/0, %eax
	.insn {rex2} 0x0f3a01/0, $0xCC, %eax

	.insn {rex2} 0x0f01/0, %rax
	.insn {rex2} 0x0f38/0, %rax
	.insn {rex2} 0x0f3801/0, %rax
	.insn {rex2} 0x0f3901/0, %rax
	.insn {rex2} 0x0f3a01/0, $0xCC, %rax

	.insn {rex2} 0x0f01/0, %r8
	.insn {rex2} 0x0f38/0, %r8
	.insn {rex2} 0x0f3801/0, %r8
	.insn {rex2} 0x0f3901/0, %r8
	.insn {rex2} 0x0f3a01/0, $0xCC, %r8

	.insn 0x0f01/0, %r16
	.insn 0x0f38/0, %r16
	.insn 0x0f3801/0, %r16
	.insn 0x0f3901/0, %r16
	.insn 0x0f3a01/0, $0xCC, %r16
