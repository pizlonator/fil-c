#as: -march=rv32i_xcvbi
#objdump: -d

.*:[ 	]+file format .*


Disassembly of section .text:

0+000 <foo>:
[ 	]+[0-9a-f]+:[ 	]+0102e00b[ 	]+cv.beqimm[ 	]+t0,-16,0[ 	]+<foo>
[ 	]+[0-9a-f]+:[ 	]+fe5eee8b[ 	]+cv.beqimm[ 	]+t4,5,0[ 	]+<foo>
[ 	]+[0-9a-f]+:[ 	]+fef3ec8b[ 	]+cv.beqimm[ 	]+t2,15,0[ 	]+<foo>
[ 	]+[0-9a-f]+:[ 	]+ff02fa8b[ 	]+cv.bneimm[ 	]+t0,-16,0[ 	]+<foo>
[ 	]+[0-9a-f]+:[ 	]+fe5ef88b[ 	]+cv.bneimm[ 	]+t4,5,0[ 	]+<foo>
[ 	]+[0-9a-f]+:[ 	]+fef3f68b[ 	]+cv.bneimm[ 	]+t2,15,0[ 	]+<foo>
