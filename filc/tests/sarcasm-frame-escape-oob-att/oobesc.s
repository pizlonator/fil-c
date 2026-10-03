# D9 fixed-frame escape promotion, pinned for the UPPER bound of the region.
# The escape cluster (`leaq 16(%rsp), %rdi` + `call fill32 ;! void(ptr)`)
# promotes the whole fixed frame to ONE GC allocation (filc_allocate in the
# sarcasm output — expected here) whose payload is the region [0,64). Every
# fixed-frame access and every plain frame lea redirects into that region, and
# derived pointers carry the region's real capability, so accesses through them
# are bounds-checked against [region+0, region+64):
#   * the in-bounds derived traffic (writes through %rdi/%rsi/%rdx and reads
#     through the post-call re-derivation in %rax) works — the region is live;
#   * then a derived pointer 128 bytes past the region+16 base (region+144)
#     lands PAST the region upper (64): the read `movq (%rbx), %rax` must panic
#     with "cannot read pointer with ptr >= upper." and exit 42 under
#     FILC_EXIT_ON_PANIC=1, BEFORE the C driver produces any output.
	.text
	.globl	oobesc
	.type	oobesc, @function
oobesc:                         #! long(long)
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

	# --- the out-of-bounds read: 128 bytes past the region+16 base ---
	leaq	128(%rax), %rbx     # derived: region+144 — far PAST the region upper
	movq	(%rbx), %rax        # OOB READ: ptr >= upper -> panic here
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	oobesc, .-oobesc
	.section	.note.GNU-stack,"",@progbits
