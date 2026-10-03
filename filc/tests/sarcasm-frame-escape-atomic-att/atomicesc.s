# D9 fixed-frame escape promotion, pinned for LOCKED RMWs on the promoted
# frame. The `leaq 16(%rsp), %rbx` handed to `call seed2 ;! void(ptr)`
# escapes, so the whole fixed frame is materialized as ONE GC allocation
# (filc_allocate in the sarcasm output — expected here) and the derived
# pointer carries the region's real capability — which makes the region
# memory behave exactly like heap memory for the locked families:
#   * `lock xaddq` (fetch-add, returns the old value), then `lock addq` with
#     that old value;
#   * `lock orq` / `lock andq` / `lock xorq` on the second slot;
#   * `lock cmpxchgq` SUCCESS path (equal -> store + ZF=1) and FAILURE path
#     (not equal -> no store, rax = loaded, ZF=0).
# All accesses go through the derived pointer (never rsp-relative: a lock on
# a stack-frame slot is rejected — the region pointer is the legal spelling).
# Plain `xchg reg, mem` is intentionally absent: it is an implicitly locked
# RMW and a compile-time rejection (see sarcasm-reject-xchg-mem); the
# guaranteed-equal cmpxchg plays the exchange role here. Single-threaded, so
# every outcome is deterministic; the checksum is exactly 415.
	.text
	.globl	atomicesc
	.type	atomicesc, @function
atomicesc:                      #! long(long)
	pushq	%rbx
	subq	$128, %rsp          # fixed frame [0,128): D0 = 136, region [0,128)

	# --- escape cluster: the lea escapes to the helper, which promotes the
	# whole frame to one GC region ---
	leaq	16(%rsp), %rbx      # region+16 — the ESCAPING lea (call-safe park)
	movq	%rbx, %rdi
	call	seed2 ;! void(ptr)  # slot16 = 100, slot24 = 200

	# --- fetch-add, then add the returned old value back ---
	movq	$5, %rcx
	lock xaddq	%rcx, (%rbx)    # rcx = 100 (old); slot16 = 105
	movq	%rcx, %r10          # r10 = the returned old value
	lock addq	%rcx, (%rbx)    # slot16 = 105 + 100 = 205

	# --- or/and/xor on the second slot (200 = 0xC8) ---
	lock orq	$0x0F, 8(%rbx)  # slot24 = 200 | 15 = 207
	lock andq	$0xFF, 8(%rbx)  # slot24 = 207 & 255 = 207
	movq	$200, %rdx
	lock xorq	%rdx, 8(%rbx)   # slot24 = 207 ^ 200 = 7

	# --- cmpxchg SUCCESS: slot16 (205) == rax -> store rcx, ZF = 1 ---
	# (sete + 32-bit movzbl: the byte->64-bit movzbl spelling hits an
	# unrelated sarcasm rendering bug — movzbq renders as `movzbl %al, %rax`,
	# which GAS rejects.)
	movq	$205, %rax
	movq	$0x64, %rcx
	lock cmpxchgq	%rcx, (%rbx)    # slot16 = 100
	sete	%r8b
	movzbl	%r8b, %r8d          # r8 = 1

	# --- cmpxchg FAILURE: slot16 (100) != rax -> no store, rax = 100, ZF = 0 ---
	movq	$205, %rax
	movq	$0x11, %rcx
	lock cmpxchgq	%rcx, (%rbx)
	sete	%r9b
	movzbl	%r9b, %r9d          # r9 = 0; rax = the loaded 100

	# --- checksum ---
	movq	16(%rsp), %rcx      # slot16 direct: 100
	addq	24(%rsp), %rcx      # slot24 direct: +7
	addq	%rcx, %rax          # 100 + 107 = 207
	movq	(%rbx), %rcx        # slot16 through the pointer: 100
	addq	8(%rbx), %rcx       # slot24 through the pointer: +7
	addq	%rcx, %rax          # 207 + 107 = 314
	addq	%r10, %rax          # + the xaddq old value (100) = 414
	addq	%r8, %rax           # + success ZF (1) = 415
	addq	%r9, %rax           # + failure ZF (0) = 415
	addq	$128, %rsp
	popq	%rbx
	ret
	.size	atomicesc, .-atomicesc
	.section	.note.GNU-stack,"",@progbits
