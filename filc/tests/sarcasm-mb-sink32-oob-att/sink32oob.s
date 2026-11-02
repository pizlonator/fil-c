	.text
# Negative proof that a 32-byte sink REQUIRES the fixed constant-address
# form (projects/sarcastic-openssl/crypto/aes/asm/aesni-mb-x86_64.pl): the
# same sliding shape as sarcasm-mb-sink-slide-oob-att, but against a
# 32-byte object and in the store direction. Iteration 0 (offset 16)
# stores at base+0, in bounds; iteration 2 (offset 48) stores at
# base+32, one past the end, and must trap. (The companion load direction
# is covered by sarcasm-mb-sink-slide-oob-att.) The fixed
# sink=base+16-offset form (sarcasm-mb-sink4x-att) keeps every sunk store
# at base for any iteration count.
	.globl	mb32old_run
	.type	mb32old_run, @function
mb32old_run:                    ;! void(long)
	# %rdi = iteration count. Pure dummy traffic: no live streams.
	leaq	mb32old_sink(%rip), %rbp
	xorq	%rbx, %rbx
	pxor	%xmm0, %xmm0
	.align	16
.Lmb32old_loop:
	addq	$16, %rbx
	movups	%xmm0, -16(%rbp,%rbx)	# sunk store (slides: iter 0 writes
					# [base,base+16), iter 1 writes
					# [base+16,base+32), iter 2 writes
					# [base+32,base+48) and must trap)
	decq	%rdi
	jnz	.Lmb32old_loop
	ret
	.size	mb32old_run, .-mb32old_run
	.comm	mb32old_sink,32,16
	.section	.note.GNU-stack,"",@progbits
