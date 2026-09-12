	.text
	# `add` keeps capabilities: a pointer combined with integers keeps the
	# pointer's capability (bounds checks still guard any misuse). The
	# first shape is the AES `shr`/`add` idiom — `%rax` is the result of a
	# `shr` (a scalar), so `add %rax,$sbox` keeps `$sbox`'s capability via
	# pointer flow. The `use capability` shapes cover the ambiguous case
	# where both inputs carry capabilities: the annotation selects the
	# winner explicitly.
	.globl	add_shr_base
	.type	add_shr_base, @function
add_shr_base:                   ;! long(ptr,long)
	endbr64
	movq	%rsi, %rax
	shrq	$3, %rax
	addq	%rax, %rdi
	movq	(%rdi), %rax
	ret
	.size	add_shr_base, .-add_shr_base
	.globl	add_usecap
	.type	add_usecap, @function
	# Both inputs carry capabilities here (`%rax` is a stale second-pointer
	# web: `sub` of two pointers is an integer, but the web stays sticky —
	# so the `add` sees two origins). The annotation keeps `%rdi`'s.
add_usecap:                     ;! long(ptr,ptr)
	endbr64
	movq	%rsi, %rax
	subq	%rsi, %rax
	addq	%rax, %rdi #! use capability %rdi
	movq	(%rdi), %rax
	ret
	.size	add_usecap, .-add_usecap
	.globl	sub_usecap
	.type	sub_usecap, @function
	# Same disambiguation on `sub` (single live source — the annotation is
	# unnecessary but must still validate and preserve the capability).
sub_usecap:                     ;! long(ptr,long)
	endbr64
	subq	%rsi, %rdi #! use capability %rdi
	movq	(%rdi), %rax
	ret
	.size	sub_usecap, .-sub_usecap
	.globl	lea_usecap_idx
	.type	lea_usecap_idx, @function
	# `lea` with a stale-zeroed base: without the annotation the lea's
	# base preference would pick `%rdi`'s stale capability; naming the
	# live index keeps `%rsi`'s instead.
lea_usecap_idx:                 ;! ptr(ptr,ptr)
	endbr64
	xorq	%rdi, %rdi
	leaq	(%rdi,%rsi), %rax #! use capability %rsi
	ret
	.size	lea_usecap_idx, .-lea_usecap_idx
	.globl	shl_count_nocap
	.type	shl_count_nocap, @function
	# Shifts never propagate capabilities — not even from a count
	# register (`shlq %cl,%rax` must not make `%rax` carry `%rcx`'s
	# capability). `%rax` stays scalar, so the `add` below keeps `%rdi`.
	# (`and` preserves, so `%rcx` keeps `%rsi`'s capability while its
	# value is masked to a small in-bounds count.)
shl_count_nocap:                ;! long(ptr,ptr)
	endbr64
	movq	%rsi, %rcx
	andq	$3, %rcx
	movq	$8, %rax
	shlq	%cl, %rax
	addq	%rax, %rdi
	movq	(%rdi), %rax
	ret
	.size	shl_count_nocap, .-shl_count_nocap
	.section	.note.GNU-stack,"",@progbits
