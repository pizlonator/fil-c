	.text
	# `new capability %reg` selects which register's capability guards a
	# memory access: the named register must be the memory operand's base
	# or index. Below, `%rdi` is zeroed (a scalar 0 on a stale pointer
	# web), so the base-first default would guard with the wrong object;
	# naming the live index keeps the access on the right capability.
	# (The x86_64-xlate.pl motivation: it flips `disp(%r13,%rdi)` to
	# `disp(%rdi,%r13)` for performance — the address is unchanged, but
	# the logical base now sits in index position.)
	.globl	newcap_store
	.type	newcap_store, @function
newcap_store:                   ;! void(ptr,ptr,long)
	endbr64
	xorq	%rdi, %rdi
	movq	%rdx, (%rdi,%rsi) #! new capability %rsi
	ret
	.size	newcap_store, .-newcap_store
	.globl	newcap_load
	.type	newcap_load, @function
newcap_load:                    ;! long(ptr,ptr)
	endbr64
	xorq	%rdi, %rdi
	movq	(%rdi,%rsi), %rax #! new capability %rsi
	ret
	.size	newcap_load, .-newcap_load
	.globl	newcap_typo
	.type	newcap_typo, @function
	# The historical typo spelling `new capabiltiy` is accepted too.
newcap_typo:                    ;! long(ptr,ptr)
	endbr64
	xorq	%rdi, %rdi
	movq	(%rdi,%rsi), %rax #! new capabiltiy %rsi
	ret
	.size	newcap_typo, .-newcap_typo
	.section	.note.GNU-stack,"",@progbits
