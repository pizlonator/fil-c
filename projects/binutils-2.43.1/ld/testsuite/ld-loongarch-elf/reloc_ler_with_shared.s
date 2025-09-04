      .text
      .globl  s
      .section	.tdata,"awT",@progbits
      .type   s, @object
 s:
      .word  123
      .text
      lu12i.w $r4,%le_hi20_r(s)
      add.d   $r4,$r4,$r2,%le_add_r(s)
