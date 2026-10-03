# GC stress for pointer round-trips through the D9-promoted frame. The escape
# cluster (`leaq 16(%rsp), %rax` + `call churn ;! long(ptr)`) promotes the
# whole fixed frame to ONE GC allocation (filc_allocate in the sarcasm output
# — expected here). The asm:
#   * stores the ARGUMENT POINTER into a frame slot with `#! store ptr` (the
#     capability rides the region's sidecar);
#   * calls a C helper that CHURNS the allocator and, every K-th call, forces
#     zgc_request_and_wait() while the pointer sits in the frame;
#   * reloads it with `#! load ptr`, writes THROUGH the reloaded pointer,
#     stores it into a SECOND slot, reloads, and dereferences again (a lost
#     capability would trap right there);
#   * round-trips a scalar slot and a movdqa slot alongside (plain region
#     traffic is lossless for non-pointers);
#   * returns an exact checksum of the helper's return, the values read
#     through the reloaded pointers, and the companions.
# The driver runs many threads with thread-distinct objects and validates the
# checksum (and the object) every iteration; dirty_stack() poisons the native
# stack between calls so any unrooted root slot would be observed.
	.text
	.globl	ptrslotfn
	.type	ptrslotfn, @function
ptrslotfn:                      #! long(ptr,long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	subq	$128, %rsp          # fixed frame [0,128): D0 = 152, region [0,128)

	# args: %rdi = the object, %rsi = the per-call scalar
	movq	%rsi, %r12          # park the scalar (call-safe)
	movq	%rdi, %rbx          # park the object pointer (call-safe)

	# --- escape cluster: the lea escapes to the churn helper -> promotion ---
	leaq	16(%rsp), %rax      # region+16 — the ESCAPING lea
	movq	%rax, %rcx          # copy of the frame pointer

	# --- park the argument pointer in slot16: the capability rides the
	# region's sidecar across the churn + GC ---
	movq	%rbx, 16(%rsp)      #! store ptr

	# --- scalar and movdqa companions (plain round-trips) ---
	movq	%r12, 24(%rsp)      # slot24 = the scalar
	movq	$7, %rcx
	movq	%rcx, %xmm0
	movq	$8, %rcx
	movq	%rcx, %xmm1
	punpcklqdq	%xmm1, %xmm0    # xmm0 = {7, 8}
	movdqa	%xmm0, 32(%rsp)     # slots 32/40

	# --- churn + a forced GC every K-th call, below the asm frame ---
	movq	%rbx, %rdi          # arg = the object pointer
	call	churn ;! long(ptr)
	movq	%rax, %r13          # the churn return (obj[0] as the helper saw it)

	# --- reload the object pointer from the region and write through it ---
	movq	16(%rsp), %rax      #! load ptr
	movq	$4242, (%rax)       # a write through the reloaded pointer

	# --- store it into a SECOND slot; reload; dereference again ---
	movq	%rax, 48(%rsp)      #! store ptr
	movq	48(%rsp), %rcx      #! load ptr
	movq	(%rcx), %rdx        # 4242 (the write through the first reload)
	movq	8(%rcx), %rsi       # obj[1] (the driver's per-thread value)

	# --- the companions round-trip ---
	movq	24(%rsp), %r8       # the scalar
	movdqa	32(%rsp), %xmm2
	movq	%xmm2, %r9          # 7
	pextrq	$1, %xmm2, %r10     # 8

	# --- checksum ---
	addq	%r13, %rdx          # + the churn return
	addq	%rsi, %rdx          # + obj[1]
	addq	%r8, %rdx           # + the scalar
	addq	%r9, %rdx           # + 7
	addq	%r10, %rdx          # + 8
	movq	%rdx, %rax
	addq	$128, %rsp
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	ptrslotfn, .-ptrslotfn
	.section	.note.GNU-stack,"",@progbits
