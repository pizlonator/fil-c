    .text
    .globl  s
    .data
    .type   s, @object
 s:
    .word  123
    .text
    lu12i.w $r4,%abs_hi20(s)
    addi.d  $r4,$r4,%abs_lo12(s)
