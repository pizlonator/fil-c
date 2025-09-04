#as: -march=rv64i_zfbfmin
#objdump: -d

.*:[ 	]+file format .*

Disassembly of section .text:

0+000 <target>:
[ 	]+[0-9a-f]+:[ 	]+4485f553[ 	]+fcvt.bf16.s[ 	]+fa0,fa1
[ 	]+[0-9a-f]+:[ 	]+44858553[ 	]+fcvt.bf16.s[ 	]+fa0,fa1,rne
[ 	]+[0-9a-f]+:[ 	]+40658553[ 	]+fcvt.s.bf16[ 	]+fa0,fa1
