# D9 fixed-frame escape promotion, pinned for RETURNING the region pointer to
# C. The escape cluster (`leaq 16(%rsp), %rdi` + `call fill32 ;! void(ptr)`)
# promotes the whole fixed frame to ONE GC allocation (filc_allocate in the
# sarcasm output — expected here) whose payload is the region [0,64).
# frameret:
#   * writes a magic and a second value into the region through derived
#     pointers, hands the derived pointer to the escaping helper, then
#     re-writes the magics over the helper's writes and reads the slots back
#     through the direct rsp spellings (one home);
#   * parks a checksum in slot56 — the C driver reads it through the returned
#     pointer, so the checksum crosses the return;
#   * launders the region pointer through the in-region slot48 with
#     `#! store ptr` / `#! load ptr` (a REAL pointer store/load into the GC
#     object — the capability rides the region object's aux) and returns it.
#     Returning the frame-derived pointer DIRECTLY is rejected at compile time
#     (see sarcasm-frame-escape-return-reject-att) — the laundered load
#     redefines %rax with a plain pointer web, which is what makes the return
#     provable. The pointer arrives in C as a real GC pointer (region+16) and
#     survives GC churn (the caller's local roots it).
# frametouch:
#   * takes the kept pointer, reads ALL the old region's slots through it
#     (re-derived from the ARGUMENT — the old region survived the GC), parks a
#     checksum back into the old region through the same pointer, then runs
#     its OWN escape cluster — a FRESH region for this activation — and
#     returns the fresh pointer, laundered the same way.
	.text
	.globl	frameret
	.type	frameret, @function
frameret:                       #! ptr(long)
	pushq	%rbx
	subq	$64, %rsp           # plain fixed frame [0,64): D0 = 72, region [0,64)

	# --- escape cluster: the lea escapes to the helper, which promotes the
	# whole frame to one GC region ---
	leaq	16(%rsp), %rdi      # region+16 — the ESCAPING lea
	movq	%rdi, %rsi          # copy of the frame pointer
	leaq	8(%rsi), %rdx       # offset of the copy: region+24
	movq	$4111, (%rdi)       # slot16 = 4111 through the derived pointer
	movq	$4222, (%rdx)       # slot24 = 4222
	call	fill32 ;! void(ptr) # helper writes 100..103 at frame+16..frame+47

	# --- re-write the magics through a fresh derived pointer, then read the
	# slots back through the direct rsp spellings (one home) ---
	leaq	16(%rsp), %rax      # fresh derived pointer: region+16 (post-call)
	movq	$4111, (%rax)       # slot16 = 4111 again (over the helper's 100)
	movq	$4222, 8(%rax)      # slot24 = 4222 again (over the helper's 101)
	movq	16(%rsp), %rcx      # direct slot read: 4111
	movq	%rcx, %r8
	movq	24(%rsp), %rcx      # direct slot read: 4222
	addq	%rcx, %r8
	movq	32(%rsp), %rcx      # the helper's 102
	addq	%rcx, %r8
	movq	40(%rsp), %rcx      # the helper's 103
	addq	%rcx, %r8           # 8538
	movq	%r8, 56(%rsp)       # park the checksum at slot56 (crosses the return)

	# --- launder the region pointer through the region slot and return it ---
	movq	%rax, 48(%rsp)      ;! store ptr
	movq	48(%rsp), %rax      ;! load ptr
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	frameret, .-frameret
	.globl	frametouch
	.type	frametouch, @function
frametouch:                     #! ptr(ptr)
	pushq	%rbx
	pushq	%r12
	subq	$64, %rsp           # plain fixed frame [0,64): D0 = 80, region [0,64)
	movq	%rdi, %rbx          # the old region pointer C kept (old region+16)
	movq	(%rbx), %rax        # p[0] = slot16 = 20881 (C's post-GC write survived)
	movq	8(%rbx), %r12       # p[1] = slot24 = 4222
	addq	%r12, %rax
	movq	16(%rbx), %r12      # p[2] = slot32 = 102
	addq	%r12, %rax
	movq	24(%rbx), %r12      # p[3] = slot40 = 103
	addq	%r12, %rax
	movq	40(%rbx), %r12      # p[5] = slot56 = 8538 (frameret's parked checksum)
	addq	%r12, %rax          # 33846
	movq	%rax, 40(%rbx)      # park it back at the old slot56 (old-region write)

	# --- a FRESH escape cluster: this activation gets its own region ---
	leaq	16(%rsp), %rdi      # fresh region+16 — the ESCAPING lea
	movq	$29555, (%rdi)      # fresh slot16 = 29555
	call	fill32 ;! void(ptr) # fresh region+16..47 = 100..103
	leaq	16(%rsp), %rax      # fresh derived pointer: fresh region+16 (post-call)
	movq	$29555, (%rax)      # fresh slot16 = 29555 again (over the helper's 100)
	movq	16(%rsp), %rcx      # direct slot read: 29555 (one home)
	movq	%rcx, %r8
	movq	24(%rsp), %rcx      # the helper's 101
	addq	%rcx, %r8
	movq	32(%rsp), %rcx      # 102
	addq	%rcx, %r8
	movq	40(%rsp), %rcx      # 103
	addq	%rcx, %r8           # 29861
	movq	%r8, 56(%rsp)       # park the fresh checksum at fresh slot56

	# --- launder the FRESH region pointer through the fresh slot and return ---
	movq	%rax, 48(%rsp)      ;! store ptr
	movq	48(%rsp), %rax      ;! load ptr
	addq	$64, %rsp
	popq	%r12
	popq	%rbx
	ret
	.size	frametouch, .-frametouch
	.section	.note.GNU-stack,"",@progbits
