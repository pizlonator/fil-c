#as: -msse2avx
#objdump: -dw
#name: x86-64 SSE+ with \{evex\} prefix

.*:     file format .*

Disassembly of section .text:

0+ <sse2avx>:
[ 	]*[a-f0-9]+:	62 f1 7c 08 5b f4    	\{evex\} vcvtdq2ps %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7c 08 5b 21    	\{evex\} vcvtdq2ps \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 ff 08 e6 f4    	\{evex\} vcvtpd2dq %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 ff 08 e6 21    	\{evex\} vcvtpd2dqx \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 fd 08 5a f4    	\{evex\} vcvtpd2ps %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 fd 08 5a 21    	\{evex\} vcvtpd2psx \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7d 08 5b f4    	\{evex\} vcvtps2dq %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7d 08 5b 21    	\{evex\} vcvtps2dq \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 fd 08 e6 f4    	\{evex\} vcvttpd2dq %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 fd 08 e6 21    	\{evex\} vcvttpd2dqx \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7e 08 5b f4    	\{evex\} vcvttps2dq %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7e 08 5b 21    	\{evex\} vcvttps2dq \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 fd 08 28 f4    	\{evex\} vmovapd %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 fd 08 28 21    	\{evex\} vmovapd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7c 08 28 f4    	\{evex\} vmovaps %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7c 08 28 21    	\{evex\} vmovaps \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7d 08 6f f4    	vmovdqa32 %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7d 08 6f 21    	vmovdqa32 \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7e 08 6f f4    	vmovdqu32 %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7e 08 6f 21    	vmovdqu32 \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7e 08 16 f4    	\{evex\} vmovshdup %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7e 08 16 21    	\{evex\} vmovshdup \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7e 08 12 f4    	\{evex\} vmovsldup %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7e 08 12 21    	\{evex\} vmovsldup \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 fd 08 10 f4    	\{evex\} vmovupd %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 fd 08 10 21    	\{evex\} vmovupd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7c 08 10 f4    	\{evex\} vmovups %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7c 08 10 21    	\{evex\} vmovups \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 1c f4    	\{evex\} vpabsb %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 1c 21    	\{evex\} vpabsb \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 1d f4    	\{evex\} vpabsw %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 1d 21    	\{evex\} vpabsw \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 1e f4    	\{evex\} vpabsd %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 1e 21    	\{evex\} vpabsd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 4c f4    	vrcp14ps %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 4c 21    	vrcp14ps \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 4e f4    	vrsqrt14ps %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 4e 21    	vrsqrt14ps \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 fd 08 51 f4    	\{evex\} vsqrtpd %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 fd 08 51 21    	\{evex\} vsqrtpd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7c 08 51 f4    	\{evex\} vsqrtps %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7c 08 51 21    	\{evex\} vsqrtps \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 fd 08 28 f4    	\{evex\} vmovapd %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 fd 08 29 21    	\{evex\} vmovapd %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 7c 08 28 f4    	\{evex\} vmovaps %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7c 08 29 21    	\{evex\} vmovaps %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 7d 08 6f f4    	vmovdqa32 %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7d 08 7f 21    	vmovdqa32 %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 7e 08 6f f4    	vmovdqu32 %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7e 08 7f 21    	vmovdqu32 %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 fd 08 10 f4    	\{evex\} vmovupd %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 fd 08 11 21    	\{evex\} vmovupd %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 7c 08 10 f4    	\{evex\} vmovups %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7c 08 11 21    	\{evex\} vmovups %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 7e 08 6f 21    	vmovdqu32 \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 2a 21    	\{evex\} vmovntdqa \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7d 08 e7 21    	\{evex\} vmovntdq %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 fd 08 2b 21    	\{evex\} vmovntpd %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 7c 08 2b 21    	\{evex\} vmovntps %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 cd 08 58 f4    	\{evex\} vaddpd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 58 31    	\{evex\} vaddpd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 58 f4    	\{evex\} vaddps %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 58 31    	\{evex\} vaddps \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 dc f4    	\{evex\} vaesenc %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 dc 31    	\{evex\} vaesenc \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 dd f4    	\{evex\} vaesenclast %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 dd 31    	\{evex\} vaesenclast \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 de f4    	\{evex\} vaesdec %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 de 31    	\{evex\} vaesdec \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 df f4    	\{evex\} vaesdeclast %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 df 31    	\{evex\} vaesdeclast \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 55 f4    	\{evex\} vandnpd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 55 31    	\{evex\} vandnpd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 55 f4    	\{evex\} vandnps %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 55 31    	\{evex\} vandnps \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 54 f4    	\{evex\} vandpd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 54 31    	\{evex\} vandpd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 54 f4    	\{evex\} vandps %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 54 31    	\{evex\} vandps \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 5e f4    	\{evex\} vdivpd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 5e 31    	\{evex\} vdivpd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 5e f4    	\{evex\} vdivps %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 5e 31    	\{evex\} vdivps \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 cf f4    	\{evex\} vgf2p8mulb %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 cf 31    	\{evex\} vgf2p8mulb \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 5f f4    	\{evex\} vmaxpd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 5f 31    	\{evex\} vmaxpd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 5f f4    	\{evex\} vmaxps %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 5f 31    	\{evex\} vmaxps \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 5d f4    	\{evex\} vminpd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 5d 31    	\{evex\} vminpd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 5d f4    	\{evex\} vminps %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 5d 31    	\{evex\} vminps \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 59 f4    	\{evex\} vmulpd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 59 31    	\{evex\} vmulpd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 59 f4    	\{evex\} vmulps %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 59 31    	\{evex\} vmulps \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 56 f4    	\{evex\} vorpd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 56 31    	\{evex\} vorpd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 56 f4    	\{evex\} vorps %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 56 31    	\{evex\} vorps \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 63 f4    	\{evex\} vpacksswb %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 63 31    	\{evex\} vpacksswb \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 6b f4    	\{evex\} vpackssdw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 6b 31    	\{evex\} vpackssdw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 67 f4    	\{evex\} vpackuswb %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 67 31    	\{evex\} vpackuswb \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 2b f4    	\{evex\} vpackusdw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 2b 31    	\{evex\} vpackusdw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 fc f4    	\{evex\} vpaddb %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 fc 31    	\{evex\} vpaddb \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 fd f4    	\{evex\} vpaddw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 fd 31    	\{evex\} vpaddw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 fe f4    	\{evex\} vpaddd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 fe 31    	\{evex\} vpaddd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 d4 f4    	\{evex\} vpaddq %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 d4 31    	\{evex\} vpaddq \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 ec f4    	\{evex\} vpaddsb %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 ec 31    	\{evex\} vpaddsb \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 ed f4    	\{evex\} vpaddsw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 ed 31    	\{evex\} vpaddsw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 dc f4    	\{evex\} vpaddusb %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 dc 31    	\{evex\} vpaddusb \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 dd f4    	\{evex\} vpaddusw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 dd 31    	\{evex\} vpaddusw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 db f4    	vpandd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 db 31    	vpandd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 df f4    	vpandnd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 df 31    	vpandnd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e0 f4    	\{evex\} vpavgb %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e0 31    	\{evex\} vpavgb \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e3 f4    	\{evex\} vpavgw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e3 31    	\{evex\} vpavgw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 44 f4 00 	\{evex\} vpclmullqlqdq %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 44 31 00 	\{evex\} vpclmullqlqdq \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 44 f4 01 	\{evex\} vpclmulhqlqdq %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 44 31 01 	\{evex\} vpclmulhqlqdq \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 44 f4 10 	\{evex\} vpclmullqhqdq %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 44 31 10 	\{evex\} vpclmullqhqdq \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 44 f4 11 	\{evex\} vpclmulhqhqdq %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 44 31 11 	\{evex\} vpclmulhqhqdq \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 f5 f4    	\{evex\} vpmaddwd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 f5 31    	\{evex\} vpmaddwd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 04 f4    	\{evex\} vpmaddubsw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 04 31    	\{evex\} vpmaddubsw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 3c f4    	\{evex\} vpmaxsb %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 3c 31    	\{evex\} vpmaxsb \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 ee f4    	\{evex\} vpmaxsw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 ee 31    	\{evex\} vpmaxsw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 3d f4    	\{evex\} vpmaxsd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 3d 31    	\{evex\} vpmaxsd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 de f4    	\{evex\} vpmaxub %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 de 31    	\{evex\} vpmaxub \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 3e f4    	\{evex\} vpmaxuw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 3e 31    	\{evex\} vpmaxuw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 3f f4    	\{evex\} vpmaxud %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 3f 31    	\{evex\} vpmaxud \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 38 f4    	\{evex\} vpminsb %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 38 31    	\{evex\} vpminsb \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 ea f4    	\{evex\} vpminsw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 ea 31    	\{evex\} vpminsw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 39 f4    	\{evex\} vpminsd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 39 31    	\{evex\} vpminsd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 da f4    	\{evex\} vpminub %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 da 31    	\{evex\} vpminub \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 3a f4    	\{evex\} vpminuw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 3a 31    	\{evex\} vpminuw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 3b f4    	\{evex\} vpminud %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 3b 31    	\{evex\} vpminud \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e4 f4    	\{evex\} vpmulhuw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e4 31    	\{evex\} vpmulhuw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 0b f4    	\{evex\} vpmulhrsw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 0b 31    	\{evex\} vpmulhrsw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e5 f4    	\{evex\} vpmulhw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e5 31    	\{evex\} vpmulhw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 d5 f4    	\{evex\} vpmullw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 d5 31    	\{evex\} vpmullw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 40 f4    	\{evex\} vpmulld %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 40 31    	\{evex\} vpmulld \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 f4 f4    	\{evex\} vpmuludq %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 f4 31    	\{evex\} vpmuludq \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 cd 08 28 f4    	\{evex\} vpmuldq %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 cd 08 28 31    	\{evex\} vpmuldq \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 eb f4    	vpord  %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 eb 31    	vpord  \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 f6 f4    	\{evex\} vpsadbw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 f6 31    	\{evex\} vpsadbw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 00 f4    	\{evex\} vpshufb %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 00 31    	\{evex\} vpshufb \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 f1 f4    	\{evex\} vpsllw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 f1 31    	\{evex\} vpsllw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 f2 f4    	\{evex\} vpslld %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 f2 31    	\{evex\} vpslld \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 f3 f4    	\{evex\} vpsllq %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 f3 31    	\{evex\} vpsllq \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e1 f4    	\{evex\} vpsraw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e1 31    	\{evex\} vpsraw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e2 f4    	\{evex\} vpsrad %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e2 31    	\{evex\} vpsrad \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 d1 f4    	\{evex\} vpsrlw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 d1 31    	\{evex\} vpsrlw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 d2 f4    	\{evex\} vpsrld %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 d2 31    	\{evex\} vpsrld \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 d3 f4    	\{evex\} vpsrlq %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 d3 31    	\{evex\} vpsrlq \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 f8 f4    	\{evex\} vpsubb %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 f8 31    	\{evex\} vpsubb \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 f9 f4    	\{evex\} vpsubw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 f9 31    	\{evex\} vpsubw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 fa f4    	\{evex\} vpsubd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 fa 31    	\{evex\} vpsubd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 fb f4    	\{evex\} vpsubq %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 fb 31    	\{evex\} vpsubq \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e8 f4    	\{evex\} vpsubsb %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e8 31    	\{evex\} vpsubsb \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e9 f4    	\{evex\} vpsubsw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 e9 31    	\{evex\} vpsubsw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 d8 f4    	\{evex\} vpsubusb %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 d8 31    	\{evex\} vpsubusb \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 d9 f4    	\{evex\} vpsubusw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 d9 31    	\{evex\} vpsubusw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 68 f4    	\{evex\} vpunpckhbw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 68 31    	\{evex\} vpunpckhbw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 69 f4    	\{evex\} vpunpckhwd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 69 31    	\{evex\} vpunpckhwd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 6a f4    	\{evex\} vpunpckhdq %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 6a 31    	\{evex\} vpunpckhdq \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 6d f4    	\{evex\} vpunpckhqdq %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 6d 31    	\{evex\} vpunpckhqdq \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 60 f4    	\{evex\} vpunpcklbw %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 60 31    	\{evex\} vpunpcklbw \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 61 f4    	\{evex\} vpunpcklwd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 61 31    	\{evex\} vpunpcklwd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 62 f4    	\{evex\} vpunpckldq %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 62 31    	\{evex\} vpunpckldq \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 6c f4    	\{evex\} vpunpcklqdq %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 6c 31    	\{evex\} vpunpcklqdq \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 ef f4    	vpxord %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4d 08 ef 31    	vpxord \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 5c f4    	\{evex\} vsubpd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 5c 31    	\{evex\} vsubpd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 5c f4    	\{evex\} vsubps %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 5c 31    	\{evex\} vsubps \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 15 f4    	\{evex\} vunpckhpd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 15 31    	\{evex\} vunpckhpd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 15 f4    	\{evex\} vunpckhps %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 15 31    	\{evex\} vunpckhps \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 14 f4    	\{evex\} vunpcklpd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 14 31    	\{evex\} vunpcklpd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 14 f4    	\{evex\} vunpcklps %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 14 31    	\{evex\} vunpcklps \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 57 f4    	\{evex\} vxorpd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 57 31    	\{evex\} vxorpd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 57 f4    	\{evex\} vxorps %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 57 31    	\{evex\} vxorps \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7d 08 70 f4 64 	\{evex\} vpshufd \$0x64,%xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7d 08 70 31 64 	\{evex\} vpshufd \$0x64,\(%rcx\),%xmm6
[ 	]*[a-f0-9]+:	62 f1 7e 08 70 f4 64 	\{evex\} vpshufhw \$0x64,%xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7e 08 70 31 64 	\{evex\} vpshufhw \$0x64,\(%rcx\),%xmm6
[ 	]*[a-f0-9]+:	62 f1 7f 08 70 f4 64 	\{evex\} vpshuflw \$0x64,%xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7f 08 70 31 64 	\{evex\} vpshuflw \$0x64,\(%rcx\),%xmm6
[ 	]*[a-f0-9]+:	62 f3 fd 08 09 f4 04 	vrndscalepd \$0x4,%xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f3 fd 08 09 31 04 	vrndscalepd \$0x4,\(%rcx\),%xmm6
[ 	]*[a-f0-9]+:	62 f3 7d 08 08 f4 04 	vrndscaleps \$0x4,%xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f3 7d 08 08 31 04 	vrndscaleps \$0x4,\(%rcx\),%xmm6
[ 	]*[a-f0-9]+:	62 f3 cd 08 ce f4 64 	\{evex\} vgf2p8affineqb \$0x64,%xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 cd 08 ce 31 64 	\{evex\} vgf2p8affineqb \$0x64,\(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 cd 08 cf f4 64 	\{evex\} vgf2p8affineinvqb \$0x64,%xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 cd 08 cf 31 64 	\{evex\} vgf2p8affineinvqb \$0x64,\(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 0f f4 64 	\{evex\} vpalignr \$0x64,%xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 0f 31 64 	\{evex\} vpalignr \$0x64,\(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 44 f4 64 	\{evex\} vpclmulqdq \$0x64,%xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 44 31 64 	\{evex\} vpclmulqdq \$0x64,\(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 c6 f4 64 	\{evex\} vshufpd \$0x64,%xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cd 08 c6 31 64 	\{evex\} vshufpd \$0x64,\(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 c6 f4 64 	\{evex\} vshufps \$0x64,%xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 c6 31 64 	\{evex\} vshufps \$0x64,\(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 fd 08 2f f4    	\{evex\} vcomisd %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 fd 08 2f 21    	\{evex\} vcomisd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7e 08 e6 f4    	\{evex\} vcvtdq2pd %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7e 08 e6 21    	\{evex\} vcvtdq2pd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7e 08 e6 21    	\{evex\} vcvtdq2pd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7c 08 5a f4    	\{evex\} vcvtps2pd %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7c 08 5a 21    	\{evex\} vcvtps2pd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 ff 08 12 f4    	\{evex\} vmovddup %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 ff 08 12 21    	\{evex\} vmovddup \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 20 f4    	\{evex\} vpmovsxbw %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 20 21    	\{evex\} vpmovsxbw \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 23 f4    	\{evex\} vpmovsxwd %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 23 21    	\{evex\} vpmovsxwd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 25 f4    	\{evex\} vpmovsxdq %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 25 21    	\{evex\} vpmovsxdq \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 30 f4    	\{evex\} vpmovzxbw %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 30 21    	\{evex\} vpmovzxbw \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 33 f4    	\{evex\} vpmovzxwd %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 33 21    	\{evex\} vpmovzxwd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 35 f4    	\{evex\} vpmovzxdq %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 35 21    	\{evex\} vpmovzxdq \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 fd 08 2e f4    	\{evex\} vucomisd %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 fd 08 2e 21    	\{evex\} vucomisd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 ff 08 10 21    	\{evex\} vmovsd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 fd 08 13 21    	\{evex\} vmovlpd %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 7c 08 13 21    	\{evex\} vmovlps %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 fd 08 17 21    	\{evex\} vmovhpd %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 7c 08 17 21    	\{evex\} vmovhps %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 ff 08 11 21    	\{evex\} vmovsd %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 fd 08 7e e1    	\{evex\} vmovq %xmm4,%rcx
[ 	]*[a-f0-9]+:	62 f1 fd 08 6e e1    	\{evex\} vmovq %rcx,%xmm4
[ 	]*[a-f0-9]+:	62 f1 fd 08 7e e1    	\{evex\} vmovq %xmm4,%rcx
[ 	]*[a-f0-9]+:	62 f1 fd 08 6e e1    	\{evex\} vmovq %rcx,%xmm4
[ 	]*[a-f0-9]+:	62 f1 fd 08 d6 21    	\{evex\} vmovq %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 fe 08 7e 21    	\{evex\} vmovq \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7f 08 2d cc    	\{evex\} vcvtsd2si %xmm4,%ecx
[ 	]*[a-f0-9]+:	62 f1 7f 08 2d 09    	\{evex\} vcvtsd2si \(%rcx\),%ecx
[ 	]*[a-f0-9]+:	62 f1 7f 08 2c cc    	\{evex\} vcvttsd2si %xmm4,%ecx
[ 	]*[a-f0-9]+:	62 f1 7f 08 2c 09    	\{evex\} vcvttsd2si \(%rcx\),%ecx
[ 	]*[a-f0-9]+:	62 f1 ff 08 2d cc    	\{evex\} vcvtsd2si %xmm4,%rcx
[ 	]*[a-f0-9]+:	62 f1 ff 08 2d 09    	\{evex\} vcvtsd2si \(%rcx\),%rcx
[ 	]*[a-f0-9]+:	62 f1 ff 08 2c cc    	\{evex\} vcvttsd2si %xmm4,%rcx
[ 	]*[a-f0-9]+:	62 f1 ff 08 2c 09    	\{evex\} vcvttsd2si \(%rcx\),%rcx
[ 	]*[a-f0-9]+:	62 f1 df 08 2a e1    	\{evex\} vcvtsi2sd %rcx,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 df 08 2a 21    	\{evex\} vcvtsi2sdq \(%rcx\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 de 08 2a e1    	\{evex\} vcvtsi2ss %rcx,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 de 08 2a 21    	\{evex\} vcvtsi2ssq \(%rcx\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f3 dd 08 22 e1 64 	\{evex\} vpinsrq \$0x64,%rcx,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f3 dd 08 22 21 64 	\{evex\} vpinsrq \$0x64,\(%rcx\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f3 fd 08 16 e1 64 	\{evex\} vpextrq \$0x64,%xmm4,%rcx
[ 	]*[a-f0-9]+:	62 f3 fd 08 16 21 64 	\{evex\} vpextrq \$0x64,%xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 dd 08 12 21    	\{evex\} vmovlpd \(%rcx\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 5c 08 12 21    	\{evex\} vmovlps \(%rcx\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 dd 08 16 21    	\{evex\} vmovhpd \(%rcx\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 5c 08 16 21    	\{evex\} vmovhps \(%rcx\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f3 cd 08 0b f4 04 	vrndscalesd \$0x4,%xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 cd 08 0b 31 04 	vrndscalesd \$0x4,\(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 58 f4    	\{evex\} vaddsd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 58 31    	\{evex\} vaddsd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 5a f4    	\{evex\} vcvtsd2ss %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 5a 31    	\{evex\} vcvtsd2ss \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 5e f4    	\{evex\} vdivsd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 5e 31    	\{evex\} vdivsd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 5f f4    	\{evex\} vmaxsd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 5f 31    	\{evex\} vmaxsd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 5d f4    	\{evex\} vminsd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 5d 31    	\{evex\} vminsd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 59 f4    	\{evex\} vmulsd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 59 31    	\{evex\} vmulsd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 51 f4    	\{evex\} vsqrtsd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 51 31    	\{evex\} vsqrtsd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 5c f4    	\{evex\} vsubsd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 5c 31    	\{evex\} vsubsd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 58 f4    	\{evex\} vaddss %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 58 31    	\{evex\} vaddss \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 5a f4    	\{evex\} vcvtss2sd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 5a 31    	\{evex\} vcvtss2sd \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 5e f4    	\{evex\} vdivss %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 5e 31    	\{evex\} vdivss \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 5f f4    	\{evex\} vmaxss %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 5f 31    	\{evex\} vmaxss \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 5d f4    	\{evex\} vminss %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 5d 31    	\{evex\} vminss \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 59 f4    	\{evex\} vmulss %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 59 31    	\{evex\} vmulss \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 4d f4    	vrcp14ss %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 4d 31    	vrcp14ss \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 4f f4    	vrsqrt14ss %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 4d 08 4f 31    	vrsqrt14ss \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 51 f4    	\{evex\} vsqrtss %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 51 31    	\{evex\} vsqrtss \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 5c f4    	\{evex\} vsubss %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 5c 31    	\{evex\} vsubss \(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7c 08 2f f4    	\{evex\} vcomiss %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7c 08 2f 21    	\{evex\} vcomiss \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 21 f4    	\{evex\} vpmovsxbd %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 21 21    	\{evex\} vpmovsxbd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 24 f4    	\{evex\} vpmovsxwq %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 24 21    	\{evex\} vpmovsxwq \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 31 f4    	\{evex\} vpmovzxbd %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 31 21    	\{evex\} vpmovzxbd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 34 f4    	\{evex\} vpmovzxwq %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 34 21    	\{evex\} vpmovzxwq \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7c 08 2e f4    	\{evex\} vucomiss %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 7c 08 2e 21    	\{evex\} vucomiss \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7e 08 10 21    	\{evex\} vmovss \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7e 08 11 21    	\{evex\} vmovss %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 7d 08 7e e1    	\{evex\} vmovd %xmm4,%ecx
[ 	]*[a-f0-9]+:	62 f1 7d 08 7e 21    	\{evex\} vmovd %xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 7d 08 6e e1    	\{evex\} vmovd %ecx,%xmm4
[ 	]*[a-f0-9]+:	62 f1 7d 08 6e 21    	\{evex\} vmovd \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7e 08 2d cc    	\{evex\} vcvtss2si %xmm4,%ecx
[ 	]*[a-f0-9]+:	62 f1 7e 08 2d 09    	\{evex\} vcvtss2si \(%rcx\),%ecx
[ 	]*[a-f0-9]+:	62 f1 7e 08 2c cc    	\{evex\} vcvttss2si %xmm4,%ecx
[ 	]*[a-f0-9]+:	62 f1 7e 08 2c 09    	\{evex\} vcvttss2si \(%rcx\),%ecx
[ 	]*[a-f0-9]+:	62 f1 fe 08 2d cc    	\{evex\} vcvtss2si %xmm4,%rcx
[ 	]*[a-f0-9]+:	62 f1 fe 08 2d 09    	\{evex\} vcvtss2si \(%rcx\),%rcx
[ 	]*[a-f0-9]+:	62 f1 fe 08 2c cc    	\{evex\} vcvttss2si %xmm4,%rcx
[ 	]*[a-f0-9]+:	62 f1 fe 08 2c 09    	\{evex\} vcvttss2si \(%rcx\),%rcx
[ 	]*[a-f0-9]+:	62 f3 fd 08 17 e1 64 	\{evex\} vextractps \$0x64,%xmm4,%ecx
[ 	]*[a-f0-9]+:	62 f3 7d 08 17 21 64 	\{evex\} vextractps \$0x64,%xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f3 7d 08 16 e1 64 	\{evex\} vpextrd \$0x64,%xmm4,%ecx
[ 	]*[a-f0-9]+:	62 f3 7d 08 16 21 64 	\{evex\} vpextrd \$0x64,%xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f3 7d 08 17 e1 64 	\{evex\} vextractps \$0x64,%xmm4,%ecx
[ 	]*[a-f0-9]+:	62 f3 7d 08 17 21 64 	\{evex\} vextractps \$0x64,%xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 5f 08 2a e1    	\{evex\} vcvtsi2sd %ecx,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 5f 08 2a 21    	\{evex\} vcvtsi2sdl \(%rcx\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 5e 08 2a e1    	\{evex\} vcvtsi2ss %ecx,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 5e 08 2a 21    	\{evex\} vcvtsi2ssl \(%rcx\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f3 4d 08 21 f4 64 	\{evex\} vinsertps \$0x64,%xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 21 31 64 	\{evex\} vinsertps \$0x64,\(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 0a f4 04 	vrndscaless \$0x4,%xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f3 4d 08 0a 31 04 	vrndscaless \$0x4,\(%rcx\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 22 f4    	\{evex\} vpmovsxbq %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 22 21    	\{evex\} vpmovsxbq \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f2 7d 08 32 f4    	\{evex\} vpmovzxbq %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f2 7d 08 32 21    	\{evex\} vpmovzxbq \(%rcx\),%xmm4
[ 	]*[a-f0-9]+:	62 f1 7d 08 c5 cc 64 	\{evex\} vpextrw \$0x64,%xmm4,%ecx
[ 	]*[a-f0-9]+:	62 f1 7d 08 c5 cc 64 	\{evex\} vpextrw \$0x64,%xmm4,%ecx
[ 	]*[a-f0-9]+:	62 f3 7d 08 15 21 64 	\{evex\} vpextrw \$0x64,%xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f1 5d 08 c4 e1 64 	\{evex\} vpinsrw \$0x64,%ecx,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 5d 08 c4 e1 64 	\{evex\} vpinsrw \$0x64,%ecx,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 5d 08 c4 21 64 	\{evex\} vpinsrw \$0x64,\(%rcx\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f3 7d 08 14 e1 64 	\{evex\} vpextrb \$0x64,%xmm4,%ecx
[ 	]*[a-f0-9]+:	62 f3 7d 08 14 e1 64 	\{evex\} vpextrb \$0x64,%xmm4,%ecx
[ 	]*[a-f0-9]+:	62 f3 7d 08 14 21 64 	\{evex\} vpextrb \$0x64,%xmm4,\(%rcx\)
[ 	]*[a-f0-9]+:	62 f3 5d 08 20 e1 64 	\{evex\} vpinsrb \$0x64,%ecx,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f3 5d 08 20 e1 64 	\{evex\} vpinsrb \$0x64,%ecx,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f3 5d 08 20 21 64 	\{evex\} vpinsrb \$0x64,\(%rcx\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 fe 08 7e f4    	\{evex\} vmovq %xmm4,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 12 f4    	\{evex\} vmovhlps %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4c 08 16 f4    	\{evex\} vmovlhps %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 cf 08 10 f4    	\{evex\} vmovsd %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 4e 08 10 f4    	\{evex\} vmovss %xmm4,%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f1 5d 08 72 f4 64 	\{evex\} vpslld \$0x64,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 5d 08 73 fc 64 	\{evex\} vpslldq \$0x64,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 dd 08 73 f4 64 	\{evex\} vpsllq \$0x64,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 5d 08 71 f4 64 	\{evex\} vpsllw \$0x64,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 5d 08 72 e4 64 	\{evex\} vpsrad \$0x64,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 5d 08 71 e4 64 	\{evex\} vpsraw \$0x64,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 5d 08 72 d4 64 	\{evex\} vpsrld \$0x64,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 5d 08 73 dc 64 	\{evex\} vpsrldq \$0x64,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 dd 08 73 d4 64 	\{evex\} vpsrlq \$0x64,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f1 5d 08 71 d4 64 	\{evex\} vpsrlw \$0x64,%xmm4,%xmm4
#pass
