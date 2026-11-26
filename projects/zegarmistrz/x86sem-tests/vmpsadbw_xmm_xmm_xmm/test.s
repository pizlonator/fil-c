 .text
.globl _start

_start:
  // PDEP  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $0, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $1, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $2, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $3, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $4, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $5, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $6, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $7, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $8, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $9, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $10, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $11, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $12, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $13, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $14, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $15, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $16, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $17, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $18, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $19, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $20, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $21, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $22, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $23, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $24, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $25, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $26, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $27, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $28, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $29, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $30, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $31, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $32, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $33, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $34, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $35, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $36, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $37, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $38, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $39, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $40, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $41, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $42, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $43, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $44, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $45, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $46, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $47, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $48, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $49, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $50, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $51, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $52, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $53, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $54, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $55, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $56, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $57, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $58, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $59, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $60, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $61, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $62, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $63, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $64, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $65, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $66, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $67, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $68, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $69, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $70, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $71, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $72, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $73, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $74, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $75, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $76, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $77, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $78, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $79, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $80, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $81, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $82, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $83, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $84, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $85, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $86, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $87, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $88, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $89, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $90, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $91, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $92, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $93, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $94, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $95, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $96, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $97, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $98, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $99, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $100, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $101, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $102, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $103, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $104, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $105, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $106, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $107, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $108, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $109, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $110, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $111, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $112, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $113, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $114, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $115, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $116, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $117, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $118, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $119, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $120, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $121, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $122, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $123, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $124, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $125, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $126, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $127, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $128, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $129, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $130, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $131, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $132, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $133, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $134, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $135, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $136, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $137, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $138, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $139, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $140, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $141, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $142, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $143, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $144, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $145, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $146, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $147, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $148, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $149, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $150, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $151, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $152, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $153, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $154, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $155, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $156, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $157, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $158, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $159, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $160, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $161, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $162, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $163, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $164, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $165, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $166, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $167, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $168, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $169, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $170, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $171, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $172, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $173, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $174, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $175, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $176, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $177, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $178, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $179, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $180, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $181, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $182, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $183, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $184, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $185, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $186, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $187, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $188, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $189, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $190, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $191, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $192, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $193, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $194, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $195, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $196, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $197, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $198, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $199, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $200, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $201, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $202, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $203, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $204, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $205, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $206, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $207, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $208, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $209, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $210, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $211, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $212, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $213, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $214, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $215, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $216, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $217, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $218, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $219, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $220, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $221, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $222, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $223, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $224, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $225, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $226, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $227, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $228, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $229, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $230, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $231, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $232, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $233, %xmm1, %xmm2, %xmm3
  
  movq  $0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $234, %xmm1, %xmm2, %xmm3
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $235, %xmm1, %xmm2, %xmm3
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $236, %xmm1, %xmm2, %xmm3
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $237, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $238, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $239, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $240, %xmm1, %xmm2, %xmm3
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $241, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $242, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $243, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $244, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $245, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $246, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $247, %xmm1, %xmm2, %xmm3
  
  movq  $0x80000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $248, %xmm1, %xmm2, %xmm3
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $249, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $250, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $251, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $252, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $253, %xmm1, %xmm2, %xmm3
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $254, %xmm1, %xmm2, %xmm3
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $255, %xmm1, %xmm2, %xmm3
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $255, %xmm1, %xmm2, %xmm3
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $255, %xmm1, %xmm2, %xmm3
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $255, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $255, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $0, %xmm1, %xmm2, %xmm3
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $1, %xmm1, %xmm2, %xmm3
  
  movq  $0xFFFFFFFFFFFFFFFF,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $255, %xmm1, %xmm2, %xmm3
  
  movq  $0xFFFFFFFFFFFFFFFF,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $0, %xmm1, %xmm2, %xmm3
  
  movq  $0x0,  %rax
  movq  %rax, %xmm0
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  
  vmpsadbw $0, %xmm1, %xmm2, %xmm3

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
