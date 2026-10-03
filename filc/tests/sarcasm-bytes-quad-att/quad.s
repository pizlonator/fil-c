# A `.quad` directive holds EIGHT bytes — enough for three whole instructions
# in one statement. The run folds the quad's little-endian bytes into one
# stream and decodes every instruction in it (48 89 d8 = movq %rbx,%rax;
# c3 = ret; 90 = nop), and a `.quad`/`.byte` mix is one run.
	.text
	.globl	quad_two
	.type	quad_two, @function
quad_two:                       ;! long(long)
	movq	%rdi, %rbx
	# 48 89 d8 c3 90 90 90 90 = movq %rbx,%rax; ret; nop x4
	.quad	0x90909090c3d88948
	# (the ret above returns; the nops are the quad's padding)
	.size	quad_two, .-quad_two
	.globl	quad_mixed
	.type	quad_mixed, @function
quad_mixed:                     ;! long(long)
	# 48 8d 47 03            leaq 3(%rdi),%rax   \ one .quad: three
	# 83 c0 04               addl $4,%eax        / instructions
	# c3                     ret                 /
	.quad	0xc304c08303478d48
	.byte	0x48,0x05,0x04,0x00,0x00,0x00	# addq $4,%rax (dead but decoded)
	.size	quad_mixed, .-quad_mixed
	.globl	quad_seq
	.type	quad_seq, @function
quad_seq:                       ;! long(long)
	# 48 01 f8 = addq %rdi,%rax ; 48 83 c0 02 = addq $2,%rax ; c3 = ret
	movq	$0, %rax
	.quad	0xc302c08348f80148
	.size	quad_seq, .-quad_seq
	.section	.note.GNU-stack,"",@progbits
