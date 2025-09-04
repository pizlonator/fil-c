#as: -march=rv32ia_zacas
#source: zacas.s
#objdump: -d

.*:[ 	]+file format .*


Disassembly of section .text:

0+000 <target>:
[ 	]+[0-9a-f]+:[  	]+28a5252f[  	]+amocas.w[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+28a5252f[  	]+amocas.w[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+2ca5252f[  	]+amocas.w.aq[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+2ca5252f[  	]+amocas.w.aq[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+2aa5252f[  	]+amocas.w.rl[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+2aa5252f[  	]+amocas.w.rl[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+2ea5252f[  	]+amocas.w.aqrl[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+2ea5252f[  	]+amocas.w.aqrl[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+28a5352f[  	]+amocas.d[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+28a5352f[  	]+amocas.d[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+2ca5352f[  	]+amocas.d.aq[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+2ca5352f[  	]+amocas.d.aq[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+2aa5352f[  	]+amocas.d.rl[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+2aa5352f[  	]+amocas.d.rl[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+2ea5352f[  	]+amocas.d.aqrl[  	]+a0,a0,\(a0\)
[ 	]+[0-9a-f]+:[  	]+2ea5352f[  	]+amocas.d.aqrl[  	]+a0,a0,\(a0\)
