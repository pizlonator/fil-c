#as: -O -msse2avx
#objdump: -drw
#name: x86-64 PEXTR optimized encoding with -msse2avx

.*: +file format .*


Disassembly of section .text:

0+ <pextr>:
 +[a-f0-9]+:	c5 f9 7e ca          	vmovd  %xmm1,%edx
 +[a-f0-9]+:	c5 f9 7e 0a          	vmovd  %xmm1,\(%rdx\)
 +[a-f0-9]+:	62 f9 7d 08 7e ca    	vmovd  %xmm1,%r18d
 +[a-f0-9]+:	62 f9 7d 08 7e 0a    	vmovd  %xmm1,\(%r18\)
 +[a-f0-9]+:	c5 f9 7e ca          	vmovd  %xmm1,%edx
 +[a-f0-9]+:	c5 f9 7e 0a          	vmovd  %xmm1,\(%rdx\)
 +[a-f0-9]+:	62 e1 7d 08 7e ca    	vmovd  %xmm17,%edx
 +[a-f0-9]+:	62 f9 7d 08 7e ca    	vmovd  %xmm1,%r18d
 +[a-f0-9]+:	62 f9 7d 08 7e 0a    	vmovd  %xmm1,\(%r18\)
 +[a-f0-9]+:	c4 e1 f9 7e ca       	vmovq  %xmm1,%rdx
 +[a-f0-9]+:	c5 f9 d6 0a          	vmovq  %xmm1,\(%rdx\)
 +[a-f0-9]+:	62 f9 fd 08 7e ca    	vmovq  %xmm1,%r18
 +[a-f0-9]+:	62 f9 fd 08 7e 0a    	vmovq  %xmm1,\(%r18\)
 +[a-f0-9]+:	c4 e1 f9 7e ca       	vmovq  %xmm1,%rdx
 +[a-f0-9]+:	c5 f9 d6 0a          	vmovq  %xmm1,\(%rdx\)
 +[a-f0-9]+:	62 e1 fd 08 7e ca    	vmovq  %xmm17,%rdx
 +[a-f0-9]+:	62 f9 fd 08 7e ca    	vmovq  %xmm1,%r18
 +[a-f0-9]+:	62 f9 fd 08 7e 0a    	vmovq  %xmm1,\(%r18\)
#pass
