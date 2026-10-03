	.file	"b2clone-oob.s"
	.text
# Out-of-bounds access through a B2 clone's lowered stack buffer must TRAP at
# runtime: the clone's indexed access carries the same runtime bounds check a
# non-clone buffer access does. fnA B2-clones fnB's tail (see
# sarcasm-stackbuf-b2clone-att); the driver calls fnA(61) — one byte past the
# declared 64-byte buffer for a 4-byte access — and the trip must panic.
	.globl	fnA
	.type	fnA, @function
fnA:                            ;! long(size_t)
	jmp	.LfnB_body
	.size	fnA, .-fnA

	.globl	fnB
	.type	fnB, @function
fnB:                            ;! long(size_t)
.LfnB_entry:
.LfnB_body:
	subq	$64, %rsp
	movl	$0x11111111, 0(%rsp)
	movl	$0x22222222, 4(%rsp)
.LfnB_tail:
	movl	(%rsp,%rdi), %eax #! stack buffer (ks, %rsp, %rsp + 64)
	addq	$64, %rsp
	ret
	.size	fnB, .-fnB
	.section	.note.GNU-stack,"",@progbits
