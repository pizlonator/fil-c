	.text
# Negative proof for the old aesni-mb 4x sliding sink
# (projects/sarcastic-openssl/crypto/aes/asm/aesni-mb-x86_64.pl before the
# fix): `leaq sink(%rip),sink-reg` with NO `sub offset` while the tail
# keeps offset-indexed accesses, so a sunk load reads base+offset with an
# unbounded offset. A 4096-byte sink covers only 256 blocks; this 300-iter
# fully-sunk run reads up to base+4800 and must trap. The fixed form
# (sink=base+16-offset, constant addressing -- see
# sarcasm-mb-sink4x-att) runs any count inside 32 bytes.
	.globl	mbold_run
	.type	mbold_run, @function
mbold_run:                      ;! void(long)
	# %rdi = iteration count. Pure dummy traffic: no live streams.
	leaq	mbold_sink(%rip), %rbp
	xorq	%rbx, %rbx
	pxor	%xmm0, %xmm0
	.align	16
.Lmbold_loop:
	addq	$16, %rbx
	movdqu	(%rbp,%rbx), %xmm0	# sunk load: slides unbounded (the bug)
	movups	%xmm0, -16(%rbp,%rbx)	# sunk store (-16 bias, the .pl shape):
					# slides too, and traps on its own a
					# block later than the load
	decq	%rdi
	jnz	.Lmbold_loop
	ret
	.size	mbold_run, .-mbold_run
	.comm	mbold_sink,4096,16
	.section	.note.GNU-stack,"",@progbits
