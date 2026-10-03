# `.byte` runs encoding whole instruction sequences with ModRM/SIB/displacement
# and immediate operands — every byte consumed by exactly one decoded
# instruction, verified behaviorally (a decoded instruction IS a spelled one
# to every downstream pass).
	.text
	.globl	byte_seq
	.type	byte_seq, @function
byte_seq:                       ;! long(long)
	pushq	%rbp
	movq	%rsp, %rbp
	# 48 8d 47 07            leaq 7(%rdi),%rax
	# 48 83 c0 03            addq $3,%rax
	# 48 89 45 f8            movq %rax,-8(%rbp)   (frame slot spill)
	# 48 8b 5d f8            movq -8(%rbp),%rbx
	# 89 d8                  movl %ebx,%eax       (truncate to 32 bits)
	.byte	0x48,0x8d,0x47,0x07
	.byte	0x48,0x83,0xc0,0x03
	.byte	0x48,0x89,0x45,0xf8
	.byte	0x48,0x8b,0x5d,0xf8
	.byte	0x89,0xd8
	popq	%rbp
	ret
	.size	byte_seq, .-byte_seq
	.globl	byte_sib
	.type	byte_sib, @function
byte_sib:                       ;! long(ptr, long)
	# rdi = pointer to a long array, rsi = index i
	# 48 8d 04 76           leaq (%rsi,%rsi,2),%rax    (SIB, no displacement)
	# 48 8b 44 c7 08        movq 8(%rdi,%rax,8),%rax   (SIB base+index+disp8:
	#                       loads arr[3i + 1])
	# 48 83 c0 01           addq $1,%rax
	.byte	0x48,0x8d,0x04,0x76
	.byte	0x48,0x8b,0x44,0xc7,0x08
	.byte	0x48,0x83,0xc0,0x01
	ret
	.size	byte_sib, .-byte_sib
	.section	.note.GNU-stack,"",@progbits
