 .text
.globl _start

_start:
  // PDEP  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $0, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $1, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $2, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $3, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $4, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $5, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $6, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $7, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $8, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $9, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $10, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $11, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $12, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $13, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $14, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $15, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $16, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $17, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $18, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $19, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $20, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $21, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $22, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $23, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $24, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $25, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $26, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $27, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $28, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $29, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $30, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $31, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $32, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $33, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $34, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $35, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $36, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $37, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $38, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $39, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $40, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $41, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $42, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $43, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $44, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $45, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $46, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $47, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $48, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $49, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $50, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $51, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $52, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $53, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $54, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $55, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $56, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $57, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $58, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $59, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $60, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $61, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $62, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $63, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $64, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $65, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $66, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $67, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $68, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $69, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $70, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $71, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $72, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $73, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $74, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $75, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $76, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $77, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $78, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $79, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $80, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $81, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $82, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $83, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $84, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $85, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $86, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $87, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $88, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $89, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $90, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $91, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $92, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $93, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $94, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $95, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $96, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $97, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $98, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $99, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $100, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $101, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $102, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $103, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $104, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $105, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $106, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $107, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $108, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $109, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $110, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $111, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $112, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $113, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $114, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $115, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $116, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $117, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $118, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $119, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $120, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $121, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $122, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $123, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $124, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $125, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $126, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $127, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $128, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $129, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $130, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $131, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $132, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $133, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $134, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $135, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $136, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $137, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $138, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $139, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $140, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $141, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $142, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $143, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $144, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $145, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $146, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $147, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $148, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $149, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $150, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $151, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $152, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $153, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $154, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $155, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $156, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $157, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $158, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $159, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $160, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $161, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $162, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $163, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $164, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $165, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $166, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $167, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $168, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $169, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $170, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $171, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $172, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $173, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $174, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $175, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $176, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $177, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $178, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $179, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $180, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $181, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $182, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $183, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $184, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $185, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $186, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $187, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $188, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $189, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $190, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $191, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $192, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $193, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $194, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $195, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $196, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $197, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $198, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $199, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $200, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $201, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $202, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $203, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $204, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $205, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $206, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $207, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $208, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $209, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $210, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $211, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $212, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $213, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $214, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $215, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $216, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $217, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $218, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $219, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $220, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $221, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $222, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $223, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $224, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $225, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $226, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $227, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $228, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $229, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $230, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $231, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $232, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $233, %xmm1, %xmm2
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $234, %xmm1, %xmm2
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $235, %xmm1, %xmm2
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $236, %xmm1, %xmm2
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $237, %xmm1, %xmm2
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $238, %xmm1, %xmm2
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $239, %xmm1, %xmm2
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $240, %xmm1, %xmm2
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $241, %xmm1, %xmm2
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $242, %xmm1, %xmm2
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $243, %xmm1, %xmm2
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $244, %xmm1, %xmm2
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $245, %xmm1, %xmm2
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $246, %xmm1, %xmm2
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $247, %xmm1, %xmm2
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $248, %xmm1, %xmm2
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $249, %xmm1, %xmm2
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $250, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $251, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $252, %xmm1, %xmm2
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $253, %xmm1, %xmm2
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $254, %xmm1, %xmm2
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $255, %xmm1, %xmm2
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $255, %xmm1, %xmm2
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $255, %xmm1, %xmm2
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $255, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $255, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $0, %xmm1, %xmm2
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $1, %xmm1, %xmm2
  
  movq  $0xFFFFFFFFFFFFFFFF,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $255, %xmm1, %xmm2
  
  movq  $0xFFFFFFFFFFFFFFFF,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $0, %xmm1, %xmm2
  
  movq  $0x0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  
  mpsadbw $0, %xmm1, %xmm2

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
