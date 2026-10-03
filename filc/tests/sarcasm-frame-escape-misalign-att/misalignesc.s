# D9 fixed-frame escape promotion, pinned for MISALIGNED and SUB-WORD access
# into the promoted frame. The `leaq 17(%rsp), %rdi` handed to `call pokew
# ;! void(ptr)` escapes (and is itself misaligned), so the whole fixed frame
# is materialized as ONE GC allocation (filc_allocate in the sarcasm output —
# expected here). Fil-C object-granular bounds make within-object misalignment
# legal, so — exactly like compiled Fil-C code — only hardware-required
# alignment applies:
#   * a helper writes a misaligned qword at region+17 through the derived
#     pointer; the asm overwrites the same home with a DIRECT misaligned
#     `movq 17(%rsp)` and re-reads it through a re-derived pointer (one home);
#   * that qword is decomposed byte-by-byte at offsets 17..24 (little-endian
#     composition);
#   * movb/movw/movl/movq sub-word and odd-offset stores at region+40..54 go
#     through a derived pointer and are read back by width;
#   * the same family at region+65..72 goes through direct rsp spellings.
# The checksum is exact and mirrored in the C driver.
	.text
	.globl	misalignesc
	.type	misalignesc, @function
misalignesc:                    #! long(long)
	pushq	%rbx
	subq	$128, %rsp          # fixed frame [0,128): D0 = 136, region [0,128)

	# --- escape cluster: the MISALIGNED lea escapes to the helper, which
	# promotes the whole frame to one GC region ---
	leaq	17(%rsp), %rdi      # region+17 — the ESCAPING (misaligned) lea
	call	pokew ;! void(ptr)  # region+17..24 = 0x0102030405060708

	# --- direct misaligned qword over the same home, then pointer read ---
	movabsq	$0x0102030405060708, %rax
	movq	%rax, 17(%rsp)      # direct misaligned 8-byte store at region+17
	leaq	17(%rsp), %r9       # re-derive region+17
	movq	(%r9), %rbx         # acc = the qword through the pointer
	movq	17(%rsp), %rax      # the same home through the direct slot
	addq	%rax, %rbx          # 2 * 0x0102030405060708

	# --- little-endian byte decomposition of that qword ---
	movzbl	17(%rsp), %eax      # 0x08
	addq	%rax, %rbx
	movzbl	18(%rsp), %eax      # 0x07
	addq	%rax, %rbx
	movzbl	19(%rsp), %eax      # 0x06
	addq	%rax, %rbx
	movzbl	20(%rsp), %eax      # 0x05
	addq	%rax, %rbx
	movzbl	21(%rsp), %eax      # 0x04
	addq	%rax, %rbx
	movzbl	22(%rsp), %eax      # 0x03
	addq	%rax, %rbx
	movzbl	23(%rsp), %eax      # 0x02
	addq	%rax, %rbx
	movzbl	24(%rsp), %eax      # 0x01
	addq	%rax, %rbx

	# --- sub-word block through a derived pointer (odd offsets) ---
	leaq	40(%rsp), %r8       # region+40 derived pointer
	movb	$0xA1, (%r8)        # byte at region+40
	movw	$0xB2B3, 1(%r8)     # word at region+41 (odd)
	movl	$0xC4C5C6C7, 3(%r8) # long at region+43 (odd)
	movabsq	$0x1112131415161718, %rax
	movq	%rax, 7(%r8)        # misaligned qword at region+47
	movzbl	(%r8), %eax
	addq	%rax, %rbx          # 0xA1
	movzwl	1(%r8), %eax
	addq	%rax, %rbx          # 0xB2B3
	movl	3(%r8), %eax
	addq	%rax, %rbx          # 0xC4C5C6C7
	movq	7(%r8), %rax
	addq	%rax, %rbx          # 0x1112131415161718

	# --- the same family through direct rsp spellings ---
	movb	$0x77, 65(%rsp)
	movw	$0x1234, 66(%rsp)
	movl	$0x9ABCDEF0, 69(%rsp)
	movzbl	65(%rsp), %eax
	addq	%rax, %rbx          # 0x77
	movzwl	66(%rsp), %eax
	addq	%rax, %rbx          # 0x1234
	movl	69(%rsp), %eax
	addq	%rax, %rbx          # 0x9ABCDEF0
	movq	%rbx, %rax
	addq	$128, %rsp
	popq	%rbx
	ret
	.size	misalignesc, .-misalignesc
	.section	.note.GNU-stack,"",@progbits
