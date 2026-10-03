# The OpenSSL aesni CBC-encrypt tail shape (${PREFIX}_cbc_encrypt's
# .Lcbc_enc_loop/.Lcbc_enc_tail in crypto/aes/asm/aesni-x86_64.pl): the
# length-driven main loop is entered from the top with distinct input and
# output buffers; on a short remainder it falls into the tail, which swaps
# the two pointer registers with xchg, copies the tail bytes with rep movsb,
# zero-pads with rep stosb (both spelled out: sarcasm rejects data directives
# like the legacy-gas `.long` spellings in function bodies and lowers the rep
# string ops to checked copies/fills), rewinds the output by one block,
# aliases input to output (mov %rdi,%rsi), zeroes the length, and re-enters
# the SAME loop label for one more spin. There is no epoch flag: exit
# discrimination is pure length arithmetic (sub/jnc/add/jnz), exactly like
# the original. The two entries give the loop's input-block read different
# buffer objects (input on the main entry, output on the tail entry), so the
# xchg-merged register webs need sarcasm's dynamic (lockstep) lowers; a
# static lower would check one entry's reads against the other's object.
	.text
	.globl	swap_loop2
	.type	swap_loop2, @function
swap_loop2:                     ;! void(ptr,ptr,long)
	endbr64
	testq	%rdx, %rdx
	jz	.Ldone
	cmpq	$4, %rdx
	jb	.Ltail
	subq	$4, %rdx
	jmp	.Lloop
	.align	16
.Lloop:
	movb	(%rdi), %al
	xorb	$0xff, %al
	movb	%al, (%rsi)
	movb	1(%rdi), %al
	xorb	$0xff, %al
	movb	%al, 1(%rsi)
	movb	2(%rdi), %al
	xorb	$0xff, %al
	movb	%al, 2(%rsi)
	movb	3(%rdi), %al
	xorb	$0xff, %al
	movb	%al, 3(%rsi)
	leaq	4(%rdi), %rdi
	leaq	4(%rsi), %rsi
	subq	$4, %rdx
	jnc	.Lloop
	addq	$4, %rdx
	jnz	.Ltail
	jmp	.Ldone
.Ltail:
	movq	%rdx, %rcx	# zaps $key
	xchgq	%rdi, %rsi	# %rdi is out and %rsi is inp now
	rep	movsb	# copy tail bytes
	movl	$4, %ecx	# zero tail
	subq	%rdx, %rcx
	xorl	%eax, %eax
	rep	stosb	# zero pad
	leaq	-4(%rdi), %rdi	# rewind $out by 1 block
	movq	%rdi, %rsi	# $inp and $out are the same
	xorq	%rdx, %rdx	# len=0
	jmp	.Lloop	# one more spin
.Ldone:
	ret
	.size	swap_loop2, .-swap_loop2
	.section	.note.GNU-stack,"",@progbits
