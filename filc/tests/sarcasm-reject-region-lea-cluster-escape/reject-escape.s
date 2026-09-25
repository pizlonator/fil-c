# UNSOUND: an INDEXED lea into the plain fixed frame escapes to a helper
# through a copy chain. (The fixed-frame escape promotion (D9) that used to
# cover plain base+disp leas was removed — every stack-address lea that the
# carrier machinery cannot legalize is rejected — so the indexed escape
# keeps the clean escape rejection.)
	.text
	.globl	badcluster
	.type	badcluster, @function
badcluster:                     #! long(long)
	subq	$136, %rsp
	movq	$32, %rax
	leaq	0(%rsp,%rax,1), %rdi  # indexed address of the fixed frame — escape
	movq	%rdi, %rsi            # copy chain
	call	fill32 ;! void(ptr)
	movq	32(%rsp), %rax
	addq	$136, %rsp
	ret
	.size	badcluster, .-badcluster
	.section	.note.GNU-stack,"",@progbits
