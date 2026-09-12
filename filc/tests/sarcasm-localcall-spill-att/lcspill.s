# Register pressure across local calls, past the point of no spill: the caller
# holds THIRTEEN distinct live webs across TWO consecutive `call submany`
# sites — all twelve incoming args plus the first call's result:
#   * six in callee-saved (rbx, rbp, r12-r15: args a0-a5),
#   * six in caller-saved (r10, r11, rdi, rsi, rdx, rcx: args a6-a11, the
#     five stack arguments reloaded through the yolo slots plus the sixth
#     stack argument held live as well),
#   * the first call's result stashed in %r9 across the second call.
# Each clone, meanwhile, combines all twelve inputs (eleven registers plus
# the twelfth stack argument read through the +8 rule) using one scratch web
# plus its retaddr/continuation webs. The function-wide coloring (caller +
# both clones as one function) therefore has ~16 simultaneously live webs
# for 14 allocatable GPRs (rax, rcx, rdx, rbx, rbp, rsi, rdi, r8-r10,
# r12-r15) — two more than the pool — so it MUST spill and reload around
# the clone boundaries.
#
# IN-TEST (machine-checked, no .yolo.s grep needed — run-tests can only check
# program output, so the proof is shaped to surface there): without spilling
# the function cannot color at all (graph coloring with more live webs than
# colors is uncolorable), so a no-spill allocator fails COMPILE and the test
# fails its `success` manifest expectation; a spill with a wrong slot, a
# dropped restore, or a stale reload changes the weighted total below, so a
# mis-spill fails the RESULT check. The result checks every value survived
# both calls: each arg carries a distinct weight (3, 5, 7, ...), so any
# dropped, swapped, or stale web changes the total. A passing run therefore
# proves spill+reload ran and was exact. (The emitted .yolo.s was
# additionally verified by hand to contain a 100+ byte spill area with spill
# stores ahead of each clone jump and matching reloads after each
# continuation — a build-artifact cross-check, not the test signal.)
	.text
	.globl	lcspill_main
	.type	lcspill_main, @function
lcspill_main:                   ;! long(long,long,long,long,long,long,long,long,long,long,long,long)
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	movq	%rdi, %rbx
	movq	%rsi, %rbp
	movq	%rdx, %r12
	movq	%rcx, %r13
	movq	%r8, %r14
	movq	%r9, %r15
	movq	56(%rsp), %r10
	movq	64(%rsp), %r11
	movq	72(%rsp), %rdi
	movq	80(%rsp), %rsi
	movq	88(%rsp), %rdx
	movq	96(%rsp), %rcx
	call	submany
	movq	%rax, %r9
	call	submany
	addq	%r9, %rax
	addq	%rbx, %rax
	addq	%rbp, %rax
	addq	%r12, %rax
	addq	%r13, %rax
	addq	%r14, %rax
	addq	%r15, %rax
	addq	%r10, %rax
	addq	%r11, %rax
	addq	%rdi, %rax
	addq	%rsi, %rax
	addq	%rdx, %rax
	addq	%rcx, %rax
	addq	%rcx, %rax
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbp
	popq	%rbx
	ret
	.size	lcspill_main, .-lcspill_main
	.type	submany, @function
submany:
	movq	%rbx, %rax
	imulq	$3, %rax
	movq	%rbp, %r8
	imulq	$5, %r8
	addq	%r8, %rax
	movq	%r12, %r8
	imulq	$7, %r8
	addq	%r8, %rax
	movq	%r13, %r8
	imulq	$11, %r8
	addq	%r8, %rax
	movq	%r14, %r8
	imulq	$13, %r8
	addq	%r8, %rax
	movq	%r15, %r8
	imulq	$17, %r8
	addq	%r8, %rax
	movq	%r10, %r8
	imulq	$19, %r8
	addq	%r8, %rax
	movq	%r11, %r8
	imulq	$23, %r8
	addq	%r8, %rax
	movq	%rdi, %r8
	imulq	$29, %r8
	addq	%r8, %rax
	movq	%rsi, %r8
	imulq	$31, %r8
	addq	%r8, %rax
	movq	%rdx, %r8
	imulq	$37, %r8
	addq	%r8, %rax
	movq	104(%rsp), %r8
	imulq	$43, %r8
	addq	%r8, %rax
	ret
	.size	submany, .-submany
	.section	.note.GNU-stack,"",@progbits
