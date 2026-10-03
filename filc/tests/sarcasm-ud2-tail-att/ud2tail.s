# A function whose body ends in `ud2` with NO `ret`: ud2 raises #UD
# unconditionally, so a control-flow path through it has no successors and the
# body cannot fall off its end. Sarcasm treats ud2 as UNCONDITIONALLY TERMINAL
# (validateBody reachability + classify fallsThrough=false), so no trailing
# `ret` is needed — the shape OpenSSL perlasm needs a `.byte 0x0f,0x0b; ret`
# workaround for compiles as-is. Both spellings are covered: the `ud2` mnemonic
# and the `.byte 0x0f,0x0b` encoding, which the byte decoder turns into the
# `ud2` mnemonic before validation. Compile-only: calling either function
# traps (SIGILL), so there is nothing to run.
	.text
	.globl	ud2_tail
	.type	ud2_tail, @function
ud2_tail:                       ;! void(void)
	endbr64
	ud2
	.size	ud2_tail, .-ud2_tail
	.globl	ud2_tail_bytes
	.type	ud2_tail_bytes, @function
ud2_tail_bytes:                 ;! void(void)
	endbr64
	.byte	0x0f,0x0b
	.size	ud2_tail_bytes, .-ud2_tail_bytes
	.section	.note.GNU-stack,"",@progbits
