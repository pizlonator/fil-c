 .text
.globl _start

_start:
  // ADCal  
  // No polarity
  vpcmpistri  $0, %xmm0, %xmm1
  vpcmpistri  $1, %xmm0, %xmm1
  vpcmpistri  $2, %xmm0, %xmm1
  vpcmpistri  $3, %xmm0, %xmm1
  vpcmpistri  $4, %xmm0, %xmm1
  vpcmpistri  $5, %xmm0, %xmm1
  vpcmpistri  $6, %xmm0, %xmm1
  vpcmpistri  $7, %xmm0, %xmm1
  vpcmpistri  $8, %xmm0, %xmm1
  vpcmpistri  $9, %xmm0, %xmm1
  vpcmpistri  $10, %xmm0, %xmm1
  vpcmpistri  $11, %xmm0, %xmm1
  vpcmpistri  $12, %xmm0, %xmm1
  vpcmpistri  $13, %xmm0, %xmm1
  vpcmpistri  $14, %xmm0, %xmm1
  vpcmpistri  $15, %xmm0, %xmm1
  
  //polarity
  vpcmpistri  $0x10, %xmm0, %xmm1
  vpcmpistri  $0x11, %xmm0, %xmm1
  vpcmpistri  $0x12, %xmm0, %xmm1
  vpcmpistri  $0x13, %xmm0, %xmm1
  vpcmpistri  $0x14, %xmm0, %xmm1
  vpcmpistri  $0x15, %xmm0, %xmm1
  vpcmpistri  $0x16, %xmm0, %xmm1
  vpcmpistri  $0x17, %xmm0, %xmm1
  vpcmpistri  $0x18, %xmm0, %xmm1
  vpcmpistri  $0x19, %xmm0, %xmm1
  vpcmpistri  $0x1a, %xmm0, %xmm1
  vpcmpistri  $0x1b, %xmm0, %xmm1
  vpcmpistri  $0x1c, %xmm0, %xmm1
  vpcmpistri  $0x1d, %xmm0, %xmm1
  vpcmpistri  $0x1e, %xmm0, %xmm1
  vpcmpistri  $0x1f, %xmm0, %xmm1

  vpcmpistri  $0x20, %xmm0, %xmm1
  vpcmpistri  $0x21, %xmm0, %xmm1
  vpcmpistri  $0x22, %xmm0, %xmm1
  vpcmpistri  $0x23, %xmm0, %xmm1
  vpcmpistri  $0x24, %xmm0, %xmm1
  vpcmpistri  $0x25, %xmm0, %xmm1
  vpcmpistri  $0x26, %xmm0, %xmm1
  vpcmpistri  $0x27, %xmm0, %xmm1
  vpcmpistri  $0x28, %xmm0, %xmm1
  vpcmpistri  $0x29, %xmm0, %xmm1
  vpcmpistri  $0x2a, %xmm0, %xmm1
  vpcmpistri  $0x2b, %xmm0, %xmm1
  vpcmpistri  $0x2c, %xmm0, %xmm1
  vpcmpistri  $0x2d, %xmm0, %xmm1
  vpcmpistri  $0x2e, %xmm0, %xmm1
  vpcmpistri  $0x2f, %xmm0, %xmm1

  vpcmpistri  $0x30, %xmm0, %xmm1
  vpcmpistri  $0x31, %xmm0, %xmm1
  vpcmpistri  $0x32, %xmm0, %xmm1
  vpcmpistri  $0x33, %xmm0, %xmm1
  vpcmpistri  $0x34, %xmm0, %xmm1
  vpcmpistri  $0x35, %xmm0, %xmm1
  vpcmpistri  $0x36, %xmm0, %xmm1
  vpcmpistri  $0x37, %xmm0, %xmm1
  vpcmpistri  $0x38, %xmm0, %xmm1
  vpcmpistri  $0x39, %xmm0, %xmm1
  vpcmpistri  $0x3a, %xmm0, %xmm1
  vpcmpistri  $0x3b, %xmm0, %xmm1
  vpcmpistri  $0x3c, %xmm0, %xmm1
  vpcmpistri  $0x3d, %xmm0, %xmm1
  vpcmpistri  $0x3e, %xmm0, %xmm1
  vpcmpistri  $0x3f, %xmm0, %xmm1

  // Output Selection
  vpcmpistri  $0x40, %xmm0, %xmm1
  vpcmpistri  $0x41, %xmm0, %xmm1
  vpcmpistri  $0x42, %xmm0, %xmm1
  vpcmpistri  $0x43, %xmm0, %xmm1
  vpcmpistri  $0x44, %xmm0, %xmm1
  vpcmpistri  $0x45, %xmm0, %xmm1
  vpcmpistri  $0x46, %xmm0, %xmm1
  vpcmpistri  $0x47, %xmm0, %xmm1
  vpcmpistri  $0x48, %xmm0, %xmm1
  vpcmpistri  $0x49, %xmm0, %xmm1
  vpcmpistri  $0x4a, %xmm0, %xmm1
  vpcmpistri  $0x4b, %xmm0, %xmm1
  vpcmpistri  $0x4c, %xmm0, %xmm1
  vpcmpistri  $0x4d, %xmm0, %xmm1
  vpcmpistri  $0x4e, %xmm0, %xmm1
  vpcmpistri  $0x4f, %xmm0, %xmm1

  vpcmpistri  $0x50, %xmm0, %xmm1
  vpcmpistri  $0x51, %xmm0, %xmm1
  vpcmpistri  $0x52, %xmm0, %xmm1
  vpcmpistri  $0x53, %xmm0, %xmm1
  vpcmpistri  $0x54, %xmm0, %xmm1
  vpcmpistri  $0x55, %xmm0, %xmm1
  vpcmpistri  $0x56, %xmm0, %xmm1
  vpcmpistri  $0x57, %xmm0, %xmm1
  vpcmpistri  $0x58, %xmm0, %xmm1
  vpcmpistri  $0x59, %xmm0, %xmm1
  vpcmpistri  $0x5a, %xmm0, %xmm1
  vpcmpistri  $0x5b, %xmm0, %xmm1
  vpcmpistri  $0x5c, %xmm0, %xmm1
  vpcmpistri  $0x5d, %xmm0, %xmm1
  vpcmpistri  $0x5e, %xmm0, %xmm1
  vpcmpistri  $0x5f, %xmm0, %xmm1

  vpcmpistri  $0x60, %xmm0, %xmm1
  vpcmpistri  $0x61, %xmm0, %xmm1
  vpcmpistri  $0x62, %xmm0, %xmm1
  vpcmpistri  $0x63, %xmm0, %xmm1
  vpcmpistri  $0x64, %xmm0, %xmm1
  vpcmpistri  $0x65, %xmm0, %xmm1
  vpcmpistri  $0x66, %xmm0, %xmm1
  vpcmpistri  $0x67, %xmm0, %xmm1
  vpcmpistri  $0x68, %xmm0, %xmm1
  vpcmpistri  $0x69, %xmm0, %xmm1
  vpcmpistri  $0x6a, %xmm0, %xmm1
  vpcmpistri  $0x6b, %xmm0, %xmm1
  vpcmpistri  $0x6c, %xmm0, %xmm1
  vpcmpistri  $0x6d, %xmm0, %xmm1
  vpcmpistri  $0x6e, %xmm0, %xmm1
  vpcmpistri  $0x6f, %xmm0, %xmm1

  vpcmpistri  $0x70, %xmm0, %xmm1
  vpcmpistri  $0x71, %xmm0, %xmm1
  vpcmpistri  $0x72, %xmm0, %xmm1
  vpcmpistri  $0x73, %xmm0, %xmm1
  vpcmpistri  $0x74, %xmm0, %xmm1
  vpcmpistri  $0x75, %xmm0, %xmm1
  vpcmpistri  $0x76, %xmm0, %xmm1
  vpcmpistri  $0x77, %xmm0, %xmm1
  vpcmpistri  $0x78, %xmm0, %xmm1
  vpcmpistri  $0x79, %xmm0, %xmm1
  vpcmpistri  $0x7a, %xmm0, %xmm1
  vpcmpistri  $0x7b, %xmm0, %xmm1
  vpcmpistri  $0x7c, %xmm0, %xmm1
  vpcmpistri  $0x7d, %xmm0, %xmm1
  vpcmpistri  $0x7e, %xmm0, %xmm1
  vpcmpistri  $0x7f, %xmm0, %xmm1
  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
