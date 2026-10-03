	.file	"granule-ptr.c"
	.text
# A narrow access at a NON-ZERO sub-offset of a granule whose web carries a
# capability (`store ptr` seeded): the extract/merge lowering would rewrite the
# capability's intval bytes as raw integer data — rejected cleanly instead.
# (The aligned (sub == 0) narrow store stays the documented capability kill;
# see sarcasm-slot-storeptr-kill-att.)
	.globl	ptrnarrow
	.type	ptrnarrow, @function
ptrnarrow:                      ;! long(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)     ;! store ptr
	movl	$0, -4(%rbp)       # 4-byte store at sub=4 of the same granule: reject
	movq	-8(%rbp), %rax     ;! load ptr
	movl	(%rax), %eax
	leave
	ret
	.size	ptrnarrow, .-ptrnarrow
	.section	.note.GNU-stack,"",@progbits
