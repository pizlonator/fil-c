# Intel-syntax input with the Intel/dword spellings: pushfd/popfd are accepted
# as aliases of the 64-bit pushf/popf (64-bit mode has no 4-byte RFLAGS push;
# the only encoding those spellings can name is 0x9C/0x9D). The renderer
# normalizes the emitted materialization/restore to the AT&T pushfq/popfq, so
# the input spelling never survives to `as`.

	.globl	pi_eq_restore
	.type	pi_eq_restore, @function
pi_eq_restore:                  ;! long(long, long)
	mov	rax, rdi
	cmp	rax, 0
	pushfd
	add	rsi, 17
	inc	rsi
	popfd
	je	.Lint_waszero
	mov	rax, rsi
	ret
.Lint_waszero:
	mov	rax, rsi
	add	rax, 111
	ret
	.size	pi_eq_restore, .-pi_eq_restore

# The bare (suffix-less) spellings pushf/popf.
	.globl	pi_bare
	.type	pi_bare, @function
pi_bare:                        ;! long(long)
	mov	rax, rdi
	cmp	rax, 0
	pushf
	add	rdi, 3
	popf
	je	.Lbare_zero
	mov	rax, 1
	ret
.Lbare_zero:
	mov	rax, 2
	ret
	.size	pi_bare, .-pi_bare

# popfd loading the flags FROM A REGISTER: build a condition word (ZF in bit
# 6), push it, popfd it, and branch on the defined result.
	.globl	pi_from_reg
	.type	pi_from_reg, @function
pi_from_reg:                    ;! long(long)
	xor	rax, rax
	cmp	rdi, 0
	sete	al
	shl	rax, 6             # 0 or 64: bit 6 is ZF
	push	rax
	popfd
	je	.Lfr_zero
	mov	rax, 1
	ret
.Lfr_zero:
	mov	rax, 2
	ret
	.size	pi_from_reg, .-pi_from_reg
	.section	.note.GNU-stack,"",@progbits
