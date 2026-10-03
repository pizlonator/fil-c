	.file	"nonzero.c"
	.text
# A buffer whose base is NOT the frame base: `#! stack buffer (x, %rsp + 16,
# %rsp + 32)` declares normalized offsets [16, 32). The indexed access's
# bounds check runs in buffer coordinates (idx in [16, 28] for a 4-byte
# access) and the lowered displacement translates by the buffer's base.
	.globl	nz_basic
	.type	nz_basic, @function
nz_basic:                       ;! int(size_t)
	subq	$64, %rsp
	movl	$0x11223344, 20(%rsp)
	movl	$0x55667788, 24(%rsp)
	movl	(%rsp,%rdi), %eax #! stack buffer (x, %rsp + 16, %rsp + 32)
	addq	$64, %rsp
	ret
	.size	nz_basic, .-nz_basic

# The same buffer through a stack-alias register parked at the frame base:
# the annotation's bounds are relative to the alias, so `%rbx + 16` is the
# buffer's base (normalized offset 16).
	.globl	nz_alias
	.type	nz_alias, @function
nz_alias:                       ;! int(size_t)
	subq	$64, %rsp
	movq	%rsp, %rbx
	movl	$0x99aabbcc, 24(%rsp)
	movl	(%rbx,%rdi), %eax #! stack buffer (x, %rbx + 16, %rbx + 32)
	addq	$64, %rsp
	ret
	.size	nz_alias, .-nz_alias
	.section	.note.GNU-stack,"",@progbits
