# D9 fixed-frame escape promotion, pinned for STACK-PASSED (7th+) argument
# marshalling OUT of a promoted frame. The escape cluster (`leaq 16(%rsp), %rax`
# + copies/offset leas + `call sink8 ;! long(ptr,...)`) promotes the whole fixed
# frame to ONE GC allocation (filc_allocate in the sarcasm output — expected
# here); the frame's OUTGOING stack-argument area [0, 8*maxOutgoingWords) is
# carved OUT of the promoted region and stays slot traffic, so the callsites can
# pass pointers as their 7th+ (stack-passed) arguments:
#   * the first callsite runs at the frame-base depth (D0) and passes the
#     REGION POINTER (region+32, derived) as argument 7 — outgoing word 0 —
#     and a malloc'd pointer as argument 8 — outgoing word 1;
#   * the second callsite runs at a SHIFTED rsp depth (a balanced dummy
#     push/pop plus a 16-byte scratch, so the call executes at D0+24) and
#     passes the region pointer as a stack argument AGAIN — a DIFFERENT
#     region offset (region+40), so a stale or depth-blind binding of the
#     outgoing word shows up in the checksum. This pins the depth-aware
#     callArgSource keying: a call at depth d must source its outgoing word o
#     from the slot keyed 8*o + D0 - d, exactly where the body's SysV-spelled
#     store landed; if the keying ignored the depth, the marshal would bind
#     the previous callsite's (or a never-defined) slot temp, and the callee
#     would see a stale value or a null capability lower and panic. The
#     scratch also keeps the outgoing words BELOW the dummy park's save slot:
#     the park's slot must not be shared with an outgoing-argument word (a
#     store into an outstanding save slot desyncs the marshal — observed as a
#     marshalled $0 lower or a garbage value for that argument).
# The C sink writes a distinct tag through every pointer argument and returns
# their sum; the asm reads its frame back through direct slots, derived
# pointers and one movdqa slot, and returns an exact checksum. The driver
# validates the checksum and the sink's writes into the malloc'd buffers.
	.text
	.globl	yoloesc
	.type	yoloesc, @function
yoloesc:                        #! long(ptr,ptr,long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$96, %rsp           # fixed frame [0,96): D0 = 136, region [16,96)
	                            # (the outgoing band [0,16) of the two 8-argument
	                            # callsites is carved out of the region)

	# args: %rdi = bufA, %rsi = bufB, %rdx = the seed
	movq	%rdx, %r12          # park the seed (rdx is a call-clobbered arg reg)
	movq	%rdi, %r13          # park bufA
	movq	%rsi, %r14          # park bufB
	xorl	%r15d, %r15d        # the checksum accumulator

	# --- escape cluster: the lea escapes to sink8 -> the whole fixed frame
	# is promoted to ONE GC region [16,96) ---
	leaq	16(%rsp), %rax      # region+0 — the ESCAPING lea
	movq	%rax, %rbx          # park the region+0 pointer (call-safe)

	# --- seed the region's own slots (neither call touches these) ---
	leaq	64(%rdx), %rcx      # seed+64
	movq	%rcx, 80(%rsp)      # region+64
	leaq	72(%rdx), %rcx      # seed+72
	movq	%rcx, 88(%rsp)      # region+72
	movq	$13, %rcx
	movq	%rcx, %xmm0
	movq	$14, %rcx
	movq	%rcx, %xmm1
	punpcklqdq	%xmm1, %xmm0    # xmm0 = {13, 14}
	movdqa	%xmm0, 64(%rsp)     # region+48/+56: the FP-parked lanes

	# --- FIRST callsite, at the frame-base depth: the region pointer rides
	# as the 7th (STACK-PASSED) argument, a malloc'd pointer as the 8th ---
	movq	%rbx, %rdx          # arg3 = region+0 (the parked derived pointer)
	leaq	24(%rsp), %rcx      # arg4 = region+8
	leaq	32(%rsp), %r8       # arg5 = region+16
	leaq	40(%rsp), %r9       # arg6 = region+24
	leaq	48(%rsp), %rax      # region+32 (derived; %rax is free again)
	movq	%rax, (%rsp)        # outgoing word 0 (arg7): the REGION POINTER
	movq	%r14, 8(%rsp)       # outgoing word 1 (arg8): the malloc'd bufB
	call	sink8 ;! long(ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr)

	# --- read the frame back after the first call (both spellings) ---
	movq	%rax, %r15          # the sink's sum (836)
	leaq	16(%rsp), %rcx      # a fresh derived region+0 pointer
	movq	(%rcx), %rdx        # 103 via the derived pointer
	addq	16(%rsp), %rdx      # + 103 via the direct slot (one home)
	addq	24(%rsp), %rdx      # 104 (the sink wrote through region+8)
	addq	32(%rsp), %rdx      # 105 (region+16)
	addq	40(%rsp), %rdx      # 106 (region+24)
	addq	48(%rsp), %rdx      # 107 (the sink wrote through the STACK arg)
	movdqa	64(%rsp), %xmm2     # the FP lanes survived the call
	movq	%xmm2, %rcx
	addq	%rcx, %rdx          # 13
	pextrq	$1, %xmm2, %rcx
	addq	%rcx, %rdx          # 14
	addq	80(%rsp), %rdx      # 964 (seed+64)
	addq	88(%rsp), %rdx      # 972 (seed+72)
	addq	%rdx, %r15

	# --- SECOND callsite at a SHIFTED rsp depth (one dummy push plus a
	# 16-byte scratch, so the call executes at D0+24): the region pointer
	# rides as a stack argument AGAIN — the depth-aware callArgSource pin ---
	pushq	%r12                # the dummy park (the call will run at D0+24;
	                            # the push/pop pair is balanced, so the popped
	                            # register keeps the seed)
	subq	$16, %rsp           # scratch: the outgoing words land BELOW the
	                            # park's save slot
	movq	%r14, %rdi          # arg1 = bufB
	movq	%r13, %rsi          # arg2 = bufA
	movq	%rbx, %rdx          # arg3 = region+0
	leaq	48(%rsp), %rcx      # arg4 = region+8 (shifted spelling: 24 @ D0+24)
	leaq	56(%rsp), %r8       # arg5 = region+16
	leaq	64(%rsp), %r9       # arg6 = region+24
	leaq	80(%rsp), %rax      # region+40 — call 2's stack-arg target
	movq	%rax, (%rsp)        # outgoing word 0 (arg7): the REGION POINTER
	movq	%r13, 8(%rsp)       # outgoing word 1 (arg8): the malloc'd bufA
	call	sink8 ;! long(ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr)
	addq	$16, %rsp           # drop the scratch
	popq	%r12                # the balanced restore: the depth is back at D0
	                            # and %r12 still holds the seed

	# --- final readback at the frame-base depth ---
	movq	%rax, %rdx          # the sink's sum (836)
	addq	%rdx, %r15
	leaq	16(%rsp), %rcx      # a fresh derived region+0 pointer
	movq	(%rcx), %rdx        # 103 via the derived pointer
	addq	16(%rsp), %rdx      # + 103 via the direct slot (one home)
	addq	24(%rsp), %rdx      # 104
	addq	32(%rsp), %rdx      # 105
	addq	40(%rsp), %rdx      # 106
	addq	48(%rsp), %rdx      # 107 (written through call 1's STACK arg)
	addq	56(%rsp), %rdx      # 107 (written through the SHIFTED-depth stack arg)
	movdqa	64(%rsp), %xmm2
	movq	%xmm2, %rcx
	addq	%rcx, %rdx          # 13
	pextrq	$1, %xmm2, %rcx
	addq	%rcx, %rdx          # 14
	addq	80(%rsp), %rdx      # 964
	addq	88(%rsp), %rdx      # 972
	addq	%rdx, %r15
	addq	%r12, %r15          # + the seed, once
	movq	%r15, %rax
	addq	$96, %rsp
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	yoloesc, .-yoloesc
	.section	.note.GNU-stack,"",@progbits
