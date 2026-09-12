	.intel_syntax noprefix
# Intel-syntax twin of sarcasm-partialreg-att (see it for the semantics
# notes): an 8/16-bit write to a low register read-modifies the enclosing
# 64-bit web — a prior zero-def must stay live across it; movzx is a full def.
	.text
	.globl	pr_movb
	.type	pr_movb, @function
pr_movb:                        ;! long(long)
	endbr64
	xor	rax, rax
	mov	al, dil
	ret
	.size	pr_movb, .-pr_movb

	.globl	pr_movl0
	.type	pr_movl0, @function
pr_movl0:                       ;! long(long)
	endbr64
	mov	eax, 0
	mov	al, dil
	ret
	.size	pr_movl0, .-pr_movl0

	.globl	pr_movw
	.type	pr_movw, @function
pr_movw:                        ;! long(long)
	endbr64
	xor	rax, rax
	mov	ax, di
	ret
	.size	pr_movw, .-pr_movw

	.globl	pr_movb_preserve
	.type	pr_movb_preserve, @function
pr_movb_preserve:               ;! long(long)
	endbr64
	mov	rax, 0x1122334455667788
	mov	al, dil
	ret
	.size	pr_movb_preserve, .-pr_movb_preserve

	.globl	pr_movw_preserve
	.type	pr_movw_preserve, @function
pr_movw_preserve:               ;! long(long)
	endbr64
	mov	rax, 0x1122334455667788
	mov	ax, di
	ret
	.size	pr_movw_preserve, .-pr_movw_preserve

	.globl	pr_movb_imm
	.type	pr_movb_imm, @function
pr_movb_imm:                    ;! long()
	endbr64
	mov	rax, 0x1122334455667788
	mov	al, 0xAB
	ret
	.size	pr_movb_imm, .-pr_movb_imm

	.globl	pr_movw_imm
	.type	pr_movw_imm, @function
pr_movw_imm:                    ;! long()
	endbr64
	mov	rax, 0x1122334455667788
	mov	ax, 0xABCD
	ret
	.size	pr_movw_imm, .-pr_movw_imm

	.globl	pr_addb
	.type	pr_addb, @function
pr_addb:                        ;! long(long, long)
	endbr64
	mov	rax, rdi
	add	al, sil
	ret
	.size	pr_addb, .-pr_addb

	.globl	pr_xor_addb
	.type	pr_xor_addb, @function
pr_xor_addb:                    ;! long(long)
	endbr64
	xor	rax, rax
	add	al, dil
	ret
	.size	pr_xor_addb, .-pr_xor_addb

	.globl	pr_andb
	.type	pr_andb, @function
pr_andb:                        ;! long(long, long)
	endbr64
	mov	rax, rdi
	and	al, sil
	ret
	.size	pr_andb, .-pr_andb

	.globl	pr_orb
	.type	pr_orb, @function
pr_orb:                         ;! long(long, long)
	endbr64
	mov	rax, rdi
	or	al, sil
	ret
	.size	pr_orb, .-pr_orb

	.globl	pr_movzbl
	.type	pr_movzbl, @function
pr_movzbl:                      ;! long(long)
	endbr64
	xor	rax, rax
	movzx	eax, dil
	ret
	.size	pr_movzbl, .-pr_movzbl

	.globl	pr_movzwl
	.type	pr_movzwl, @function
pr_movzwl:                      ;! long(long)
	endbr64
	xor	rax, rax
	movzx	eax, di
	ret
	.size	pr_movzwl, .-pr_movzwl

	.globl	pr_movzbq
	.type	pr_movzbq, @function
pr_movzbq:                      ;! long(long)
	endbr64
	mov	rax, -1
	movzx	rax, dil
	ret
	.size	pr_movzbq, .-pr_movzbq

	.globl	pr_index
	.type	pr_index, @function
pr_index:                       ;! long(long, ptr)
	endbr64
	xor	rax, rax
	mov	al, dil
	mov	rax, QWORD PTR [rsi+rax*8]
	ret
	.size	pr_index, .-pr_index

	.globl	pr_index_shl
	.type	pr_index_shl, @function
pr_index_shl:                   ;! long(long, ptr)
	endbr64
	xor	rax, rax
	mov	al, dil
	shl	al, 2
	mov	rax, QWORD PTR [rsi+rax*8]
	ret
	.size	pr_index_shl, .-pr_index_shl

	.globl	pr_movw_index
	.type	pr_movw_index, @function
pr_movw_index:                  ;! long(long, ptr)
	endbr64
	xor	rax, rax
	mov	ax, di
	and	ax, 0xff
	mov	rax, QWORD PTR [rsi+rax*8]
	ret
	.size	pr_movw_index, .-pr_movw_index

	.globl	pr_branch
	.type	pr_branch, @function
pr_branch:                      ;! long(long, long)
	endbr64
	test	rdi, rdi
	je	.Lalt
	xor	rax, rax
	mov	al, sil
	ret
.Lalt:
	mov	rax, rdi
	ret
	.size	pr_branch, .-pr_branch

	.globl	pr_branch2
	.type	pr_branch2, @function
pr_branch2:                     ;! long(long, long)
	endbr64
	xor	rax, rax
	test	rdi, rdi
	je	.Lskip2
	mov	al, sil
.Lskip2:
	ret
	.size	pr_branch2, .-pr_branch2

	.globl	pr_loop
	.type	pr_loop, @function
pr_loop:                        ;! long(long, ptr)
	endbr64
	xor	rax, rax
	mov	al, dil
	mov	ecx, 3
	xor	edx, edx
.Lloop:
	add	rdx, QWORD PTR [rsi+rax*8]
	sub	rcx, 1
	jne	.Lloop
	mov	rax, rdx
	ret
	.size	pr_loop, .-pr_loop

	.globl	pr_slot
	.type	pr_slot, @function
pr_slot:                        ;! long(long, long)
	endbr64
	sub	rsp, 24
	mov	QWORD PTR [rsp+8], rdi
	mov	BYTE PTR [rsp+8], sil
	mov	rax, QWORD PTR [rsp+8]
	add	rsp, 24
	ret
	.size	pr_slot, .-pr_slot

	.globl	pr_slotw
	.type	pr_slotw, @function
pr_slotw:                       ;! long(long, long)
	endbr64
	sub	rsp, 24
	mov	QWORD PTR [rsp+8], rdi
	mov	WORD PTR [rsp+8], si
	mov	rax, QWORD PTR [rsp+8]
	add	rsp, 24
	ret
	.size	pr_slotw, .-pr_slotw

	.globl	pr_setcc_slot
	.type	pr_setcc_slot, @function
pr_setcc_slot:                  ;! long(long, long)
	endbr64
	sub	rsp, 24
	mov	QWORD PTR [rsp+8], rdi
	cmp	rsi, rsi
	sete	BYTE PTR [rsp+8]
	mov	rax, QWORD PTR [rsp+8]
	add	rsp, 24
	ret
	.size	pr_setcc_slot, .-pr_setcc_slot

	.globl	pr_cwd
	.type	pr_cwd, @function
pr_cwd:                         ;! long(long)
	endbr64
	mov	rax, rdi
	mov	rdx, 0x1122334455660000
	cwd
	mov	rax, rdx
	ret
	.size	pr_cwd, .-pr_cwd

	.globl	pr_mulw
	.type	pr_mulw, @function
pr_mulw:                        ;! long(long, long)
	endbr64
	mov	rax, rdi
	mov	rdx, 0x1122334455660000
	mul	si
	mov	rax, rdx
	ret
	.size	pr_mulw, .-pr_mulw

	.globl	pr_fstsw
	.type	pr_fstsw, @function
pr_fstsw:                       ;! long()
	endbr64
	mov	rax, 0x1122334455660000
	fldz
	fldz
	fucom	st(1)
	fnstsw	ax
	fstp	st(0)
	fstp	st(0)
	ret
	.size	pr_fstsw, .-pr_fstsw

	# Intel-syntax twins of the xor-zero + narrow-def + wider-use cases
	# (see sarcasm-partialreg-att for the semantics notes).
	.globl	pr_xor_movb_32use
	.type	pr_xor_movb_32use, @function
pr_xor_movb_32use:              ;! long(long)
	endbr64
	mov	rax, 0x1122334455667788
	xor	rax, rax
	mov	al, dil
	mov	edx, eax
	mov	rax, rdx
	ret
	.size	pr_xor_movb_32use, .-pr_xor_movb_32use

	.globl	pr_xor_movw_32use
	.type	pr_xor_movw_32use, @function
pr_xor_movw_32use:              ;! long(long)
	endbr64
	mov	rax, 0x1122334455667788
	xor	rax, rax
	mov	ax, di
	mov	edx, eax
	mov	rax, rdx
	ret
	.size	pr_xor_movw_32use, .-pr_xor_movw_32use

	.globl	pr_xor_movw_cx_hi16
	.type	pr_xor_movw_cx_hi16, @function
pr_xor_movw_cx_hi16:            ;! long(long)
	endbr64
	mov	rcx, 0x1122334455667788
	xor	rcx, rcx
	mov	cx, di
	shr	rcx, 16
	mov	rax, rcx
	ret
	.size	pr_xor_movw_cx_hi16, .-pr_xor_movw_cx_hi16

	.globl	pr_loop_redef
	.type	pr_loop_redef, @function
pr_loop_redef:                  ;! long(long, ptr)
	endbr64
	xor	rax, rax
	mov	al, dil
	xor	r10, r10
	mov	ecx, 8
.Lredef:
	mov	r8, QWORD PTR [rsi+rax*8]
	xor	r10, r8
	rol	rdi, 8
	mov	al, dil
	shl	al, 4
	sub	rcx, 1
	jne	.Lredef
	mov	rax, r10
	ret
	.size	pr_loop_redef, .-pr_loop_redef

	.section	.note.GNU-stack,"",@progbits
