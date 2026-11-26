 .text
.globl _start

_start:
  // PDEP  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $0, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $1, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $2, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $3, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $4, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $5, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $6, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $7, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $8, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $9, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $10, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $11, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $12, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $13, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $14, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $15, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $16, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $17, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $18, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $19, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $20, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $21, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $22, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $23, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $24, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $25, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $26, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $27, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $28, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $29, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $30, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $31, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $32, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $33, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $34, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $35, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $36, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $37, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $38, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $39, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $40, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $41, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $42, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $43, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $44, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $45, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $46, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $47, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $48, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $49, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $50, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $51, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $52, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $53, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $54, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $55, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $56, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $57, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $58, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $59, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $60, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $61, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $62, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $63, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $64, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $65, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $66, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $67, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $68, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $69, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $70, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $71, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $72, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $73, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $74, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $75, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $76, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $77, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $78, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $79, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $80, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $81, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $82, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $83, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $84, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $85, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $86, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $87, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $88, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $89, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $90, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $91, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $92, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $93, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $94, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $95, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $96, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $97, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $98, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $99, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $100, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $101, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $102, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $103, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $104, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $105, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $106, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $107, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $108, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $109, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $110, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $111, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $112, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $113, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $114, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $115, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $116, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $117, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $118, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $119, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $120, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $121, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $122, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $123, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $124, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $125, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $126, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $127, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $128, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $129, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $130, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $131, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $132, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $133, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $134, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $135, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $136, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $137, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $138, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $139, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $140, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $141, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $142, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $143, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $144, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $145, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $146, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $147, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $148, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $149, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $150, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $151, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $152, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $153, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $154, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $155, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $156, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $157, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $158, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $159, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $160, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $161, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $162, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $163, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $164, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $165, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $166, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $167, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $168, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $169, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $170, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $171, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $172, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $173, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $174, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $175, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $176, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $177, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $178, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $179, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $180, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $181, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $182, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $183, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $184, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $185, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $186, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $187, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $188, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $189, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $190, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $191, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $192, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $193, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $194, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $195, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $196, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $197, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $198, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $199, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $200, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $201, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $202, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $203, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $204, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $205, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $206, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $207, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $208, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $209, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $210, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $211, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $212, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $213, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $214, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $215, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $216, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $217, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $218, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $219, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $220, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $221, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $222, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $223, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $224, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $225, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $226, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $227, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $228, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $229, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $230, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $231, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $232, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $233, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $234, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $235, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $236, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $237, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $238, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $239, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $240, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $241, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $242, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $243, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $244, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $245, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $246, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $247, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $248, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $249, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $250, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $251, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $252, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $253, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $254, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $255, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $255, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $255, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $255, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $255, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $0, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $1, %xmm1, %xmm2, %xmm3
  
  movq  $0xFFFFFFFFFFFFFFFF,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $255, %xmm1, %xmm2, %xmm3
  
  movq  $0xFFFFFFFFFFFFFFFF,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $0, %xmm1, %xmm2, %xmm3
  
  movq  $0x0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vpclmulqdq $0, %xmm1, %xmm2, %xmm3

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
