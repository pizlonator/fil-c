	.text
	.globl	confstack
	.type	confstack, @function
# Stack-form access through a conflicted base must trap: on one path %rax
# holds the parked stack pointer, on the other a heap pointer — an explicit
# conflict mark. A `;! load ptr` through it passes through and is
# bounds-checked at runtime against the explicit capability, fail-closed:
# the heap path (sel != 0) succeeds exactly like confheap, but the stack
# path (sel == 0, exercised here) carries a stack+offset value with no valid
# capability lower, so the check traps with a filc safety error instead of
# accessing out of bounds. This is the must-trap dual of
# sarcasm-frame-alias-conflict-heap-att (which exercises only the heap
# path): without the fail-closed check the stack path would read arbitrary
# caller stack as a pointer. Plain unannotated memory forms through a
# conflicted base stay rejected (see sarcasm-reject-alias-ambig-att).
confstack:                      ;! long(ptr,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rsp, %rax
	testq	%rsi, %rsi
	je	.Luse
	movq	%rdi, %rax
.Luse:
	movq	(%rax), %rbx  ;! load ptr
	movq	(%rbx), %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	confstack, .-confstack
	.section	.note.GNU-stack,"",@progbits
