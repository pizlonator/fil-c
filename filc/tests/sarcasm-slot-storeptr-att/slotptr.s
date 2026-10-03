	.text
	# Frame slots CAN hold pointers with capabilities: `#! store ptr` /
	# `;! load ptr` on a plain 8-byte register<->slot move are accepted by the
	# frame pass. A virtualized slot is an ordinary GPR web, so the capability
	# rides the same pointer flow a register move uses -- the store hands the
	# source web's capability to the slot web, and the load hands the slot
	# web's capability to the destination web.
	.globl	slot_roundtrip
	.type	slot_roundtrip, @function
slot_roundtrip:                 ;! long(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)     ;! store ptr
	movq	-8(%rbp), %rax     ;! load ptr
	movq	(%rax), %rax       # deref: reads the stored value with the slot's capability
	leave
	ret
	.size	slot_roundtrip, .-slot_roundtrip

	# A second roundtrip through a %rsp-relative slot (the same web as no
	# other access), with the pointer written and re-read at a different
	# frame offset than the first function -- both spellings virtualize.
	.globl	slot_roundtrip_rsp
	.type	slot_roundtrip_rsp, @function
slot_roundtrip_rsp:             ;! long(ptr)
	subq	$32, %rsp
	movq	%rdi, 16(%rsp)     ;! store ptr
	movq	16(%rsp), %rax     ;! load ptr
	movq	8(%rax), %rax
	addq	$32, %rsp
	ret
	.size	slot_roundtrip_rsp, .-slot_roundtrip_rsp
	.section	.note.GNU-stack,"",@progbits
