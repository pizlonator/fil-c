	.text
	# `new capability` (and the historical typo spellings `new capabiltiy` /
	# `use capabiltiy`) are deprecated aliases for `use capability`: every
	# spelling validates and behaves identically — on pure arithmetic, on
	# plain memory accesses (scalar and vector), and on dual-effect
	# arithmetic-with-memory instructions.
	.globl	alias_add
	.type	alias_add, @function
	# Alias on pure arithmetic: both inputs carry capabilities (the stale
	# second-pointer web idiom — `sub` of two pointers is an integer, but
	# the web stays sticky, so the `add` sees two origins). The annotation
	# keeps `%rdi`'s.
alias_add:                      ;! long(ptr,ptr)
	endbr64
	movq	%rsi, %rax
	subq	%rsi, %rax
	addq	%rax, %rdi #! new capability %rdi
	movq	(%rdi), %rax
	ret
	.size	alias_add, .-alias_add
	.globl	alias_sub_typo
	.type	alias_sub_typo, @function
	# Typo spelling on pure arithmetic (a single live source — the
	# annotation is unnecessary but must still validate and preserve).
alias_sub_typo:                 ;! long(ptr,long)
	endbr64
	subq	%rsi, %rdi #! new capabiltiy %rdi
	movq	(%rdi), %rax
	ret
	.size	alias_sub_typo, .-alias_sub_typo
	.globl	alias_mov_new
	.type	alias_mov_new, @function
	# Alias on a plain load: `%rdi` is zeroed (a scalar 0 on a stale
	# pointer web), so the base-first default would guard with the wrong
	# object; naming the live index keeps the access on the right
	# capability.
alias_mov_new:                  ;! long(ptr,ptr)
	endbr64
	xorq	%rdi, %rdi
	movq	(%rdi,%rsi), %rax #! new capability %rsi
	ret
	.size	alias_mov_new, .-alias_mov_new
	.globl	alias_mov_new_typo
	.type	alias_mov_new_typo, @function
alias_mov_new_typo:             ;! void(ptr,ptr,long)
	endbr64
	xorq	%rdi, %rdi
	movq	%rdx, (%rdi,%rsi) #! new capabiltiy %rsi
	ret
	.size	alias_mov_new_typo, .-alias_mov_new_typo
	.globl	alias_mov_use_typo
	.type	alias_mov_use_typo, @function
alias_mov_use_typo:             ;! long(ptr,ptr)
	endbr64
	xorq	%rdi, %rdi
	movq	(%rdi,%rsi), %rax #! use capabiltiy %rsi
	ret
	.size	alias_mov_use_typo, .-alias_mov_use_typo
	.globl	alias_vcopy_new
	.type	alias_vcopy_new, @function
	# Alias on a vector load: stale-zeroed base, live index.
alias_vcopy_new:                ;! void(ptr,ptr,ptr)
	endbr64
	xorq	%rdi, %rdi
	vmovdqu	(%rdi,%rsi), %ymm0 #! new capability %rsi
	vmovdqu	%ymm0, (%rdx)
	vzeroupper
	ret
	.size	alias_vcopy_new, .-alias_vcopy_new
	.globl	alias_vstore_new_typo
	.type	alias_vstore_new_typo, @function
	# Typo spelling on a vector store naming the base (stale-zeroed index).
alias_vstore_new_typo:          ;! void(ptr,ptr,ptr)
	endbr64
	vmovdqu	(%rdx), %ymm0
	xorq	%rdx, %rdx
	vmovdqu	%ymm0, (%rsi,%rdx) #! new capabiltiy %rsi
	vzeroupper
	ret
	.size	alias_vstore_new_typo, .-alias_vstore_new_typo
	.globl	alias_vcopy_use_typo
	.type	alias_vcopy_use_typo, @function
alias_vcopy_use_typo:           ;! void(ptr,ptr,ptr)
	endbr64
	xorq	%rdi, %rdi
	vmovdqu	(%rdi,%rsi), %ymm0 #! use capabiltiy %rsi
	vmovdqu	%ymm0, (%rdx)
	vzeroupper
	ret
	.size	alias_vcopy_use_typo, .-alias_vcopy_use_typo
	.globl	dual_usecap
	.type	dual_usecap, @function
	# Dual effect: an `add` with a memory operand both guards the access
	# with the named capability AND draws the ALU result from it. The base
	# is a stale zero, so the annotation is load-bearing for the guard;
	# the trailing load proves the result carries `%rsi`'s capability too
	# (buf[0] holds the address of buf[2], so the result points in bounds
	# of buf's object and the trailing load returns buf[2]).
dual_usecap:                    ;! long(ptr,ptr)
	endbr64
	xorq	%rdi, %rdi
	addq	(%rdi,%rsi), %rdi #! use capability %rsi
	movq	(%rdi), %rax
	ret
	.size	dual_usecap, .-dual_usecap
	.globl	dual_newcap
	.type	dual_newcap, @function
	# The same dual effect through the deprecated alias spelling.
dual_newcap:                    ;! long(ptr,ptr)
	endbr64
	xorq	%rdi, %rdi
	addq	(%rdi,%rsi), %rdi #! new capability %rsi
	movq	(%rdi), %rax
	ret
	.size	dual_newcap, .-dual_newcap
	.section	.note.GNU-stack,"",@progbits
