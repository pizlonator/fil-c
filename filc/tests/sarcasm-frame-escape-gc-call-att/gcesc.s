# D9 fixed-frame escape promotion, pinned for a GC forced INSIDE a callee
# while the asm's derived pointers are live. The escape cluster
# (`leaq 16(%rsp), %rdi` + `call gc_helper ;! long(ptr)`) promotes the whole
# fixed frame to ONE GC allocation (filc_allocate in the sarcasm output —
# expected here) whose payload is the region [0,64). The asm:
#   * writes the argument plus three constants into the region through derived
#     pointers;
#   * passes the derived pointer (parked in %rbx, call-safe) to gc_helper,
#     which reads the region through it, CHURNS + forces zgc_request_and_wait()
#     while the asm's derived pointers and frame slots are live, writes a new
#     value through it, and returns the pre-GC value;
#   * then reads ALL its frame slots back — through BOTH spellings: a fresh
#     post-GC derived pointer (%rbx re-derived from the frame lea) AND the
#     direct rsp-relative slots — and checksums. Every read must reflect the
#     helper's write (one home, survived the GC): the checksum is exact
#     (48657).
	.text
	.globl	gcesc
	.type	gcesc, @function
gcesc:                          #! long(ptr)
	pushq	%rbx
	pushq	%r12
	subq	$64, %rsp           # plain fixed frame [0,64): D0 = 80, region [0,64)
	movq	%rdi, %r12          # the argument (parked; %rdi is taken by the lea)
	leaq	16(%rsp), %rdi      # region+16 — the ESCAPING lea: promotion
	movq	%r12, (%rdi)        # slot16 = the argument through the derived pointer
	movq	%rdi, %rsi          # copy of the frame pointer
	leaq	8(%rsi), %rdx       # offset of the copy: region+24
	movq	$4222, (%rdx)       # slot24
	movq	$5444, 16(%rsi)     # slot32
	movq	$4333, 24(%rsi)     # slot40
	movq	%rsi, %rbx          # park the derived region+16 pointer (call-safe)
	call	gc_helper ;! long(ptr)  # reads slot16, churns + GCs, writes slot16

	# --- read every slot back after the GC, both spellings ---
	movq	%rax, %r12          # the helper's return (the pre-GC slot16)
	leaq	16(%rsp), %rbx      # fresh derived pointer: region+16 (post-GC!)
	movq	(%rbx), %rax        # derived read: the helper's post-GC write (20881)
	addq	%r12, %rax          # + the helper's return (4111)
	movq	8(%rbx), %rcx       # derived: slot24 = 4222
	addq	%rcx, %rax
	movq	24(%rsp), %rcx      # direct slot read: slot24 again (one home)
	addq	%rcx, %rax
	movq	16(%rbx), %rcx      # derived: slot32 = 5444
	addq	%rcx, %rax
	movq	32(%rsp), %rcx      # direct slot read: slot32 again (one home)
	addq	%rcx, %rax
	movq	40(%rsp), %rcx      # direct: slot40 = 4333
	addq	%rcx, %rax
	addq	$64, %rsp
	popq	%r12
	popq	%rbx
	ret
	.size	gcesc, .-gcesc
	.section	.note.GNU-stack,"",@progbits
