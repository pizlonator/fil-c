# GC stress for the D9 fixed-frame escape promotion. The escape cluster
# (`leaq 16(%rsp), %rax` + `call gchelper ;! long(ptr,ptr,long)`) promotes the
# whole fixed frame to ONE GC allocation (filc_allocate in the sarcasm output
# — expected here) whose payload backs every frame slot. The asm writes a
# per-iteration pattern (thread- and iteration-distinct) into the frame via
# DIRECT SLOTS and DERIVED POINTERS over the same homes, GPR and FP (one
# movdqa slot), then calls a C helper that CHURNS the allocator and, every
# K-th call, forces zgc_request_and_wait() while the asm's derived pointers
# and frame slots are live, and writes through BOTH derived pointers. The asm
# then reads ALL slots back (direct slots AND a fresh post-GC derived
# pointer) and returns an exact checksum combining its own writes, the
# helper's writes, the argument and the object's contents. Any slot that lost
# its one home, any unrooted region pointer, and any slot the GC or the
# helper's writes corrupted shows up as a checksum mismatch in the driver.
	.text
	.globl	gcescfn
	.type	gcescfn, @function
gcescfn:                        #! long(ptr,long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	subq	$128, %rsp          # fixed frame [0,128): D0 = 152, region [0,128)

	# args: %rdi = the object, %rsi = the per-call scalar
	movq	%rsi, %r12          # park the scalar (call-safe)
	movq	%rdi, %r13          # park the object pointer (call-safe)

	# --- escape cluster: the lea escapes to the helper -> promotion ---
	leaq	16(%rsp), %rax      # region+16 — the ESCAPING lea
	movq	%rax, %rbx          # park the region+16 pointer (call-safe)

	# --- the asm's own writes: direct slots AND derived pointers over the
	# same homes, GPR and FP ---
	movq	%rsi, 16(%rsp)      # slot16 = the scalar (direct)
	leaq	1(%r12), %rcx
	movq	%rcx, (%rax)        # region+16 = the scalar + 1 (derived overwrite)
	leaq	2(%r12), %rcx
	movq	%rcx, 24(%rsp)      # slot24 = the scalar + 2 (direct)
	movq	%rcx, 8(%rax)       # region+24 = the scalar + 2 (derived: same home)
	movq	(%r13), %rcx        # obj[0] (checked read: the driver's per-iteration value)
	addq	$1000, %rcx
	movq	%rcx, 32(%rsp)      # slot32 = obj[0] + 1000
	movq	$7, %rcx
	movq	%rcx, %xmm0
	movq	$8, %rcx
	movq	%rcx, %xmm1
	punpcklqdq	%xmm1, %xmm0    # xmm0 = {7, 8}
	movdqa	%xmm0, 48(%rsp)     # slots 48/56: the movdqa slot
	movq	%r12, %rcx
	addq	$3000, %rcx
	movq	%rcx, 64(%rsp)      # slot64 = the scalar + 3000
	movq	(%r13), %rcx        # obj[0] again
	addq	$4000, %rcx
	movq	%rcx, 72(%rsp)      # slot72 = obj[0] + 4000
	movq	$5000, 80(%rsp)     # slot80
	movq	$6000, 88(%rsp)     # slot88
	movq	%r12, 8(%r13)       # obj[1] = the scalar (a write into the object)

	# --- the helper: churn + a forced GC every K-th call, then writes through
	# BOTH derived pointers while the asm's frame is live ---
	leaq	24(%rsp), %rdi      # arg1 = region+24 (derived)
	movq	%rbx, %rsi          # arg2 = region+16 (the parked derived pointer)
	movq	%r12, %rdx          # arg3 = the scalar
	call	gchelper ;! long(ptr,ptr,long)

	# --- read ALL slots back (the helper's writes over slots 16/24), through
	# both spellings ---
	movq	%rax, %rbx          # the helper's return (33333 + the scalar)
	leaq	16(%rsp), %rcx      # a FRESH post-GC derived region+16 pointer
	movq	(%rcx), %rdx        # 22222 via the derived pointer (the helper's write)
	addq	%rdx, %rbx
	addq	16(%rsp), %rbx      # + 22222 via the direct slot (one home)
	movq	8(%rcx), %rdx       # 11111 via the derived pointer (region+24)
	addq	%rdx, %rbx
	addq	24(%rsp), %rbx      # + 11111 via the direct slot (one home)
	addq	32(%rsp), %rbx      # + obj[0] + 1000 (survived the GC)
	movdqa	48(%rsp), %xmm2     # the movdqa slot survived the GC
	movq	%xmm2, %rdx
	addq	%rdx, %rbx          # 7
	pextrq	$1, %xmm2, %rdx
	addq	%rdx, %rbx          # 8
	addq	64(%rsp), %rbx      # + the scalar + 3000
	addq	72(%rsp), %rbx      # + obj[0] + 4000
	addq	80(%rsp), %rbx      # + 5000
	addq	88(%rsp), %rbx      # + 6000
	movq	(%r13), %rdx        # obj[0] readback (post-GC checked read)
	addq	%rdx, %rbx
	movq	8(%r13), %rdx       # obj[1] = the scalar (the asm's write survived)
	addq	%rdx, %rbx
	addq	%r12, %rbx          # + the scalar (the argument), once
	movq	%rbx, %rax
	addq	$128, %rsp
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	gcescfn, .-gcescfn
	.section	.note.GNU-stack,"",@progbits
