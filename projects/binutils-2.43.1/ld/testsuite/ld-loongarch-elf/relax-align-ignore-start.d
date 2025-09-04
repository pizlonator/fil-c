#as: -mignore-start-align
#ld: -e0
#objdump: -d

.*:[    ]+file format .*


Disassembly of section aaa:
0000000120000078 <aaa>:
[ 	]+120000078:[ 	]+4c000020[ 	]+ret
Disassembly of section bbb:
0000000120000080 <bbb>:
[ 	]+120000080:[ 	]+4c000020[ 	]+ret
Disassembly of section ccc:
0000000120000090 <__bss_start-0x4004>:
[ 	]+120000090:[ 	]+4c000020[ 	]+ret
