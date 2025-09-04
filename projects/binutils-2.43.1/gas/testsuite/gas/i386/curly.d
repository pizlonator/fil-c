#objdump: -dw
#name: i386 curly braces with blanks

.*: +file format .*

Disassembly of section .text:

0+ <curly>:
[ 	]*[a-f0-9]+:	62 f1 74 58 58 10    	vaddps \(%eax\)\{1to16\},%zmm1,%zmm2
[ 	]*[a-f0-9]+:	62 f1 74 58 58 10    	vaddps \(%eax\)\{1to16\},%zmm1,%zmm2
[ 	]*[a-f0-9]+:	62 f1 74 58 58 10    	vaddps \(%eax\)\{1to16\},%zmm1,%zmm2
[ 	]*[a-f0-9]+:	62 f1 74 58 58 10    	vaddps \(%eax\)\{1to16\},%zmm1,%zmm2
[ 	]*[a-f0-9]+:	62 f1 74 18 58 d0    	vaddps \{rn-sae\},%zmm0,%zmm1,%zmm2
[ 	]*[a-f0-9]+:	62 f1 74 18 58 d0    	vaddps \{rn-sae\},%zmm0,%zmm1,%zmm2
[ 	]*[a-f0-9]+:	62 f1 74 18 58 d0    	vaddps \{rn-sae\},%zmm0,%zmm1,%zmm2
[ 	]*[a-f0-9]+:	62 f1 74 cb 58 d0    	vaddps %zmm0,%zmm1,%zmm2\{%k3\}\{z\}
[ 	]*[a-f0-9]+:	62 f1 74 cb 58 d0    	vaddps %zmm0,%zmm1,%zmm2\{%k3\}\{z\}
[ 	]*[a-f0-9]+:	62 f1 74 cb 58 d0    	vaddps %zmm0,%zmm1,%zmm2\{%k3\}\{z\}
[ 	]*[a-f0-9]+:	62 f1 74 cb 58 d0    	vaddps %zmm0,%zmm1,%zmm2\{%k3\}\{z\}
[ 	]*[a-f0-9]+:	62 f1 74 cb 58 d0    	vaddps %zmm0,%zmm1,%zmm2\{%k3\}\{z\}
[ 	]*[a-f0-9]+:	62 f1 74 cb 58 d0    	vaddps %zmm0,%zmm1,%zmm2\{%k3\}\{z\}
[ 	]*[a-f0-9]+:	62 f1 74 08 58 d0    	\{evex\} vaddps %xmm0,%xmm1,%xmm2
[ 	]*[a-f0-9]+:	62 f1 74 08 58 d0    	\{evex\} vaddps %xmm0,%xmm1,%xmm2
[ 	]*[a-f0-9]+:	62 f1 74 08 58 d0    	\{evex\} vaddps %xmm0,%xmm1,%xmm2
[ 	]*[a-f0-9]+:	62 f1 74 18 58 c2    	vaddps \{rn-sae\},%zmm2,%zmm1,%zmm0
[ 	]*[a-f0-9]+:	62 f1 74 18 58 c2    	vaddps \{rn-sae\},%zmm2,%zmm1,%zmm0
[ 	]*[a-f0-9]+:	62 f1 74 18 58 c2    	vaddps \{rn-sae\},%zmm2,%zmm1,%zmm0
#pass
