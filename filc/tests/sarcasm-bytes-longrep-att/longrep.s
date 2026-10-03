# `.long 0x9066A4F3` — the OpenSSL perlasm legacy-gas spelling of `rep movsb`
# plus a 2-byte `nop` pad (`66 90`, the `xchg %ax,%ax` nop) — decodes into a
# checked copy plus a nop exactly like the spelled form, in every rep width
# and with the nop pad riding along (padding after the rep must not disturb
# the copy's lowering or the fallthrough).
	.text
	.globl	longrep_movsb
	.type	longrep_movsb, @function
longrep_movsb:                  ;! void(ptr, ptr, size_t)
	movq	%rdx, %rcx
	.long	0x9066A4F3	# rep movsb + 66 90 nop pad
	ret
	.size	longrep_movsb, .-longrep_movsb
	.globl	longrep_movsl
	.type	longrep_movsl, @function
longrep_movsl:                  ;! void(ptr, ptr, size_t)
	# 48 F3 A5 = REX.W + rep + movsd: the qword string copy
	movq	%rdx, %rcx
	.long	0x90A548F3	# rep movsq + 66 90 nop pad
	ret
	.size	longrep_movsl, .-longrep_movsl
	.globl	longrep_stosb
	.type	longrep_stosb, @function
longrep_stosb:                  ;! void(ptr, long)
	# rax is the fill value; rdi the destination: rep stosb fills %rcx bytes
	movq	%rsi, %rax
	movl	$32, %ecx
	.long	0x9066AAF3	# rep stosb + 66 90 nop pad
	ret
	.size	longrep_stosb, .-longrep_stosb
	.section	.note.GNU-stack,"",@progbits
