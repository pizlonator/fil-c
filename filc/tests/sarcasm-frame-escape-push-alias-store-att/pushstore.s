# D9 fixed-frame escape promotion pinned for the FIXED push-alias bug: an
# outgoing stack-argument word that SHARES ITS SLOT with an outstanding mid-
# function push's save slot, where the outgoing store's source is NOT the
# pushed register. Before the fix (the prologue-prefix scan swallowed the
# escaping lea cluster and the push into the "prologue prefix"), the callee
# received the PUSHED web instead of the STORED one:
#   * pushst: a callee-saved park (`pushq %r12`, the integer seed) whose save
#     slot IS outgoing word 0, storing the REGION pointer (`movq %rbx,(%rsp)`)
#     — the marshal emitted the seed with a null lower and the callee panicked
#     "cannot write pointer with null object. pointer: 0x384,<null>" (the seed).
#   * pushst2: a spill park (`movq %r13,%rax; pushq %rax`, bufA) whose slot is
#     outgoing word 0, storing the region pointer — a SILENT wrong-value
#     miscompile (the callee got bufA), and the D9 promotion silently did not
#     fire (no filc_allocate) because the swallowed lea classified as a phantom
#     rsp-save carrier instead of an escaping frame address.
# The fix ends the prologue prefix at the interior frame-address lea, so the
# lea escapes (the promotion fires — filc_allocate in the transformed output
# for BOTH functions), the push is a mid-body save/spill, and the alias model
# rewrites the outgoing store onto the right web: the callee receives the
# STORED web, value and capability lower included. The popq after the call
# then restores the STORED value (the outgoing store clobbered the parked
# one), which the `cmpq` pins.
	.text
	.globl	pushst
	.type	pushst, @function
pushst:                         #! long(ptr,ptr,long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	subq	$96, %rsp           # fixed frame [0,96): D0 = 136, region [16,96)
	                            # (the outgoing band [0,16) of the 8-argument
	                            # callsite is carved out of the promoted region)

	# args: %rdi = bufA, %rsi = bufB, %rdx = the seed
	movq	%rdx, %r12          # park the seed (rdx is a call-clobbered arg reg)
	movq	%rdi, %r13          # park bufA
	movq	%rsi, %r14          # park bufB

	# --- escape cluster: the lea escapes to sink8 -> the whole fixed frame
	# is promoted to ONE GC region [16,96) ---
	leaq	16(%rsp), %rax      # region+0 — the ESCAPING lea
	movq	%rax, %rbx          # park the region+0 pointer (call-safe)

	# --- seed the region's own slots (the sink overwrites the low ones) ---
	movq	$940, 48(%rsp)      # region+32
	movq	$950, 56(%rsp)      # region+40
	movq	$960, 64(%rsp)      # region+48
	movq	$970, 72(%rsp)      # region+56

	# --- the callsite: a mid-function push of the CALLEE-SAVED %r12 parks the
	# seed; the push's save slot IS the call's outgoing word 0, and the
	# outgoing store writes a DIFFERENT source (the region pointer) ---
	pushq	%r12                # the pad push: parks the integer seed
	movq	%r14, %rdi          # arg1 = bufB
	movq	%r13, %rsi          # arg2 = bufA
	movq	%rbx, %rdx          # arg3 = region+0
	leaq	24(%rsp), %rcx      # arg4 = region+0 (the shifted spelling)
	leaq	32(%rsp), %r8       # arg5 = region+8
	leaq	40(%rsp), %r9       # arg6 = region+16
	movq	%rbx, (%rsp)        # outgoing word 0 (arg7): the REGION POINTER —
	                            # a different web than the pushed %r12
	movq	%r14, 8(%rsp)       # outgoing word 1 (arg8): the malloc'd bufB
	call	sink8 ;! long(ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr)

	# --- the dropped pop restores what the SLOT holds: the STORED region
	# pointer (the outgoing store clobbered the parked seed) ---
	popq	%r12
	cmpq	%rbx, %r12          # the popped web must be the STORED region pointer
	jne	.Lbad

	# --- read the region back after the call: the sink's writes through the
	# register args and through the STACK-PASSED region pointer all landed ---
	movq	%rax, %r15          # 836 (the sink's return)
	movq	(%rbx), %rdx        # 107 — written through the STACK arg (arg7)
	addq	%rdx, %r15
	addq	24(%rsp), %r15      # 105 (region+8, written through arg5)
	addq	32(%rsp), %r15      # 106 (region+16, written through arg6)
	addq	48(%rsp), %r15      # 940 (the seeded slot survived the call)
	addq	56(%rsp), %r15      # 950
	addq	64(%rsp), %r15      # 960
	addq	72(%rsp), %r15      # 970
	movq	%r15, %rax
	addq	$96, %rsp
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
.Lbad:
	movq	$-777, %rax
	addq	$96, %rsp
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	pushst, .-pushst

	.globl	pushst2
	.type	pushst2, @function
pushst2:                        #! long(ptr,ptr,long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	subq	$96, %rsp           # fixed frame [0,96): D0 = 136, region [16,96)

	movq	%rdx, %r12          # park the seed
	movq	%rdi, %r13          # park bufA
	movq	%rsi, %r14          # park bufB

	leaq	16(%rsp), %rax      # region+0 — the ESCAPING lea
	movq	%rax, %rbx          # park the region+0 pointer

	movq	$841, 48(%rsp)      # region+32
	movq	$851, 56(%rsp)      # region+40
	movq	$861, 64(%rsp)      # region+48
	movq	$871, 72(%rsp)      # region+56

	# --- the callsite: a mid-function SPILL push (a caller-saved register) ---
	movq	%r13, %rax          # rax = bufA (a DIFFERENT web than the stored one)
	pushq	%rax                # the spill push: an ordinary slot web
	movq	%r14, %rdi          # arg1 = bufB
	movq	%r13, %rsi          # arg2 = bufA
	movq	%rbx, %rdx          # arg3 = region+0
	leaq	24(%rsp), %rcx      # arg4 = region+0 (the shifted spelling)
	leaq	32(%rsp), %r8       # arg5 = region+8
	leaq	40(%rsp), %r9       # arg6 = region+16
	movq	%rbx, (%rsp)        # outgoing word 0 (arg7): the REGION POINTER —
	                            # re-stores the spill slot with a different web
	movq	%r14, 8(%rsp)       # outgoing word 1 (arg8): the malloc'd bufB
	call	sink8 ;! long(ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr)
	movq	%rax, %r15          # 836 (park the return: the pop lands in %rax)

	# --- the spill pop reloads the slot's LAST stored value: the STORED
	# region pointer (the outgoing store re-defined the spill slot's web) ---
	popq	%rax
	cmpq	%rbx, %rax          # the popped web must be the STORED region pointer
	jne	.Lbad2

	movq	(%rbx), %rdx        # 107 — written through the STACK arg (arg7)
	addq	%rdx, %r15
	addq	24(%rsp), %r15      # 105 (region+8, written through arg5)
	addq	32(%rsp), %r15      # 106 (region+16, written through arg6)
	addq	48(%rsp), %r15      # 841
	addq	56(%rsp), %r15      # 851
	addq	64(%rsp), %r15      # 861
	addq	72(%rsp), %r15      # 871
	movq	%r15, %rax
	addq	$96, %rsp
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
.Lbad2:
	movq	$-778, %rax
	addq	$96, %rsp
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	pushst2, .-pushst2
	.section	.note.GNU-stack,"",@progbits
