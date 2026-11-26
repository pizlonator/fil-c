 .text
.globl _start

_start:
  // ADCal  
  // No polarity
  pcmpestri  $0, -128(%rsp), %xmm1
  pcmpestri  $1, -128(%rsp), %xmm1
  pcmpestri  $2, -128(%rsp), %xmm1
  pcmpestri  $3, -128(%rsp), %xmm1
  pcmpestri  $4, -128(%rsp), %xmm1
  pcmpestri  $5, -128(%rsp), %xmm1
  pcmpestri  $6, -128(%rsp), %xmm1
  pcmpestri  $7, -128(%rsp), %xmm1
  pcmpestri  $8, -128(%rsp), %xmm1
  pcmpestri  $9, -128(%rsp), %xmm1
  pcmpestri  $10, -128(%rsp), %xmm1
  pcmpestri  $11, -128(%rsp), %xmm1
  pcmpestri  $12, -128(%rsp), %xmm1
  pcmpestri  $13, -128(%rsp), %xmm1
  pcmpestri  $14, -128(%rsp), %xmm1
  pcmpestri  $15, -128(%rsp), %xmm1
  
  //polarity
  pcmpestri  $0x10, -128(%rsp), %xmm1
  pcmpestri  $0x11, -128(%rsp), %xmm1
  pcmpestri  $0x12, -128(%rsp), %xmm1
  pcmpestri  $0x13, -128(%rsp), %xmm1
  pcmpestri  $0x14, -128(%rsp), %xmm1
  pcmpestri  $0x15, -128(%rsp), %xmm1
  pcmpestri  $0x16, -128(%rsp), %xmm1
  pcmpestri  $0x17, -128(%rsp), %xmm1
  pcmpestri  $0x18, -128(%rsp), %xmm1
  pcmpestri  $0x19, -128(%rsp), %xmm1
  pcmpestri  $0x1a, -128(%rsp), %xmm1
  pcmpestri  $0x1b, -128(%rsp), %xmm1
  pcmpestri  $0x1c, -128(%rsp), %xmm1
  pcmpestri  $0x1d, -128(%rsp), %xmm1
  pcmpestri  $0x1e, -128(%rsp), %xmm1
  pcmpestri  $0x1f, -128(%rsp), %xmm1

  pcmpestri  $0x20, -128(%rsp), %xmm1
  pcmpestri  $0x21, -128(%rsp), %xmm1
  pcmpestri  $0x22, -128(%rsp), %xmm1
  pcmpestri  $0x23, -128(%rsp), %xmm1
  pcmpestri  $0x24, -128(%rsp), %xmm1
  pcmpestri  $0x25, -128(%rsp), %xmm1
  pcmpestri  $0x26, -128(%rsp), %xmm1
  pcmpestri  $0x27, -128(%rsp), %xmm1
  pcmpestri  $0x28, -128(%rsp), %xmm1
  pcmpestri  $0x29, -128(%rsp), %xmm1
  pcmpestri  $0x2a, -128(%rsp), %xmm1
  pcmpestri  $0x2b, -128(%rsp), %xmm1
  pcmpestri  $0x2c, -128(%rsp), %xmm1
  pcmpestri  $0x2d, -128(%rsp), %xmm1
  pcmpestri  $0x2e, -128(%rsp), %xmm1
  pcmpestri  $0x2f, -128(%rsp), %xmm1

  pcmpestri  $0x30, -128(%rsp), %xmm1
  pcmpestri  $0x31, -128(%rsp), %xmm1
  pcmpestri  $0x32, -128(%rsp), %xmm1
  pcmpestri  $0x33, -128(%rsp), %xmm1
  pcmpestri  $0x34, -128(%rsp), %xmm1
  pcmpestri  $0x35, -128(%rsp), %xmm1
  pcmpestri  $0x36, -128(%rsp), %xmm1
  pcmpestri  $0x37, -128(%rsp), %xmm1
  pcmpestri  $0x38, -128(%rsp), %xmm1
  pcmpestri  $0x39, -128(%rsp), %xmm1
  pcmpestri  $0x3a, -128(%rsp), %xmm1
  pcmpestri  $0x3b, -128(%rsp), %xmm1
  pcmpestri  $0x3c, -128(%rsp), %xmm1
  pcmpestri  $0x3d, -128(%rsp), %xmm1
  pcmpestri  $0x3e, -128(%rsp), %xmm1
  pcmpestri  $0x3f, -128(%rsp), %xmm1

  // Output Selection
  pcmpestri  $0x40, -128(%rsp), %xmm1
  pcmpestri  $0x41, -128(%rsp), %xmm1
  pcmpestri  $0x42, -128(%rsp), %xmm1
  pcmpestri  $0x43, -128(%rsp), %xmm1
  pcmpestri  $0x44, -128(%rsp), %xmm1
  pcmpestri  $0x45, -128(%rsp), %xmm1
  pcmpestri  $0x46, -128(%rsp), %xmm1
  pcmpestri  $0x47, -128(%rsp), %xmm1
  pcmpestri  $0x48, -128(%rsp), %xmm1
  pcmpestri  $0x49, -128(%rsp), %xmm1
  pcmpestri  $0x4a, -128(%rsp), %xmm1
  pcmpestri  $0x4b, -128(%rsp), %xmm1
  pcmpestri  $0x4c, -128(%rsp), %xmm1
  pcmpestri  $0x4d, -128(%rsp), %xmm1
  pcmpestri  $0x4e, -128(%rsp), %xmm1
  pcmpestri  $0x4f, -128(%rsp), %xmm1

  pcmpestri  $0x50, -128(%rsp), %xmm1
  pcmpestri  $0x51, -128(%rsp), %xmm1
  pcmpestri  $0x52, -128(%rsp), %xmm1
  pcmpestri  $0x53, -128(%rsp), %xmm1
  pcmpestri  $0x54, -128(%rsp), %xmm1
  pcmpestri  $0x55, -128(%rsp), %xmm1
  pcmpestri  $0x56, -128(%rsp), %xmm1
  pcmpestri  $0x57, -128(%rsp), %xmm1
  pcmpestri  $0x58, -128(%rsp), %xmm1
  pcmpestri  $0x59, -128(%rsp), %xmm1
  pcmpestri  $0x5a, -128(%rsp), %xmm1
  pcmpestri  $0x5b, -128(%rsp), %xmm1
  pcmpestri  $0x5c, -128(%rsp), %xmm1
  pcmpestri  $0x5d, -128(%rsp), %xmm1
  pcmpestri  $0x5e, -128(%rsp), %xmm1
  pcmpestri  $0x5f, -128(%rsp), %xmm1

  pcmpestri  $0x60, -128(%rsp), %xmm1
  pcmpestri  $0x61, -128(%rsp), %xmm1
  pcmpestri  $0x62, -128(%rsp), %xmm1
  pcmpestri  $0x63, -128(%rsp), %xmm1
  pcmpestri  $0x64, -128(%rsp), %xmm1
  pcmpestri  $0x65, -128(%rsp), %xmm1
  pcmpestri  $0x66, -128(%rsp), %xmm1
  pcmpestri  $0x67, -128(%rsp), %xmm1
  pcmpestri  $0x68, -128(%rsp), %xmm1
  pcmpestri  $0x69, -128(%rsp), %xmm1
  pcmpestri  $0x6a, -128(%rsp), %xmm1
  pcmpestri  $0x6b, -128(%rsp), %xmm1
  pcmpestri  $0x6c, -128(%rsp), %xmm1
  pcmpestri  $0x6d, -128(%rsp), %xmm1
  pcmpestri  $0x6e, -128(%rsp), %xmm1
  pcmpestri  $0x6f, -128(%rsp), %xmm1

  pcmpestri  $0x70, -128(%rsp), %xmm1
  pcmpestri  $0x71, -128(%rsp), %xmm1
  pcmpestri  $0x72, -128(%rsp), %xmm1
  pcmpestri  $0x73, -128(%rsp), %xmm1
  pcmpestri  $0x74, -128(%rsp), %xmm1
  pcmpestri  $0x75, -128(%rsp), %xmm1
  pcmpestri  $0x76, -128(%rsp), %xmm1
  pcmpestri  $0x77, -128(%rsp), %xmm1
  pcmpestri  $0x78, -128(%rsp), %xmm1
  pcmpestri  $0x79, -128(%rsp), %xmm1
  pcmpestri  $0x7a, -128(%rsp), %xmm1
  pcmpestri  $0x7b, -128(%rsp), %xmm1
  pcmpestri  $0x7c, -128(%rsp), %xmm1
  pcmpestri  $0x7d, -128(%rsp), %xmm1
  pcmpestri  $0x7e, -128(%rsp), %xmm1
  pcmpestri  $0x7f, -128(%rsp), %xmm1
  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
