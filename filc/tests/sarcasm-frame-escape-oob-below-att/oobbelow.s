# D9 fixed-frame escape promotion, pinned for the LOWER bound of the region.
# The escape cluster (`leaq 16(%rsp), %rdi` + `call fill32 ;! void(ptr)`)
# promotes the whole fixed frame to ONE GC allocation (filc_allocate in the
# sarcasm output — expected here) whose payload is the region [0,64). Derived
# pointers carry the region's real capability, so accesses through them are
# bounds-checked against [region+0, region+64) on BOTH ends:
#   * the in-bounds derived traffic works — the region is live;
#   * then a derived pointer 128 bytes BELOW the region+16 base (region-112)
#     lands BELOW the region lower (0): the read `movq (%rbx), %rax` must
#     panic with "cannot read pointer with ptr < lower." and exit 42 under
#     FILC_EXIT_ON_PANIC=1, BEFORE the C driver produces any output.
# (Note the contrast with the red-zone test: a promoted frame's own red-zone
# slots ride the SYNTHESIZED frame and never go through the region's
# capability — this test walks a DERIVED pointer below the region instead.)
	.text
	.globl	oobbelow
	.type	oobbelow, @function
oobbelow:                       #! long(long)
	pushq	%rbx
	subq	$64, %rsp           # plain fixed frame [0,64): D0 = 72, region [0,64)

	# --- escape cluster: the lea escapes to the helper, which promotes the
	# whole frame to one GC region ---
	leaq	16(%rsp), %rdi      # region+16 — the ESCAPING lea
	movq	%rdi, %rsi          # copy of the frame pointer
	leaq	8(%rsi), %rdx       # offset of the copy: region+24
	movq	$100, (%rdi)        # in-bounds write through the derived pointer
	movq	$101, (%rdx)        # in-bounds write through the offset copy
	call	fill32 ;! void(ptr) # helper writes 100..103 at frame+16..frame+47

	# --- in-bounds derived reads still work after the call ---
	leaq	16(%rsp), %rax      # fresh derived pointer: region+16
	movq	(%rax), %rcx        # derived read: 100
	movq	8(%rax), %rcx       # derived read: 101

	# --- the out-of-bounds read: 128 bytes below the region+16 base ---
	leaq	-128(%rax), %rbx    # derived: region-112 — BELOW the region lower
	movq	(%rbx), %rax        # OOB READ: ptr < lower -> panic here
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	oobbelow, .-oobbelow
	.section	.note.GNU-stack,"",@progbits
