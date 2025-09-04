.globl _start
_start:
        pcalau12i       $r5,%got_pc_hi20(_start)
        ld.d    $r5,$r5,%got_pc_lo12(_start)
        ret
