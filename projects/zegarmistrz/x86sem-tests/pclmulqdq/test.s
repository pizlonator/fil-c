 .text
.globl _start

_start:
  // PDEP  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $0, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $1, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $2, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $3, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $4, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $5, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $6, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $7, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $8, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $9, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $10, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $11, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $12, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $13, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $14, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $15, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $16, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $17, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $18, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $19, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $20, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $21, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $22, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $23, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $24, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $25, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $26, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $27, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $28, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $29, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $30, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $31, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $32, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $33, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $34, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $35, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $36, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $37, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $38, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $39, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $40, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $41, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $42, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $43, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $44, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $45, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $46, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $47, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $48, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $49, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $50, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $51, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $52, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $53, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $54, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $55, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $56, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $57, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $58, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $59, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $60, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $61, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $62, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $63, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $64, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $65, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $66, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $67, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $68, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $69, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $70, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $71, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $72, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $73, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $74, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $75, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $76, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $77, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $78, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $79, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $80, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $81, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $82, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $83, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $84, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $85, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $86, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $87, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $88, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $89, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $90, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $91, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $92, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $93, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $94, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $95, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $96, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $97, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $98, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $99, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $100, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $101, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $102, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $103, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $104, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $105, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $106, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $107, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $108, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $109, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $110, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $111, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $112, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $113, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $114, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $115, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $116, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $117, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $118, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $119, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $120, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $121, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $122, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $123, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $124, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $125, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $126, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $127, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $128, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $129, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $130, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $131, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $132, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $133, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $134, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $135, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $136, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $137, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $138, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $139, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $140, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $141, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $142, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $143, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $144, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $145, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $146, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $147, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $148, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $149, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $150, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $151, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $152, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $153, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $154, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $155, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $156, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $157, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $158, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $159, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $160, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $161, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $162, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $163, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $164, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $165, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $166, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $167, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $168, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $169, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $170, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $171, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $172, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $173, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $174, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $175, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $176, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $177, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $178, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $179, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $180, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $181, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $182, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $183, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $184, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $185, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $186, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $187, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $188, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $189, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $190, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $191, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $192, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $193, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $194, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $195, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $196, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $197, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $198, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $199, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $200, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $201, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $202, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $203, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $204, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $205, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $206, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $207, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $208, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $209, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $210, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $211, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $212, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $213, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $214, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $215, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $216, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $217, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $218, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $219, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $220, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $221, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $222, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $223, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $224, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $225, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $226, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $227, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $228, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $229, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $230, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $231, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $232, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $233, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $234, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $235, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $236, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $237, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $238, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $239, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $240, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $241, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $242, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $243, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $244, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $245, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $246, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $247, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $248, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $249, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $250, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $251, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $252, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $253, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $254, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $255, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $255, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $255, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $255, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $255, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $0, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $1, %xmm1, %xmm2
  
  movq  $0xFFFFFFFFFFFFFFFF,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $255, %xmm1, %xmm2
  
  movq  $0xFFFFFFFFFFFFFFFF,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $0, %xmm1, %xmm2
  
  movq  $0x0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  pclmulqdq $0, %xmm1, %xmm2

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
