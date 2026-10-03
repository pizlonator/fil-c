# D9 fixed-frame escape promotion, pinned for RED-ZONE traffic coexisting with
# the promoted region. The escape cluster (`leaq 16(%rsp), %rdi` +
# `call fill32 ;! void(ptr)`) promotes the whole fixed frame to ONE GC
# allocation (filc_allocate in the sarcasm output — expected here) whose
# payload is the region [0,64). Below the region base, the promoted frame still
# has its synthesized frame: a red-zone slot at -8(%rsp) is accepted, rides the
# SYNTHESIZED frame (never the region's capability), and survives the escaping
# helper call untouched. The test:
#   * writes region traffic through derived pointers (slot16/slot24);
#   * does a red-zone RMW (`movq $0x77, -8(%rsp)` then `addq %rcx, -8(%rsp)` —
#     0x177 = 375) at a spot the helper's region writes cannot reach;
#   * after the call, reads the red-zone slot back (375 — the call did not
#     disturb it) AND the region slots through both spellings (a fresh derived
#     pointer and the direct rsp slots, one home), and checksums exactly
#     (375 + 100 + 101 + 101 + 102 + 103 + 100 = 982).
	.text
	.globl	redesc
	.type	redesc, @function
redesc:                         #! long(long)
	pushq	%rbx
	subq	$64, %rsp           # plain fixed frame [0,64): D0 = 72, region [0,64)
	leaq	16(%rsp), %rdi      # region+16 — the ESCAPING lea: promotion
	movq	%rdi, %rsi          # copy of the frame pointer
	leaq	8(%rsi), %rdx       # offset of the copy: region+24
	movq	$4111, (%rdi)       # region traffic: slot16
	movq	$4222, (%rdx)       # region traffic: slot24
	movq	$0x77, -8(%rsp)     # RED-ZONE traffic: rides the SYNTHESIZED frame
	movq	$0x100, %rcx
	addq	%rcx, -8(%rsp)      # RMW on the red-zone slot: 0x77 + 0x100 = 0x177 (375)
	call	fill32 ;! void(ptr) # the escape: 100..103 at region+16..47; the red zone rides it out

	# --- readback: red zone and region, both spellings ---
	movq	-8(%rsp), %rcx      # red-zone readback: 375
	movq	%rcx, %rbx
	leaq	16(%rsp), %rax      # fresh derived pointer: region+16 (post-call)
	movq	(%rax), %rcx        # derived: slot16 = 100 (the helper's write)
	addq	%rcx, %rbx
	movq	8(%rax), %rcx       # derived: slot24 = 101
	addq	%rcx, %rbx
	movq	24(%rsp), %rcx      # direct slot read: slot24 again (one home)
	addq	%rcx, %rbx
	movq	32(%rsp), %rcx      # direct: slot32 = 102
	addq	%rcx, %rbx
	movq	40(%rsp), %rcx      # direct: slot40 = 103
	addq	%rcx, %rbx
	movq	16(%rsp), %rcx      # direct: slot16 again (one home)
	addq	%rcx, %rbx
	movq	%rbx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	redesc, .-redesc
	.section	.note.GNU-stack,"",@progbits
