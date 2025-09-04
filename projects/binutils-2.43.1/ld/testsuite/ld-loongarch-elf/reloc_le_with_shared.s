    .text
    .globl  s
    .section  .tdata,"awT",@progbits
    .type     s, @object
 s:
    .word  123
    .text
    la.tls.le $r4, s
