# Constant-indexed stack traffic in a B2 clone (fail-closed): the index
# register holds a compile-time-constant 0, so `0(%rsp,%rcx,1)` names exactly
# the address the plain store just wrote — yet the access is still indexed,
# and only plain base+disp slots virtualize. The frame pass cannot fold the
# constant (it never evaluates register values), so accepting the load under
# the plain key would alias two keyings to one address: the moment %rcx
# differs (or a second indexed store lands elsewhere) the webs desync — a
# store through one keying invisible to a load through the other. Static
# error. Companion to sarcasm-reject-b2-constindex-att (variable index) and
# sarcasm-reject-b2-indexed-att (scaled index): together they pin every
# indexed spelling.
	.text
	.globl	cidxc_jump
	.type	cidxc_jump, @function
cidxc_jump:                     ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	jmp	.Lcidxc_tail
	.size	cidxc_jump, .-cidxc_jump
	.globl	cidxc_owner
	.type	cidxc_owner, @function
cidxc_owner:                    ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	nop
.Lcidxc_tail:
	movq	$7, 0(%rsp)
	movq	$0, %rcx
	movq	0(%rsp,%rcx,1), %rax
	addq	%rbx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	cidxc_owner, .-cidxc_owner
	.section	.note.GNU-stack,"",@progbits
