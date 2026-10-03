# An unconditional-entry loop (`jmp .Ltop` first) whose body ends in `ud2`,
# with nothing after the loop and no `ret` anywhere in the function: no
# control-flow path escapes the loop (the ud2 ends every path), so the body
# cannot fall off its end and compiles with no trailing `ret`. Compile-only:
# entering the loop traps.
	.text
	.globl	ud2_loop_tail
	.type	ud2_loop_tail, @function
ud2_loop_tail:                  ;! void(long)
	endbr64
	jmp	.Ltop
.Ltop:
	ud2
	.size	ud2_loop_tail, .-ud2_loop_tail
	.section	.note.GNU-stack,"",@progbits
