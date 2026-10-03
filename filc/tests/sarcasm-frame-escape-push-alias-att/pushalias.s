# D9 fixed-frame escape promotion pinned for the push-alias RECOVERY path: a
# promoted frame whose SECOND callsite executes at a SHIFTED rsp depth (one
# dummy `pushq %rbx` in front) and passes the region pointer as its 7th
# (STACK-PASSED) argument whose outgoing word 0 IS the dummy push's save slot —
# storing the PUSHED REGISTER ITSELF (`pushq %rbx; movq %rbx, (%rsp)` shape).
# The escape cluster (`leaq 16(%rsp), %rax` + copies/offset leas + `call sink8
# ;! long(ptr,...)`) promotes the whole fixed frame to ONE GC allocation
# (filc_allocate in the sarcasm output — expected here); the outgoing band
# [0,16) is carved out of the promoted region and stays slot traffic.
# This shape marshalled correctly even BEFORE the pinned fix (the pushed
# register IS the stored source, so the save-slot alias model's rewrite of the
# outgoing store defines the same web the call keys); the pinned bug is the
# DIFFERENT-source store (see sarcasm-frame-escape-push-alias-store-att).
# The sink writes a distinct tag through every pointer argument and returns
# their sum; the asm reads its frame back through direct slots, derived
# pointers and one movdqa slot, and returns an exact checksum (9642,
# hardware-exact vs a native gcc build of the same asm semantics).
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
	leaq	40(%rdx), %rcx      # seed+40
	movq	%rcx, 56(%rsp)      # region+40
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
	addq	56(%rsp), %rdx      # 940 (seed+40 survived the call)
	movdqa	64(%rsp), %xmm2     # the FP lanes survived the call
	movq	%xmm2, %rcx
	addq	%rcx, %rdx          # 13
	pextrq	$1, %xmm2, %rcx
	addq	%rcx, %rdx          # 14
	addq	80(%rsp), %rdx      # 964 (seed+64)
	addq	88(%rsp), %rdx      # 972 (seed+72)
	addq	%rdx, %r15

	# --- SECOND callsite at a SHIFTED rsp depth (one dummy push in front,
	# so the call executes at D0+8): the region pointer rides as a stack
	# argument AGAIN — the depth-aware callArgSource pin ---
	pushq	%rbx                # the dummy park: parks the REGION POINTER itself
	                            # (the call now executes at D0+8; its outgoing word
	                            # 0 shares this slot, and every def of the shared
	                            # slot web is a pointer store)
	movq	%r14, %rdi          # arg1 = bufB
	movq	%r13, %rsi          # arg2 = bufA
	movq	%rbx, %rdx          # arg3 = region+0
	leaq	32(%rsp), %rcx      # arg4 = region+8 (shifted spelling: disp 32 @ D0+8)
	leaq	40(%rsp), %r8       # arg5 = region+16
	leaq	48(%rsp), %r9       # arg6 = region+24
	movq	%rbx, (%rsp)        # outgoing word 0 (arg7): the REGION POINTER
	movq	%r13, 8(%rsp)       # outgoing word 1 (arg8): the malloc'd bufA
	call	sink8 ;! long(ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr)
	popq	%rbx                # the balanced restore: the depth is back at D0
	                            # and %rbx still holds the region pointer

	# --- final readback at the frame-base depth ---
	movq	%rax, %rdx          # the sink's sum (836)
	addq	%rdx, %r15
	leaq	16(%rsp), %rcx      # a fresh derived region+0 pointer
	movq	(%rcx), %rdx        # 103 via the derived pointer
	addq	16(%rsp), %rdx      # + 103 via the direct slot (one home)
	addq	24(%rsp), %rdx      # 104
	addq	32(%rsp), %rdx      # 105
	addq	40(%rsp), %rdx      # 106
	addq	48(%rsp), %rdx      # 107 (written through the SHIFTED-depth stack arg)
	addq	56(%rsp), %rdx      # 940
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
