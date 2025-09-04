#as: -march=rv64iv_zvfbfmin
#objdump: -d

.*:[ 	]+file format .*

Disassembly of section .text:

0+000 <target>:
[ 	]+[0-9a-f]+:[ 	]+4a8e9257[ 	]+vfncvtbf16.f.f.w[ 	]+v4,v8
[ 	]+[0-9a-f]+:[ 	]+488e9257[ 	]+vfncvtbf16.f.f.w[ 	]+v4,v8,v0.t
[ 	]+[0-9a-f]+:[ 	]+4a869257[ 	]+vfwcvtbf16.f.f.v[ 	]+v4,v8
[ 	]+[0-9a-f]+:[ 	]+48869257[ 	]+vfwcvtbf16.f.f.v[ 	]+v4,v8,v0.t
