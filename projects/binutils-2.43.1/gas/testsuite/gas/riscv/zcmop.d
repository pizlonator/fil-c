#as: -march=rv64i_zcmop
#objdump: -d

.*:[ 	]+file format .*

Disassembly of section .text:

0+000 <target>:
[ 	]+[0-9a-f]+:[ 	]+6081[ 	]+c.mop.1
[ 	]+[0-9a-f]+:[ 	]+6181[ 	]+c.mop.3
[ 	]+[0-9a-f]+:[ 	]+6281[ 	]+c.mop.5
[ 	]+[0-9a-f]+:[ 	]+6381[ 	]+c.mop.7
[ 	]+[0-9a-f]+:[ 	]+6481[ 	]+c.mop.9
[ 	]+[0-9a-f]+:[ 	]+6581[ 	]+c.mop.11
[ 	]+[0-9a-f]+:[ 	]+6681[ 	]+c.mop.13
[ 	]+[0-9a-f]+:[ 	]+6781[ 	]+c.mop.15
