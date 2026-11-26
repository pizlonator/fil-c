 .text
.globl _start

// VBLENDV* (VBLENDVPS/VBLENDVPD, VEX.128 and VEX.256) take the mask as the
// 4th explicit operand (encoded in imm8[7:4]); only the legacy SSE
// BLENDVPS/BLENDVPD/PBLENDVB encodings read the mask from the implicit XMM0.
// Regression: the emulator used to read XMM0 for the VEX forms too -- found
// via supercop crypto_onetimeauth/poly1305 avx, whose masks live in
// ymm6/7/8/9/15 (an XMM0-resident mask happens to look correct, which is why
// a gcc -O3 intrinsic probe can mask the bug).
//
// Values are lane-distinct qwords so every blend outcome is visible in the
// whole-state trace.

_start:
  // a (src1 role) = {0x1111.., 0x2222.., 0x3333.., 0x4444..}
  movabsq $0x4444444444444444, %rax
  pushq %rax
  movabsq $0x3333333333333333, %rax
  pushq %rax
  movabsq $0x2222222222222222, %rax
  pushq %rax
  movabsq $0x1111111111111111, %rax
  pushq %rax
  vmovdqu (%rsp), %ymm1

  // b (src2 role) = {0xaaaa.., 0xbbbb.., 0xcccc.., 0xdddd..}
  movabsq $0xdddddddddddddddd, %rax
  pushq %rax
  movabsq $0xcccccccccccccccc, %rax
  pushq %rax
  movabsq $0xbbbbbbbbbbbbbbbb, %rax
  pushq %rax
  movabsq $0xaaaaaaaaaaaaaaaa, %rax
  pushq %rax
  vmovdqu (%rsp), %ymm2

  // mask: sign bits in lanes 0 and 2
  movabsq $0x8000000000000000, %rax
  movq  $0, %rcx
  // lane3 = 0
  pushq %rcx
  // lane2 = sign
  pushq %rax
  // lane1 = 0
  pushq %rcx
  // lane0 = sign
  pushq %rax
  vmovdqu (%rsp), %ymm6
  // lanes 0,2 from src2
  vblendvpd %ymm6, %ymm2, %ymm1, %ymm0

  // mask: sign bits in lanes 1 and 3
  leaq 32(%rsp), %rsp
  movq  $0, %rcx
  // lane3 = sign
  pushq %rax
  // lane2 = 0
  pushq %rcx
  // lane1 = sign
  pushq %rax
  // lane0 = 0
  pushq %rcx
  vmovdqu (%rsp), %ymm7
  // lanes 1,3 from src2
  vblendvpd %ymm7, %ymm2, %ymm1, %ymm0

  // mask: all sign bits set -> everything from src2
  leaq 32(%rsp), %rsp
  vpcmpeqd %ymm8, %ymm8, %ymm8
  vblendvpd %ymm8, %ymm2, %ymm1, %ymm0

  // mask: zero -> everything from src1
  vpxor %ymm9, %ymm9, %ymm9
  vblendvpd %ymm9, %ymm2, %ymm1, %ymm0

  // mask: sign bit in lane 2 only (the poly1305-avx failing shape)
  leaq 32(%rsp), %rsp
  movabsq $0x8000000000000000, %rax
  movq  $0, %rcx
  // lane3 = 0
  pushq %rcx
  // lane2 = sign
  pushq %rax
  pushq %rcx
  pushq %rcx
  vmovdqu (%rsp), %ymm15
  vblendvpd %ymm15, %ymm2, %ymm1, %ymm12

  // mask held in ymm0 for the VEX form (not special, but legal)
  leaq 32(%rsp), %rsp
  movq  $0, %rcx
  // lane3 = 0
  pushq %rcx
  // lane2 = sign
  pushq %rax
  pushq %rcx
  pushq %rcx
  vmovdqu (%rsp), %ymm0
  vblendvpd %ymm0, %ymm2, %ymm1, %ymm13

  // memory source: mask AND src2 both from the stack image at (%rsp)
  // (image = {0, 0, sign, 0}: lane2 of the result must come from memory)
  vmovdqu (%rsp), %ymm6
  vblendvpd %ymm6, (%rsp), %ymm1, %ymm14

  leaq 32(%rsp), %rsp

  // VBLENDVPS: 32-bit lanes, sign bit of each dword
  vmovdqu a32(%rip), %ymm1
  vmovdqu b32(%rip), %ymm2
  movabsq $0x8000000080000000, %rax
  movq  $0, %rcx
  // dwords 6,7 = 0
  pushq %rcx
  // dwords 4,5 sign
  pushq %rax
  pushq %rcx
  // dwords 0,1 sign
  pushq %rax
  vmovdqu (%rsp), %ymm6
  vblendvps %ymm6, %ymm2, %ymm1, %ymm0
  vmovdqu (%rsp), %ymm7
  vblendvps %ymm7, %ymm2, %ymm1, %ymm11
  vpcmpeqd %ymm8, %ymm8, %ymm8
  vblendvps %ymm8, %ymm2, %ymm1, %ymm10
  leaq 32(%rsp), %rsp

  // VEX.128: mask in a low xmm that is NOT xmm0
  vmovdqu a32(%rip), %xmm1
  vmovdqu b32(%rip), %xmm2
  movabsq $0x8000000080000000, %rax
  pushq %rax
  vmovdqu (%rsp), %xmm6
  vblendvps %xmm6, %xmm2, %xmm1, %xmm0
  vmovdqu (%rsp), %xmm7
  vblendvpd %xmm7, %xmm2, %xmm1, %xmm3
  leaq 8(%rsp), %rsp

  // legacy BLENDVPD/BLENDVPS: implicit XMM0 mask (dest == mask reg)
  movabsq $0x0000000000000000, %rax
  pushq %rax
  movabsq $0x8000000000000000, %rax
  pushq %rax
  // qword lane0 = sign, lane1 = 0
  vmovdqu (%rsp), %xmm0
  vmovdqu b32(%rip), %xmm1
  blendvpd %xmm1, %xmm0
  movabsq $0x0000000000000000, %rax
  pushq %rax
  movabsq $0x8000000080000000, %rax
  pushq %rax
  // dwords 0,1 sign
  vmovdqu (%rsp), %xmm0
  vmovdqu b32(%rip), %xmm1
  blendvps %xmm1, %xmm0
  leaq 32(%rsp), %rsp

  // legacy PBLENDVB (implicit XMM0 mask) and VEX VPBLENDVB (explicit mask)
  movabsq $0x0101010101010101, %rax
  pushq %rax
  movabsq $0x8080808080808080, %rax
  pushq %rax
  vmovdqu (%rsp), %xmm0
  vmovdqu b32(%rip), %xmm1
  pblendvb %xmm1, %xmm0
  // explicit mask for the VEX form
  vmovdqu (%rsp), %xmm4
  vpblendvb %xmm4, %xmm1, %xmm0, %xmm3
  leaq 16(%rsp), %rsp

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall

    .section .rodata
    .p2align 4
a32:  .quad 0x1111111111111111, 0x2222222222222222, 0x3333333333333333, 0x4444444444444444
b32:  .quad 0xaaaaaaaaaaaaaaaa, 0xbbbbbbbbbbbbbbbb, 0xcccccccccccccccc, 0xdddddddddddddddd
    .section .note.GNU-stack,"",@progbits
