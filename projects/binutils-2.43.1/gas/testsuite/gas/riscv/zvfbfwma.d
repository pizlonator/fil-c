#as: -march=rv64iv_zvfbfwma
#objdump: -d

.*:[ 	]+file format .*

Disassembly of section .text:

0+000 <target>:
[ 	]+[0-9a-f]+:[ 	]+ee865257[ 	]+vfwmaccbf16.vf[ 	]+v4,fa2,v8
[ 	]+[0-9a-f]+:[ 	]+ec865257[ 	]+vfwmaccbf16.vf[ 	]+v4,fa2,v8,v0.t
[ 	]+[0-9a-f]+:[ 	]+ee861257[ 	]+vfwmaccbf16.vv[ 	]+v4,v12,v8
[ 	]+[0-9a-f]+:[ 	]+ec861257[ 	]+vfwmaccbf16.vv[ 	]+v4,v12,v8,v0.t
