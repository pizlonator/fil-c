#as:
#objdump: -dr
#skip: loongarch32-*-*

.*:[    ]+file format .*


Disassembly of section .text:

[ 	]*0000000000000000 <.text>:
[ 	]+0:[ 	]+1e000001[ 	]+pcaddu18i[ 	]+\$ra, 0
[ 	]+0: R_LARCH_CALL36[ 	]+a
[ 	]+0: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+4:[ 	]+4c000021[ 	]+jirl[ 	]+\$ra, \$ra, 0
[ 	]+8:[ 	]+1e000001[ 	]+pcaddu18i[ 	]+\$ra, 0
[ 	]+8: R_LARCH_CALL36[ 	]+a
[ 	]+8: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+c:[ 	]+4c000021[ 	]+jirl[ 	]+\$ra, \$ra, 0
[ 	]+10:[ 	]+1e00000c[ 	]+pcaddu18i[ 	]+\$t0, 0
[ 	]+10: R_LARCH_CALL36[ 	]+a
[ 	]+10: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+14:[ 	]+4c000180[ 	]+jr[ 	]+\$t0
[ 	]+18:[ 	]+1e00000c[ 	]+pcaddu18i[ 	]+\$t0, 0
[ 	]+18: R_LARCH_CALL36[ 	]+a
[ 	]+18: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+1c:[ 	]+4c000180[ 	]+jr[ 	]+\$t0
