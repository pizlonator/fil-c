	.text
	# `sub` of two pointers is an integer difference, and a shift result is
	# always an integer: neither keeps a capability. This is the AES_decrypt
	# Td-select shape — `mov key,scratch / sub table,scratch /
	# and $0x300,scratch / lea (table,scratch),table / shr $3,scratch /
	# add scratch,table` — where the scratch must be a bare integer by the
	# final add, so the add keeps the TABLE capability. Before the ptrflow
	# kill fix, the scratch kept the KEY capability through the sub/shr (an
	# RMW web shares the input and output temps, so merely skipping
	# propagation left the key's lower on the integer), the add then
	# re-based the table web onto the key, and the first Td-table read
	# panicked with AES_KEY-sized bounds.
	.globl	dec_select
	.type	dec_select, @function
dec_select:                     ;! long(ptr,ptr)
	endbr64
	movq	%rsi, %rcx
	subq	%rdi, %rcx
	andq	$0x300, %rcx
	leaq	(%rdi,%rcx), %rdi
	shrq	$3, %rcx
	addq	%rcx, %rdi
	movq	(%rdi), %rax
	ret
	.size	dec_select, .-dec_select
	# Shift applied directly to a pointer web: the shifted value is an
	# integer, so the add below keeps the table's capability.
	.globl	shr_ptr_kill
	.type	shr_ptr_kill, @function
shr_ptr_kill:                   ;! long(ptr,ptr)
	endbr64
	movq	%rsi, %rax
	andq	$0x3f0, %rax
	shrq	$4, %rax
	addq	%rax, %rdi
	movq	(%rdi), %rax
	ret
	.size	shr_ptr_kill, .-shr_ptr_kill
	# ptr-ptr sub alone: the difference is an integer zero here, so the
	# add keeps the base's capability with no annotation.
	.globl	sub_ptr_zero_add
	.type	sub_ptr_zero_add, @function
sub_ptr_zero_add:               ;! long(ptr,ptr)
	endbr64
	movq	%rsi, %rax
	subq	%rsi, %rax
	addq	%rax, %rdi
	movq	(%rdi), %rax
	ret
	.size	sub_ptr_zero_add, .-sub_ptr_zero_add
	.section	.note.GNU-stack,"",@progbits
