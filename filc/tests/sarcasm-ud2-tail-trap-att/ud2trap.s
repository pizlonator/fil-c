# The `.byte 0x0f,0x0b` spelling of ud2 is decoded to the `ud2` mnemonic before
# validation (x86_64_parse BYTE_DECODE), so it gets the same terminal semantics
# — and at runtime it really is a ud2: executing it raises #UD and the process
# dies with SIGILL. The body compiles with no trailing `ret`.
	.text
	.globl	ud2_tail_trap
	.type	ud2_tail_trap, @function
ud2_tail_trap:                  ;! void(void)
	endbr64
	.byte	0x0f,0x0b
	.size	ud2_tail_trap, .-ud2_tail_trap
	.section	.note.GNU-stack,"",@progbits
