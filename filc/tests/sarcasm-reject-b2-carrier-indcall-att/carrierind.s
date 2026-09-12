# Indirect call through a clone carrier (fail-closed): `call *%rax` reads
# %rax, which holds the dropped prologue save — observing the phantom value
# is a static error at the call itself (the use-check runs on the incoming
# carrier map before the call's caller-saved poisoning takes effect, so the
# error names the call rather than silently poisoning).
	.text
	.globl	cind_jump
	.type	cind_jump, @function
cind_jump:                      ;! long(long)
	movq	%rdi, %rbx
	jmp	.Lcind_tail
	.size	cind_jump, .-cind_jump
	.globl	cind_owner
	.type	cind_owner, @function
cind_owner:                     ;! long(long)
	movq	%rdi, %rbx
	nop
.Lcind_tail:
	movq	%rsp, %rax
	call	*%rax            ;! long(long)
	movq	%rbx, %rax
	ret
	.size	cind_owner, .-cind_owner
	.section	.note.GNU-stack,"",@progbits
