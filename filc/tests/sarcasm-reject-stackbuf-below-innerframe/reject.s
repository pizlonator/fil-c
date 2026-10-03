	.text
# The buffer declaration reaches BELOW the declaring statement's own
# allocated stack. The leading half owns the prologue scan, so the second
# half's push+sub keys as a mid-function inner frame: at the declaring
# statement the provable allocation covers normalized [-32, 0) only, but the
# declared range asks for [-48, -16) — 16 bytes below the floor. Rejected
# (bytes below the inner frame's bottom were never allocated on this path).
	.globl	bad
	.type	bad, @function
bad:                            ;! void(ptr, size_t)
	testl	%esi, %esi
	jz	.Lret
	subq	$16, %rsp
	movl	%esi, (%rsp)
	call	ext ;! void()
	addq	$16, %rsp
	pushq	%rbp
	subq	$32, %rsp
	xorl	%eax, %eax
	movl	(%rsp,%rdi), %eax #! stack buffer (b, %rsp - 48, %rsp + 16)
	addq	$32, %rsp
	popq	%rbp
.Lret:
	ret
	.size	bad, .-bad

	.globl	ext
	.type	ext, @function
ext:                            ;! void()
	ret
	.size	ext, .-ext
	.section	.note.GNU-stack,"",@progbits
