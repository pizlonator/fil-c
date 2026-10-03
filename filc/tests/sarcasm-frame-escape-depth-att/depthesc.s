# D9 fixed-frame escape promotion, pinned for DEPTH-NORMALIZED region keying.
# The prologue takes the address of the fixed frame and it escapes to a helper
# (`call stomp8 ;! void(ptr)`), so the whole fixed frame is materialized as ONE
# GC allocation (filc_allocate in the sarcasm output — expected here) and every
# fixed-frame access redirects into it. This test pins the keying of accesses
# made at PERTURBED rsp depths (mid-body pushes) and through the prologue
# frame pointer:
#   * an access at a shifted depth keys disp + D0 - d: a post-push
#     `movq $33, 40(%rsp)` must hit the SAME region slot as the pre-push
#     `movq $22, 32(%rsp)` (aliased writes overwrite);
#   * two nested pushes key disp + D0 - d - 16 the same way;
#   * rbp-relative accesses (rbp established in the prologue) key into the
#     same region at any depth and agree with rsp-relative spellings;
#   * the spill-push slots themselves live BELOW the region base (the
#     hardware red zone), so the pops return exactly the pushed values.
# All frame slots and derived-pointer traffic hit one home; the checksum
# below is exact (979) and is mirrored slot-for-slot in the C driver.
	.text
	.globl	depthesc
	.type	depthesc, @function
depthesc:                       #! long(long)
	pushq	%rbp
	movq	%rsp, %rbp          # frame pointer: rbpNorm = 136 (D0 144 - fpDepth 8)
	pushq	%rbx
	subq	$128, %rsp          # fixed frame [0,128): D0 = 144, region [0,136)

	# --- escape cluster at the frame-base depth: the lea escapes to the
	# helper, which promotes the whole frame to one GC region ---
	leaq	16(%rsp), %rdi      # region+16 — the ESCAPING lea
	movq	%rdi, %rsi          # copy of the frame pointer
	leaq	8(%rsi), %rdx       # offset of the copy: region+24
	movq	$10, (%rdi)         # slot16 = 10
	movq	$11, (%rsi)         # slot16 = 11 (aliased overwrite via the copy)
	movq	$12, (%rdx)         # slot24 = 12
	call	stomp8 ;! void(ptr) # helper: slot16..slot40 = 256..259

	# --- direct slots at the frame-base depth ---
	movq	$22, 32(%rsp)       # slot32 = 22 (over the helper's 258)
	movq	$23, 40(%rsp)       # slot40 = 23
	movq	$24, 48(%rsp)       # slot48 = 24
	movq	32(%rsp), %rax      # 22
	addq	40(%rsp), %rax      # +23 = 45
	movq	%rax, %rbx          # acc = 45

	# --- FIRST depth shift: one push (+8) ---
	pushq	%rax                # parks 45 BELOW the region (red zone slot)
	movq	$33, 40(%rsp)       # 40 @ depth+8 = slot32 = 33 (ALIAS: kills 22)
	movq	$34, 48(%rsp)       # 48 @ depth+8 = slot40 = 34
	movq	$35, 56(%rsp)       # 56 @ depth+8 = slot48 = 35
	movq	$13, 24(%rsp)       # 24 @ depth+8 = slot16 = 13
	movq	40(%rsp), %rcx      # 33
	addq	48(%rsp), %rcx      # +34 = 67
	addq	56(%rsp), %rcx      # +35 = 102
	addq	%rcx, %rbx          # acc = 147
	movq	$25, -96(%rbp)      # rbpNorm 136 - 96 = slot40 = 25 (rbp write)
	movq	-88(%rbp), %rcx     # slot48 = 35
	addq	%rcx, %rbx          # acc = 182
	movq	48(%rsp), %rcx      # 48 @ depth+8 = slot40 = 25 (rbp/rsp agree)
	addq	%rcx, %rbx          # acc = 207
	popq	%rax                # the pushed 45 (parked below the region)
	movq	32(%rsp), %rcx      # slot32 = 33 (post-pop alias read)
	addq	%rcx, %rbx          # acc = 240
	addq	%rax, %rbx          # acc = 285 (pop returns the pushed value)

	# --- SECOND depth shift: two nested pushes (+16 total) ---
	movq	$77, %rcx
	pushq	%rcx                # parks 77 below the region
	pushq	%rcx                # parks 77 deeper below the region
	movq	$44, 56(%rsp)       # 56 @ depth+16 = slot40 = 44
	movq	$45, 64(%rsp)       # 64 @ depth+16 = slot48 = 45
	movq	$14, 32(%rsp)       # 32 @ depth+16 = slot16 = 14
	movq	$26, -104(%rbp)     # rbpNorm 136 - 104 = slot32 = 26
	movq	56(%rsp), %rdx      # 44
	addq	64(%rsp), %rdx      # +45 = 89
	addq	32(%rsp), %rdx      # +14 = 103
	addq	-104(%rbp), %rdx    # +26 = 129
	addq	%rdx, %rbx          # acc = 414
	popq	%rcx
	popq	%rcx                # depth back to D0; rcx = 77 (pushed value)
	addq	%rcx, %rbx          # acc = 491

	# --- final readback at the frame-base depth ---
	movq	16(%rsp), %rcx      # slot16 = 14
	addq	%rcx, %rbx          # acc = 505
	movq	24(%rsp), %rcx      # slot24 = 257 (helper's 0x101 survived)
	addq	%rcx, %rbx          # acc = 762
	movq	32(%rsp), %rcx      # slot32 = 26
	addq	%rcx, %rbx          # acc = 788
	movq	40(%rsp), %rcx      # slot40 = 44
	addq	%rcx, %rbx          # acc = 832
	movq	48(%rsp), %rcx      # slot48 = 45
	addq	%rcx, %rbx          # acc = 877
	movq	$51, -64(%rbp)      # rbpNorm 136 - 64 = slot72 = 51
	movq	72(%rsp), %rcx      # slot72 via rsp = 51
	addq	%rcx, %rbx          # acc = 928
	movq	-64(%rbp), %rcx     # slot72 via rbp = 51
	addq	%rcx, %rbx          # acc = 979
	movq	%rbx, %rax
	addq	$128, %rsp
	popq	%rbx
	popq	%rbp
	ret
	.size	depthesc, .-depthesc
	.section	.note.GNU-stack,"",@progbits
