	.text
	# `use capability %reg` on memory operands: the access is guarded by the
	# NAMED register's capability, which must be the memory operand's base
	# or index. Below, `%rdi` is zeroed (a scalar 0 on a stale pointer
	# web), so the base-first default would guard with the wrong object;
	# naming the live index keeps the access on the right capability.
	# (The x86_64-xlate.pl motivation: it flips `disp(%r13,%rdi)` to
	# `disp(%rdi,%r13)` for performance — the address is unchanged, but
	# the logical base now sits in index position.)
	.globl	usecapmem_store
	.type	usecapmem_store, @function
usecapmem_store:                ;! void(ptr,ptr,long)
	endbr64
	xorq	%rdi, %rdi
	movq	%rdx, (%rdi,%rsi) #! use capability %rsi
	ret
	.size	usecapmem_store, .-usecapmem_store
	.globl	usecapmem_load
	.type	usecapmem_load, @function
usecapmem_load:                 ;! long(ptr,ptr)
	endbr64
	xorq	%rdi, %rdi
	movq	(%rdi,%rsi), %rax #! use capability %rsi
	ret
	.size	usecapmem_load, .-usecapmem_load
	.globl	usecapmem_disp_store
	.type	usecapmem_disp_store, @function
	# Same, with a displacement: the address is base+index+disp as written,
	# only the capability source changes.
usecapmem_disp_store:           ;! void(ptr,ptr,long)
	endbr64
	xorq	%rdi, %rdi
	movq	%rdx, 16(%rdi,%rsi) #! use capability %rsi
	ret
	.size	usecapmem_disp_store, .-usecapmem_disp_store
	.globl	usecapmem_disp_load
	.type	usecapmem_disp_load, @function
usecapmem_disp_load:            ;! long(ptr,ptr)
	endbr64
	xorq	%rdi, %rdi
	movq	16(%rdi,%rsi), %rax #! use capability %rsi
	ret
	.size	usecapmem_disp_load, .-usecapmem_disp_load
	.globl	usecapmem_vcopy
	.type	usecapmem_vcopy, @function
	# Vector form: 32-byte copy whose load names the live index.
usecapmem_vcopy:                ;! void(ptr,ptr,ptr)
	endbr64
	xorq	%rdi, %rdi
	vmovdqu	(%rdi,%rsi), %ymm0 #! use capability %rsi
	vmovdqu	%ymm0, (%rdx)
	vzeroupper
	ret
	.size	usecapmem_vcopy, .-usecapmem_vcopy
	.globl	usecapmem_vstore_base
	.type	usecapmem_vstore_base, @function
	# Naming the base: guards with the written base's capability while the
	# index is a zeroed stale web (the xlate-flip mirror image).
usecapmem_vstore_base:          ;! void(ptr,ptr,ptr)
	endbr64
	vmovdqu	(%rdx), %ymm0
	xorq	%rdx, %rdx
	vmovdqu	%ymm0, (%rsi,%rdx) #! use capability %rsi
	vzeroupper
	ret
	.size	usecapmem_vstore_base, .-usecapmem_vstore_base
	.section	.note.GNU-stack,"",@progbits
