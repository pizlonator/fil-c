# Regression test for the depth-0 `and $-N, %rsp` sentinel bug: a prologue
# `and` note whose rsp DEPTH is 0 — nothing pushed or subtracted before it, the
# rsaz-avx2/curve25519-donna prologue idiom `mov %rsp,%r11; and $-32,%rsp;
# subq $N,%rsp`. The keying machinery used frameAndDepth==0 both as "the
# note's rsp depth" and as the "no note" sentinel, so the depth-0 note was
# misread as "no note": the vmovapd cluster was keyed at the SysV entry base
# instead of the and base and placed at the SysV residue, so the emitted
# displacement came out misaligned by a constant (584 ≡ 8 mod 32 under an
# `andq $-32` prologue — a #GP on every call). The note is now tracked by
# frameAndSeen (frameAndDepth only holds its depth), the cluster keys at the
# and base, and the vmovapd lands 32-aligned (displacement 576 ≡ 0 mod 32 in
# the emitted .yolo.s, whose prologue keeps the dynamic `subq`+`andq $-32`).
#
# Vacuous-proof two ways. First, the vmovapd is a REAL materialized aligned
# access: a misaligned vmovapd raises #GP (an uncatchable fault), so any
# regression faults the test instead of silently passing. Second, the C caller
# wraps every call in a swept stack-depth canary wall (see anddepth0-main.c):
# 128 recursion levels each own a verified 256-byte wall segment directly above
# the call's entry rsp, so an escaping cluster corrupts verified bytes.
	.text
	.globl	anddepth0_test
	.type	anddepth0_test, @function
anddepth0_test:                 ;! void(ptr,ptr)
	mov	%rsp, %r11
	and	$-32, %rsp
	subq	$832, %rsp
	movq	%r11, 712(%rsp)
	movq	%rdi, 704(%rsp)
	vmovupd	(%rdi), %ymm0
	vmovapd	%ymm0, 32(%rsp)
	vmovupd	32(%rsp), %ymm1
	vmovupd	%ymm1, 64(%rsp)
	vmovupd	64(%rsp), %ymm2
	vmovupd	%ymm2, (%rsi)
	movq	704(%rsp), %rdi
	movq	712(%rsp), %r11
	mov	%r11, %rsp
	ret
	.size	anddepth0_test, .-anddepth0_test
	.section	.note.GNU-stack,"",@progbits
