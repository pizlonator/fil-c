# A local subroutine whose `ret` sits at an unprovable ("dyn") stack-pointer
# depth is a static error: two paths adjust rsp by different amounts
# (`subq $72, %rsp` vs `subq $80, %rsp`) and merge before `ret`, so no static
# depth can be proven for the return. The clone's frame is dropped (the real
# rsp never moves), so every localRet must sit at exactly its clone entry's
# depth — the continuation resumes at the caller's depth.
	.text
	.globl	subdyn_main
	.type	subdyn_main, @function
subdyn_main:                    ;! long(long)
	pushq	%rbx
	subq	$32, %rsp
	movq	%rdi, 0(%rsp)
	call	subdyn
	movq	0(%rsp), %rax
	addq	$32, %rsp
	popq	%rbx
	ret
	.size	subdyn_main, .-subdyn_main
	.type	subdyn, @function
subdyn:
	movq	8(%rsp), %rax
	testq	%rax, %rax
	je	.Ldyn_b
	subq	$72, %rsp
	jmp	.Ldyn_ret
.Ldyn_b:
	subq	$80, %rsp
.Ldyn_ret:
	ret
	.size	subdyn, .-subdyn
	.section	.note.GNU-stack,"",@progbits
