# B2 clone with a MISALIGNED 8-byte global load (the OPENSSL_ia32cap_P+4
# shape from OpenSSL's perlasm dispatch: an 8-byte load at a 4-byte offset
# inside a global object). Globals key off capabilities, never frame bands —
# the rip-relative operand carries a dispSym, which the band layout skips
# outright — so the clone's misaligned global traffic behaves exactly like
# the jumper's: it succeeds. Executes through the join to prove the clone's
# global load observes the same bytes (a banded global would misaddress and
# return garbage or trap).
	.text
	.globl	gcap_jump
	.type	gcap_jump, @function
gcap_jump:                      ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	jmp	.Lgcap_tail
	.size	gcap_jump, .-gcap_jump
	.globl	gcap_owner
	.type	gcap_owner, @function
gcap_owner:                     ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	nop
.Lgcap_tail:
	movq	OPENSSL_ia32cap_P+4(%rip), %rax
	addq	%rbx, %rax
	addq	%rsi, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	gcap_owner, .-gcap_owner
	.data
	.p2align	2
OPENSSL_ia32cap_P:
	.long	0x11111111
	.long	0x22222222
	.long	0x33333333
	.section	.note.GNU-stack,"",@progbits
