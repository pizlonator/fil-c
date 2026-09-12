# Partial-register-write liveness: an 8/16-bit write to a LOW register
# (%al/%ax & co) preserves the upper bits of the enclosing 64-bit register —
# it is a read-modify-write of the register web, never a full kill. A prior
# zero-def (xorq/movl $0) or any other value feeding the preserved bits must
# stay live across the partial write; otherwise the later full-width read
# (return value, table index) observes garbage upper bits. movzbl/movzwl ARE
# full defs (a zero/sign-extending write to a 32/64-bit register defines the
# whole web), so the zeroing before THOSE may still be eliminated — the value
# must be right either way. (The ghash miscompile: `xorq %rax,%rax` dropped
# ahead of a literal `movb %sil,%al` left garbage in an index register.)
	.text
	.globl	pr_movb
	.type	pr_movb, @function
pr_movb:                        ;! long(long)
	endbr64
	xorq	%rax, %rax
	movb	%dil, %al
	ret
	.size	pr_movb, .-pr_movb

	.globl	pr_movl0
	.type	pr_movl0, @function
pr_movl0:                       ;! long(long)
	endbr64
	movl	$0, %eax
	movb	%dil, %al
	ret
	.size	pr_movl0, .-pr_movl0

	.globl	pr_movw
	.type	pr_movw, @function
pr_movw:                        ;! long(long)
	endbr64
	xorq	%rax, %rax
	movw	%di, %ax
	ret
	.size	pr_movw, .-pr_movw

	.globl	pr_movb_preserve
	.type	pr_movb_preserve, @function
pr_movb_preserve:               ;! long(long)
	endbr64
	movq	$0x1122334455667788, %rax
	movb	%dil, %al
	ret
	.size	pr_movb_preserve, .-pr_movb_preserve

	.globl	pr_movw_preserve
	.type	pr_movw_preserve, @function
pr_movw_preserve:               ;! long(long)
	endbr64
	movq	$0x1122334455667788, %rax
	movw	%di, %ax
	ret
	.size	pr_movw_preserve, .-pr_movw_preserve

	.globl	pr_movb_imm
	.type	pr_movb_imm, @function
pr_movb_imm:                    ;! long()
	endbr64
	movq	$0x1122334455667788, %rax
	movb	$0xAB, %al
	ret
	.size	pr_movb_imm, .-pr_movb_imm

	.globl	pr_movw_imm
	.type	pr_movw_imm, @function
pr_movw_imm:                    ;! long()
	endbr64
	movq	$0x1122334455667788, %rax
	movw	$0xABCD, %ax
	ret
	.size	pr_movw_imm, .-pr_movw_imm

	.globl	pr_addb
	.type	pr_addb, @function
pr_addb:                        ;! long(long, long)
	endbr64
	movq	%rdi, %rax
	addb	%sil, %al
	ret
	.size	pr_addb, .-pr_addb

	.globl	pr_xor_addb
	.type	pr_xor_addb, @function
pr_xor_addb:                    ;! long(long)
	endbr64
	xorq	%rax, %rax
	addb	%dil, %al
	ret
	.size	pr_xor_addb, .-pr_xor_addb

	.globl	pr_andb
	.type	pr_andb, @function
pr_andb:                        ;! long(long, long)
	endbr64
	movq	%rdi, %rax
	andb	%sil, %al
	ret
	.size	pr_andb, .-pr_andb

	.globl	pr_orb
	.type	pr_orb, @function
pr_orb:                         ;! long(long, long)
	endbr64
	movq	%rdi, %rax
	orb	%sil, %al
	ret
	.size	pr_orb, .-pr_orb

	# movzbl/movzwl are FULL defs: the zeroing above them is genuinely
	# redundant and may be eliminated — the result must be exact regardless.
	.globl	pr_movzbl
	.type	pr_movzbl, @function
pr_movzbl:                      ;! long(long)
	endbr64
	xorq	%rax, %rax
	movzbl	%dil, %eax
	ret
	.size	pr_movzbl, .-pr_movzbl

	.globl	pr_movzwl
	.type	pr_movzwl, @function
pr_movzwl:                      ;! long(long)
	endbr64
	xorq	%rax, %rax
	movzwl	%di, %eax
	ret
	.size	pr_movzwl, .-pr_movzwl

	.globl	pr_movzbq
	.type	pr_movzbq, @function
pr_movzbq:                      ;! long(long)
	endbr64
	movq	$-1, %rax
	movzbq	%dil, %rax
	ret
	.size	pr_movzbq, .-pr_movzbq

	# the ghash shape: zeroed register, byte write, full-register index
	.globl	pr_index
	.type	pr_index, @function
pr_index:                       ;! long(long, ptr)
	endbr64
	xorq	%rax, %rax
	movb	%dil, %al
	movq	(%rsi,%rax,8), %rax
	ret
	.size	pr_index, .-pr_index

	# ghash's shl on the low byte between the byte write and the index
	.globl	pr_index_shl
	.type	pr_index_shl, @function
pr_index_shl:                   ;! long(long, ptr)
	endbr64
	xorq	%rax, %rax
	movb	%dil, %al
	shlb	$2, %al
	movq	(%rsi,%rax,8), %rax
	ret
	.size	pr_index_shl, .-pr_index_shl

	# 16-bit partial writes feeding a full-register index
	.globl	pr_movw_index
	.type	pr_movw_index, @function
pr_movw_index:                  ;! long(long, ptr)
	endbr64
	xorq	%rax, %rax
	movw	%di, %ax
	andw	$0xff, %ax
	movq	(%rsi,%rax,8), %rax
	ret
	.size	pr_movw_index, .-pr_movw_index

	# zeroing + partial write inside a taken branch
	.globl	pr_branch
	.type	pr_branch, @function
pr_branch:                      ;! long(long, long)
	endbr64
	testq	%rdi, %rdi
	je	.Lalt
	xorq	%rax, %rax
	movb	%sil, %al
	ret
.Lalt:
	movq	%rdi, %rax
	ret
	.size	pr_branch, .-pr_branch

	# zeroing dominating a branch; partial write on one side; merge after
	.globl	pr_branch2
	.type	pr_branch2, @function
pr_branch2:                     ;! long(long, long)
	endbr64
	xorq	%rax, %rax
	testq	%rdi, %rdi
	je	.Lskip2
	movb	%sil, %al
.Lskip2:
	ret
	.size	pr_branch2, .-pr_branch2

	# the byte write's full-register read inside a loop body
	.globl	pr_loop
	.type	pr_loop, @function
pr_loop:                        ;! long(long, ptr)
	endbr64
	xorq	%rax, %rax
	movb	%dil, %al
	movl	$3, %ecx
	xorl	%edx, %edx
.Lloop:
	addq	(%rsi,%rax,8), %rdx
	subq	$1, %rcx
	jne	.Lloop
	movq	%rdx, %rax
	ret
	.size	pr_loop, .-pr_loop

	# a narrow store to a frame slot preserves the slot's other bytes
	.globl	pr_slot
	.type	pr_slot, @function
pr_slot:                        ;! long(long, long)
	endbr64
	subq	$24, %rsp
	movq	%rdi, 8(%rsp)
	movb	%sil, 8(%rsp)
	movq	8(%rsp), %rax
	addq	$24, %rsp
	ret
	.size	pr_slot, .-pr_slot

	.globl	pr_slotw
	.type	pr_slotw, @function
pr_slotw:                       ;! long(long, long)
	endbr64
	subq	$24, %rsp
	movq	%rdi, 8(%rsp)
	movw	%si, 8(%rsp)
	movq	8(%rsp), %rax
	addq	$24, %rsp
	ret
	.size	pr_slotw, .-pr_slotw

	# setcc to a frame slot writes ONE byte of the slot, no more
	.globl	pr_setcc_slot
	.type	pr_setcc_slot, @function
pr_setcc_slot:                  ;! long(long, long)
	endbr64
	subq	$24, %rsp
	movq	%rdi, 8(%rsp)
	cmpq	%rsi, %rsi
	sete	8(%rsp)
	movq	8(%rsp), %rax
	addq	$24, %rsp
	ret
	.size	pr_setcc_slot, .-pr_setcc_slot

	# cwd writes only %dx; the upper 48 bits of rdx are preserved
	.globl	pr_cwd
	.type	pr_cwd, @function
pr_cwd:                         ;! long(long)
	endbr64
	movq	%rdi, %rax
	movq	$0x1122334455660000, %rdx
	cwd
	movq	%rdx, %rax
	ret
	.size	pr_cwd, .-pr_cwd

	# mulw writes only %dx of rdx (the product's high half lives there)
	.globl	pr_mulw
	.type	pr_mulw, @function
pr_mulw:                        ;! long(long, long)
	endbr64
	movq	%rdi, %rax
	movq	$0x1122334455660000, %rdx
	mulw	%si
	movq	%rdx, %rax
	ret
	.size	pr_mulw, .-pr_mulw

	# fstsw writes only %ax; the upper 48 bits of rax are preserved
	.globl	pr_fstsw
	.type	pr_fstsw, @function
pr_fstsw:                       ;! long()
	endbr64
	movq	$0x1122334455660000, %rax
	fldz
	fldz
	fucom	%st(1)
	fnstsw	%ax
	fstp	%st(0)
	fstp	%st(0)
	ret
	.size	pr_fstsw, .-pr_fstsw

	# xor-zeroing, an 8-bit def, then a 32-bit USE: bits 8-31 of the low 32
	# are preserved by the movb, so they still read the zeroing — the xor
	# must stay live across the movb. (The ghash shape: `xorq %rax,%rax` /
	# `movb %sil,%al` feeding a wider read.) The leading constant makes a
	# dropped xor deterministically observable.
	.globl	pr_xor_movb_32use
	.type	pr_xor_movb_32use, @function
pr_xor_movb_32use:              ;! long(long)
	endbr64
	movq	$0x1122334455667788, %rax
	xorq	%rax, %rax
	movb	%dil, %al
	movl	%eax, %edx
	movq	%rdx, %rax
	ret
	.size	pr_xor_movb_32use, .-pr_xor_movb_32use

	# xor-zeroing, a 16-bit def, then a 32-bit use: bits 16-31 are preserved
	# by the movw and still read the zeroing.
	.globl	pr_xor_movw_32use
	.type	pr_xor_movw_32use, @function
pr_xor_movw_32use:              ;! long(long)
	endbr64
	movq	$0x1122334455667788, %rax
	xorq	%rax, %rax
	movw	%di, %ax
	movl	%eax, %edx
	movq	%rdx, %rax
	ret
	.size	pr_xor_movw_32use, .-pr_xor_movw_32use

	# a 16-bit def to %cx preserves the HIGH 16 of %ecx: after xor+movw the
	# high half reads back zero.
	.globl	pr_xor_movw_cx_hi16
	.type	pr_xor_movw_cx_hi16, @function
pr_xor_movw_cx_hi16:            ;! long(long)
	endbr64
	movq	$0x1122334455667788, %rcx
	xorq	%rcx, %rcx
	movw	%di, %cx
	shrq	$16, %rcx
	movq	%rcx, %rax
	ret
	.size	pr_xor_movw_cx_hi16, .-pr_xor_movw_cx_hi16

	# ghash inner-loop shape: ONE xor-zeroing outside the loop, then the low
	# byte is redefined (an RMW) and used as a full-register table index on
	# every iteration — the upper bits stay zero across all iterations.
	.globl	pr_loop_redef
	.type	pr_loop_redef, @function
pr_loop_redef:                  ;! long(long, ptr)
	endbr64
	xorq	%rax, %rax
	movb	%dil, %al
	xorq	%r10, %r10
	movl	$8, %ecx
.Lredef:
	movq	(%rsi,%rax,8), %r8
	xorq	%r8, %r10
	rolq	$8, %rdi
	movb	%dil, %al
	shlb	$4, %al
	subq	$1, %rcx
	jne	.Lredef
	movq	%r10, %rax
	ret
	.size	pr_loop_redef, .-pr_loop_redef

	.section	.note.GNU-stack,"",@progbits
