# Duplicate `save capability (buf)`: a name may only be saved once per
# function. Two SEPARATE annotated instructions (two clauses in one
# annotation is its own, different error) each save under the same name;
# the second save is rejected and reports the first save's location.
	.text
	.globl	cap_dup
	.type	cap_dup, @function
cap_dup:                        ;! long(ptr)
	endbr64
	movq	%rdi, %rbx
	movq	%rbx, %rax  #! save capability (buf)
	movq	(%rax), %rax
	movq	%rbx, %rcx  #! save capability (buf)
	movq	(%rcx), %rcx
	ret
	.size	cap_dup, .-cap_dup
	.section	.note.GNU-stack,"",@progbits
