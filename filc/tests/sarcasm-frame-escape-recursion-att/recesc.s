# D9 fixed-frame escape promotion, pinned for RECURSION: every activation gets
# a FRESH region. The escape cluster (`leaq 16(%rsp), %rsi` +
# `call rec_c ;! long(long,ptr)` — the derived pointer IS rec_c's ptr argument)
# promotes the whole fixed frame to ONE GC allocation (filc_allocate in the
# sarcasm output — expected here) whose payload is the region [0,64). Per
# activation:
#   * the per-depth magic (the depth itself) is written into the region through
#     the derived pointer, along with a second magic (3000+depth) at slot24;
#   * the derived pointer is parked in %rbx (call-safe) and handed to rec_c,
#     which churns mallocs, occasionally forces zgc_request_and_wait(), reads
#     the CURRENT activation's region from C (region[0] == depth — a shared
#     region would corrupt the magics here), and recurses into the asm with
#     depth-1;
#   * after the call, the activation reads its own slot16 through the direct
#     rsp spelling and its slot24 through the parked derived pointer (one home,
#     survived the whole recursion below it) and XORs both into the return.
# 16 levels; the C driver mirrors the exact XOR chain. (FUGC_SCRIBBLE safety:
# every slot the asm reads was first written by the asm.)
	.text
	.globl	recesc
	.type	recesc, @function
recesc:                         #! long(long,ptr)
	pushq	%rbx
	pushq	%r12
	subq	$64, %rsp           # plain fixed frame [0,64): D0 = 80, region [0,64)
	movq	%rdi, %r12          # depth
	leaq	16(%rsp), %rsi      # region+16 — the ESCAPING lea: promotion (rec_c's ptr arg)
	movq	%r12, (%rsi)        # slot16 = depth — the per-activation magic (derived write)
	movq	$3000, %rcx
	addq	%r12, %rcx          # 3000 + depth
	movq	%rcx, 8(%rsi)       # slot24 via the derived pointer
	movq	%rsi, %rbx          # park the derived pointer (call-safe across the recursion)
	movq	%r12, %rdi          # rec_c's depth
	call	rec_c ;! long(long,ptr)  # churn + occasional GC + recurse with depth-1
	movq	16(%rsp), %rcx      # direct slot read: THIS activation's own slot16 = depth
	xorq	%rcx, %rax
	movq	8(%rbx), %rcx       # derived read: slot24 = 3000+depth (one home)
	xorq	%rcx, %rax
	addq	$64, %rsp
	popq	%r12
	popq	%rbx
	ret
	.size	recesc, .-recesc
	.section	.note.GNU-stack,"",@progbits
