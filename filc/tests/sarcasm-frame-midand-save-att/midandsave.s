# Mid-dispatch frames with a saved-rsp carrier crossing a mid-function
# `and $-N, %rsp`: the bulk path is entered via a conditional jump (so the
# prologue prefix ends at the early `ret` and the `and` is a mid-function
# note), the entry rsp is parked in %r11 before the pushes/sub/and and used
# after it ONLY for the epilogue restore (`-8(%r11)`) and the rsp recovery.
# Restores do not virtualize slot webs (they are dropped), and their save
# slot is at an absolute entry-relative address the and slack cannot shift,
# so the crossing is sound and must be accepted (regression test for the
# aesni-xts/ctr32 `and $-16` and aesni-sha256 `and $-64` shapes, which park
# entry rsp in %r11/%rax/%rsi before the frame and recover through it).
# Four variants share one sweep harness: `and $-32` with lea recovery (the
# base shape), `and $-16` and `and $-64` (the real xts/sha256 widths), and an
# `and $-32` shape whose teardown recovers with a constant `add` plus the
# paired pop (the sound add form — the add is a verified teardown start, so
# it is dropped exactly like the lea recovery).
	.text
	.globl	midandsave_test
	.type	midandsave_test, @function
midandsave_test:                ;! long(long,long)
	cmpq	$0, %rdi
	jne	.Lbulk
	movq	%rdi, %rax
	ret
.Lbulk:
	leaq	(%rsp), %r11
	pushq	%rbp
	subq	$64, %rsp
	and	$-32, %rsp
	movq	%rdi, 0(%rsp)
	movq	%rsi, 8(%rsp)
	movq	0(%rsp), %rax
	addq	8(%rsp), %rax
	movq	-8(%r11), %rbp
	leaq	(%r11), %rsp
	ret
	.size	midandsave_test, .-midandsave_test
	.globl	midandsave16_test
	.type	midandsave16_test, @function
midandsave16_test:              ;! long(long,long)
	cmpq	$0, %rdi
	jne	.Lbulk16
	movq	%rdi, %rax
	ret
.Lbulk16:
	leaq	(%rsp), %r11
	pushq	%rbp
	subq	$64, %rsp
	and	$-16, %rsp
	movq	%rdi, 0(%rsp)
	movq	%rsi, 8(%rsp)
	movq	0(%rsp), %rax
	addq	8(%rsp), %rax
	movq	-8(%r11), %rbp
	leaq	(%r11), %rsp
	ret
	.size	midandsave16_test, .-midandsave16_test
	.globl	midandsave64_test
	.type	midandsave64_test, @function
midandsave64_test:              ;! long(long,long)
	cmpq	$0, %rdi
	jne	.Lbulk64
	movq	%rdi, %rax
	ret
.Lbulk64:
	leaq	(%rsp), %r11
	pushq	%rbp
	subq	$64, %rsp
	and	$-64, %rsp
	movq	%rdi, 0(%rsp)
	movq	%rsi, 8(%rsp)
	movq	0(%rsp), %rax
	addq	8(%rsp), %rax
	movq	-8(%r11), %rbp
	leaq	(%r11), %rsp
	ret
	.size	midandsave64_test, .-midandsave64_test
	.globl	midandsaveadd_test
	.type	midandsaveadd_test, @function
midandsaveadd_test:             ;! long(long,long)
	cmpq	$0, %rdi
	jne	.Lbulkadd
	movq	%rdi, %rax
	ret
.Lbulkadd:
	leaq	(%rsp), %r11
	pushq	%rbp
	subq	$64, %rsp
	and	$-32, %rsp
	movq	%rdi, 0(%rsp)
	movq	%rsi, 8(%rsp)
	movq	0(%rsp), %rax
	addq	8(%rsp), %rax
	movq	-8(%r11), %rbp
	addq	$64, %rsp
	popq	%rbp
	ret
	.size	midandsaveadd_test, .-midandsaveadd_test
	.section	.note.GNU-stack,"",@progbits
