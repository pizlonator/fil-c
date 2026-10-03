# D9 fixed-frame escape promotion, pinned for storing the region pointer into
# a HEAP object. The escape cluster (`leaq 16(%rsp), %rdi` +
# `call fill32 ;! void(ptr)`) promotes the whole fixed frame to ONE GC
# allocation (filc_allocate in the sarcasm output — expected here) whose
# payload is the region [0,64). storeheap:
#   * writes a magic and a second value into the region through derived
#     pointers, re-writes them over the escaping helper's writes, and reads
#     them back through the direct rsp spellings (one home);
#   * stores the region pointer into the caller's heap object with
#     `#! store ptr` — the region stays alive rooted by the heap object's aux.
# The driver churns the allocator and forces a GC, then loadheap re-reads the
# pointer out of the heap object with `#! load ptr` (the capability comes back
# from the heap object's aux) and C dereferences it: the magics and the
# helper's values must all be intact post-GC.
	.text
	.globl	storeheap
	.type	storeheap, @function
storeheap:                      #! void(ptr,long)
	pushq	%rbx
	pushq	%r12
	subq	$64, %rsp           # plain fixed frame [0,64): D0 = 80, region [0,64)
	movq	%rdi, %rbx          # the heap object pointer (callee-saved park)
	movq	%rsi, %r12          # the magic value
	leaq	16(%rsp), %rdi      # region+16 — the ESCAPING lea: promotion
	movq	%r12, (%rdi)        # slot16 = the magic through the derived pointer
	movq	%rdi, %rsi          # copy of the frame pointer
	leaq	8(%rsi), %rdx       # offset of the copy: region+24
	movq	$4222, (%rdx)       # slot24
	call	fill32 ;! void(ptr) # helper writes 100..103 at frame+16..frame+47

	# --- re-write the values through a fresh derived pointer (one home with
	# the direct rsp spellings) ---
	leaq	16(%rsp), %rax      # fresh derived pointer: region+16 (post-call)
	movq	%r12, (%rax)        # slot16 = the magic again (over the helper's 100)
	movq	$4222, 8(%rax)      # slot24 = 4222 again (over the helper's 101)
	movq	16(%rsp), %rcx      # direct slot read: the magic
	movq	%rcx, %r8
	movq	24(%rsp), %rcx      # direct slot read: 4222
	addq	%rcx, %r8
	movq	32(%rsp), %rcx      # the helper's 102
	addq	%rcx, %r8
	movq	40(%rsp), %rcx      # the helper's 103
	addq	%rcx, %r8           # 8538 (with magic 4111)
	movq	%r8, 56(%rsp)       # park the checksum at slot56

	# --- store the region pointer into the heap object ---
	movq	%rax, (%rbx)        ;! store ptr
	addq	$64, %rsp
	popq	%r12
	popq	%rbx
	ret
	.size	storeheap, .-storeheap
	.globl	loadheap
	.type	loadheap, @function
loadheap:                       #! ptr(ptr)
	movq	(%rdi), %rax        ;! load ptr
	ret
	.size	loadheap, .-loadheap
	.section	.note.GNU-stack,"",@progbits
