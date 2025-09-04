#source: tlsdesc.s
#objdump: -dr

.*:[ 	]+file format .*


Disassembly of section .text:

0+00 <_start>:
[ 	]+0:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+0:[ 	]+R_RISCV_TLSDESC_HI20[ 	]+sg1
[ 	]+0:[ 	]+R_RISCV_RELAX[ 	]+\*ABS\*
[ 	]+4:[ 	]+00052283[ 	]+lw[ 	]+t0,0\(a0\) # 0( <.*>)?
[ 	]+4:[ 	]+R_RISCV_TLSDESC_LOAD_LO12[ 	]+\.desc1
[ 	]+4:[ 	]+R_RISCV_RELAX[ 	]+\*ABS\*
[ 	]+8:[ 	]+00050513[ 	]+mv[ 	]+a0,a0
[ 	]+8:[ 	]+R_RISCV_TLSDESC_ADD_LO12[ 	]+\.desc1
[ 	]+8:[ 	]+R_RISCV_RELAX[ 	]+\*ABS\*
[ 	]+c:[ 	]+000282e7[ 	]+jalr[ 	]+t0,t0
[ 	]+c:[ 	]+R_RISCV_TLSDESC_CALL[ 	]+\.desc1
[ 	]+c:[ 	]+R_RISCV_RELAX[ 	]+\*ABS\*

0+10 <\.desc2>:
[ 	]+10:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+10:[ 	]+R_RISCV_TLSDESC_HI20[ 	]+sl1
[ 	]+10:[ 	]+R_RISCV_RELAX[ 	]+\*ABS\*
[ 	]+14:[ 	]+00052283[ 	]+lw[ 	]+t0,0\(a0\) # 10( <.*>)?
[ 	]+14:[ 	]+R_RISCV_TLSDESC_LOAD_LO12[ 	]+\.desc2
[ 	]+14:[ 	]+R_RISCV_RELAX[ 	]+\*ABS\*
[ 	]+18:[ 	]+00050513[ 	]+mv[ 	]+a0,a0
[ 	]+18:[ 	]+R_RISCV_TLSDESC_ADD_LO12[ 	]+\.desc2
[ 	]+18:[ 	]+R_RISCV_RELAX[ 	]+\*ABS\*
[ 	]+1c:[ 	]+000282e7[ 	]+jalr[ 	]+t0,t0
[ 	]+1c:[ 	]+R_RISCV_TLSDESC_CALL[ 	]+\.desc2
[ 	]+1c:[ 	]+R_RISCV_RELAX[ 	]+\*ABS\*
[ 	]+20:[ 	]+00008067[ 	]+ret
