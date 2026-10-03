# D9 fixed-frame escape promotion, pinned for REP STRING OPS into/out of the
# promoted frame. The `leaq 16(%rsp), %rdi` handed to `call seed8
# ;! void(ptr)` escapes, so the whole fixed frame is materialized as ONE GC
# allocation (filc_allocate in the sarcasm output — expected here) and every
# derived pointer carries that region's real capability:
#   * `rep movsb` copies 100 caller bytes INTO the region through a derived
#     destination (rsi = caller buffer, rdi = region+32);
#   * `rep stosq` fills region+160.. with a fixed 8-qword pattern;
#   * `rep movsb` copies the stored bytes back OUT through another derived
#     pointer (rsi = region+32, rdi = caller buffer) — the C driver checks
#     the round trip byte-exactly;
#   * the checksum reads the seed area, the fill area, and first/last copied
#     bytes through a derived pointer: exactly 161330.
	.text
	.globl	repesc
	.type	repesc, @function
repesc:                         #! long(ptr, ptr)
	pushq	%rbx
	pushq	%r12
	subq	$256, %rsp          # fixed frame [0,256): D0 = 272, region [0,256)
	movq	%rdi, %rbx          # rbx = src (call-safe)
	movq	%rsi, %r12          # r12 = out (call-safe)

	# --- escape cluster: the lea escapes to the helper, which promotes the
	# whole frame to one GC region ---
	leaq	16(%rsp), %rdi      # region+16 — the ESCAPING lea
	call	seed8 ;! void(ptr)  # slot16 = 0x1111, slot24 = 0x2222

	# --- rep movsb: caller buffer INTO the region through a derived dst ---
	leaq	32(%rsp), %rdi      # region+32 derived destination
	movq	%rbx, %rsi          # caller's source buffer
	movq	$100, %rcx
	rep movsb
	# --- rep stosq: fill region+160.. with a fixed pattern ---
	leaq	160(%rsp), %rdi     # region+160 derived destination
	movq	$0xC0DE, %rax
	movq	$8, %rcx
	rep stosq
	# --- rep movsb: the region bytes back OUT through a derived src ---
	leaq	32(%rsp), %rsi      # region+32 derived source
	movq	%r12, %rdi          # caller's output buffer
	movq	$100, %rcx
	rep movsb

	# --- checksum: seed area + fill area + copied edge bytes ---
	movq	16(%rsp), %rax      # 0x1111
	addq	24(%rsp), %rax      # +0x2222
	movq	160(%rsp), %rcx     # 0xC0DE
	addq	168(%rsp), %rcx     # +0xC0DE
	addq	216(%rsp), %rcx     # +0xC0DE (the last filled qword)
	addq	%rcx, %rax          # 0x1111 + 0x2222 + 3*0xC0DE
	leaq	32(%rsp), %rcx      # region+32 derived pointer
	movzbl	(%rcx), %edx        # copied byte 0
	movzbl	99(%rcx), %ecx      # copied byte 99
	addq	%rdx, %rax
	addq	%rcx, %rax          # + src[0] + src[99]
	addq	$256, %rsp
	popq	%r12
	popq	%rbx
	ret
	.size	repesc, .-repesc
	.section	.note.GNU-stack,"",@progbits
