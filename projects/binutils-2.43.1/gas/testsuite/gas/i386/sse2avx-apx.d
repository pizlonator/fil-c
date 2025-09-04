#as: -msse2avx
#objdump: -dw
#name: x86-64 SSE+ with APX encoding

.*:     file format .*

Disassembly of section .text:

0+ <sse2avx>:
[ 	]*[a-f0-9]+:	62 f9 7c 08 5b 60 01 	vcvtdq2ps 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 ff 08 e6 60 01 	vcvtpd2dqx 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 fd 08 5a 60 01 	vcvtpd2psx 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7d 08 5b 60 01 	vcvtps2dq 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 fd 08 e6 60 01 	vcvttpd2dqx 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7e 08 5b 60 01 	vcvttps2dq 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7e 08 6f 60 01 	vmovdqu32 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 fd 08 28 60 01 	vmovapd 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7c 08 28 60 01 	vmovaps 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7d 08 6f 60 01 	vmovdqa32 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7e 08 6f 60 01 	vmovdqu32 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 2a 60 01 	vmovntdqa 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7e 08 16 60 01 	vmovshdup 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7e 08 12 60 01 	vmovsldup 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 fd 08 10 60 01 	vmovupd 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7c 08 10 60 01 	vmovups 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 1c 60 01 	vpabsb 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 1d 60 01 	vpabsw 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 1e 60 01 	vpabsd 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 4c 60 01 	vrcp14ps 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 4e 60 01 	vrsqrt14ps 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 fd 08 51 60 01 	vsqrtpd 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7c 08 51 60 01 	vsqrtps 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 fd 08 29 60 01 	vmovapd %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 7c 08 29 60 01 	vmovaps %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 7d 08 7f 60 01 	vmovdqa32 %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 7e 08 7f 60 01 	vmovdqu32 %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 7d 08 e7 60 01 	vmovntdq %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 fd 08 2b 60 01 	vmovntpd %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 7c 08 2b 60 01 	vmovntps %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 fd 08 11 60 01 	vmovupd %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 7c 08 11 60 01 	vmovups %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 cd 08 58 70 01 	vaddpd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4c 08 58 70 01 	vaddps 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 dc 70 01 	vaesenc 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 dd 70 01 	vaesenclast 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 de 70 01 	vaesdec 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 df 70 01 	vaesdeclast 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 55 70 01 	vandnpd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4c 08 55 70 01 	vandnps 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 54 70 01 	vandpd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4c 08 54 70 01 	vandps 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 5e 70 01 	vdivpd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4c 08 5e 70 01 	vdivps 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 cf 70 01 	vgf2p8mulb 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 5f 70 01 	vmaxpd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4c 08 5f 70 01 	vmaxps 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 5d 70 01 	vminpd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4c 08 5d 70 01 	vminps 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 59 70 01 	vmulpd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4c 08 59 70 01 	vmulps 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 56 70 01 	vorpd  0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4c 08 56 70 01 	vorps  0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 63 70 01 	vpacksswb 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 6b 70 01 	vpackssdw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 67 70 01 	vpackuswb 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 2b 70 01 	vpackusdw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 fc 70 01 	vpaddb 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 fd 70 01 	vpaddw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 fe 70 01 	vpaddd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 d4 70 01 	vpaddq 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 ec 70 01 	vpaddsb 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 ed 70 01 	vpaddsw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 dc 70 01 	vpaddusb 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 dd 70 01 	vpaddusw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 db 70 01 	vpandd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 df 70 01 	vpandnd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 e0 70 01 	vpavgb 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 e3 70 01 	vpavgw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fb 4d 08 44 70 01 00 	vpclmullqlqdq 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fb 4d 08 44 70 01 01 	vpclmulhqlqdq 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fb 4d 08 44 70 01 10 	vpclmullqhqdq 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fb 4d 08 44 70 01 11 	vpclmulhqhqdq 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 f5 70 01 	vpmaddwd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 04 70 01 	vpmaddubsw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 3c 70 01 	vpmaxsb 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 ee 70 01 	vpmaxsw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 3d 70 01 	vpmaxsd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 de 70 01 	vpmaxub 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 3e 70 01 	vpmaxuw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 3f 70 01 	vpmaxud 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 38 70 01 	vpminsb 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 ea 70 01 	vpminsw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 39 70 01 	vpminsd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 da 70 01 	vpminub 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 3a 70 01 	vpminuw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 3b 70 01 	vpminud 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa cd 08 28 70 01 	vpmuldq 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 e4 70 01 	vpmulhuw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 0b 70 01 	vpmulhrsw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 e5 70 01 	vpmulhw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 d5 70 01 	vpmullw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 40 70 01 	vpmulld 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 f4 70 01 	vpmuludq 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 eb 70 01 	vpord  0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 f6 70 01 	vpsadbw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 00 70 01 	vpshufb 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 f1 70 01 	vpsllw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 f2 70 01 	vpslld 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 f3 70 01 	vpsllq 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 e1 70 01 	vpsraw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 e2 70 01 	vpsrad 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 d1 70 01 	vpsrlw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 d2 70 01 	vpsrld 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 d3 70 01 	vpsrlq 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 f8 70 01 	vpsubb 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 f9 70 01 	vpsubw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 fa 70 01 	vpsubd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 fb 70 01 	vpsubq 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 e8 70 01 	vpsubsb 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 e9 70 01 	vpsubsw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 d8 70 01 	vpsubusb 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 d9 70 01 	vpsubusw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 68 70 01 	vpunpckhbw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 69 70 01 	vpunpckhwd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 6a 70 01 	vpunpckhdq 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 6d 70 01 	vpunpckhqdq 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 60 70 01 	vpunpcklbw 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 61 70 01 	vpunpcklwd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 62 70 01 	vpunpckldq 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 6c 70 01 	vpunpcklqdq 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4d 08 ef 70 01 	vpxord 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 5c 70 01 	vsubpd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4c 08 5c 70 01 	vsubps 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 15 70 01 	vunpckhpd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4c 08 15 70 01 	vunpckhps 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 14 70 01 	vunpcklpd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4c 08 14 70 01 	vunpcklps 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 57 70 01 	vxorpd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4c 08 57 70 01 	vxorps 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 7d 08 70 70 01 64 	vpshufd \$0x64,0x10\(%r16\),%xmm6
[ 	]*[a-f0-9]+:	62 f9 7e 08 70 70 01 64 	vpshufhw \$0x64,0x10\(%r16\),%xmm6
[ 	]*[a-f0-9]+:	62 f9 7f 08 70 70 01 64 	vpshuflw \$0x64,0x10\(%r16\),%xmm6
[ 	]*[a-f0-9]+:	62 fb fd 08 09 70 01 04 	vrndscalepd \$(0x)?4,0x10\(%r16\),%xmm6
[ 	]*[a-f0-9]+:	62 fb 7d 08 08 70 01 04 	vrndscaleps \$(0x)?4,0x10\(%r16\),%xmm6
[ 	]*[a-f0-9]+:	62 fb cd 08 ce 70 01 64 	vgf2p8affineqb \$0x64,0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fb cd 08 cf 70 01 64 	vgf2p8affineinvqb \$0x64,0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fb 4d 08 0f 70 01 64 	vpalignr \$0x64,0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fb 4d 08 44 70 01 64 	vpclmulqdq \$0x64,0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cd 08 c6 70 01 64 	vshufpd \$0x64,0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4c 08 c6 70 01 64 	vshufps \$0x64,0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 fd 08 2f 60 02 	vcomisd 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7e 08 e6 60 02 	vcvtdq2pd 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7e 08 e6 60 02 	vcvtdq2pd 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7c 08 5a 60 02 	vcvtps2pd 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 ff 08 12 60 02 	vmovddup 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 ff 08 10 60 02 	vmovsd 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 20 60 02 	vpmovsxbw 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 23 60 02 	vpmovsxwd 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 25 60 02 	vpmovsxdq 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 30 60 02 	vpmovzxbw 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 33 60 02 	vpmovzxwd 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 35 60 02 	vpmovzxdq 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 fd 08 2e 60 02 	vucomisd 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 fd 08 13 60 02 	vmovlpd %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 7c 08 13 60 02 	vmovlps %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 fd 08 17 60 02 	vmovhpd %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 7c 08 17 60 02 	vmovhps %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 ff 08 11 60 02 	vmovsd %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 dd 08 12 60 02 	vmovlpd 0x10\(%r16\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f9 5c 08 12 60 02 	vmovlps 0x10\(%r16\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f9 dd 08 16 60 02 	vmovhpd 0x10\(%r16\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f9 5c 08 16 60 02 	vmovhps 0x10\(%r16\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f9 fd 08 7e e0    	vmovq  %xmm4,%r16
[ 	]*[a-f0-9]+:	62 f9 fd 08 6e e0    	vmovq  %r16,%xmm4
[ 	]*[a-f0-9]+:	62 f9 fd 08 7e e0    	vmovq  %xmm4,%r16
[ 	]*[a-f0-9]+:	62 f9 fd 08 6e e0    	vmovq  %r16,%xmm4
[ 	]*[a-f0-9]+:	62 f9 fd 08 d6 60 02 	vmovq  %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 fe 08 7e 60 02 	vmovq  0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 e1 7f 08 2d c4    	vcvtsd2si %xmm4,%r16d
[ 	]*[a-f0-9]+:	62 f9 7f 08 2d 48 02 	vcvtsd2si 0x10\(%r16\),%ecx
[ 	]*[a-f0-9]+:	62 e1 7f 08 2c c4    	vcvttsd2si %xmm4,%r16d
[ 	]*[a-f0-9]+:	62 f9 7f 08 2c 48 02 	vcvttsd2si 0x10\(%r16\),%ecx
[ 	]*[a-f0-9]+:	62 e1 ff 08 2d c4    	vcvtsd2si %xmm4,%r16
[ 	]*[a-f0-9]+:	62 f9 ff 08 2d 48 02 	vcvtsd2si 0x10\(%r16\),%rcx
[ 	]*[a-f0-9]+:	62 e1 ff 08 2c c4    	vcvttsd2si %xmm4,%r16
[ 	]*[a-f0-9]+:	62 f9 ff 08 2c 48 02 	vcvttsd2si 0x10\(%r16\),%rcx
[ 	]*[a-f0-9]+:	62 f9 df 08 2a e0    	vcvtsi2sd %r16,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f9 df 08 2a 60 02 	vcvtsi2sdq 0x10\(%r16\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f9 de 08 2a e0    	vcvtsi2ss %r16,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f9 de 08 2a 60 02 	vcvtsi2ssq 0x10\(%r16\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 fb dd 08 22 e0 64 	vpinsrq \$0x64,%r16,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 fb dd 08 22 60 02 64 	vpinsrq \$0x64,0x10\(%r16\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 fb fd 08 16 e0 64 	vpextrq \$0x64,%xmm4,%r16
[ 	]*[a-f0-9]+:	62 fb fd 08 16 60 02 64 	vpextrq \$0x64,%xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 fb cd 08 0b 70 02 04 	vrndscalesd \$(0x)?4,0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cf 08 58 70 02 	vaddsd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cf 08 5a 70 02 	vcvtsd2ss 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cf 08 5e 70 02 	vdivsd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cf 08 5f 70 02 	vmaxsd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cf 08 5d 70 02 	vminsd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cf 08 59 70 02 	vmulsd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cf 08 51 70 02 	vsqrtsd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 cf 08 5c 70 02 	vsubsd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4e 08 58 70 04 	vaddss 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4e 08 5a 70 04 	vcvtss2sd 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4e 08 5e 70 04 	vdivss 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4e 08 5f 70 04 	vmaxss 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4e 08 5d 70 04 	vminss 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4e 08 59 70 04 	vmulss 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 4d 70 04 	vrcp14ss 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 4d 08 4f 70 04 	vrsqrt14ss 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4e 08 51 70 04 	vsqrtss 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 4e 08 5c 70 04 	vsubss 0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 f9 7c 08 2f 60 04 	vcomiss 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7e 08 10 60 04 	vmovss 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 21 60 04 	vpmovsxbd 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 24 60 04 	vpmovsxwq 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 31 60 04 	vpmovzxbd 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 34 60 04 	vpmovzxwq 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7c 08 2e 60 04 	vucomiss 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 f9 7e 08 11 60 04 	vmovss %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 7d 08 7e e0    	vmovd  %xmm4,%r16d
[ 	]*[a-f0-9]+:	62 f9 7d 08 7e 60 04 	vmovd  %xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 7d 08 6e e0    	vmovd  %r16d,%xmm4
[ 	]*[a-f0-9]+:	62 f9 7d 08 6e 60 04 	vmovd  0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 e1 7e 08 2d c4    	vcvtss2si %xmm4,%r16d
[ 	]*[a-f0-9]+:	62 f9 7e 08 2d 48 04 	vcvtss2si 0x10\(%r16\),%ecx
[ 	]*[a-f0-9]+:	62 e1 7e 08 2c c4    	vcvttss2si %xmm4,%r16d
[ 	]*[a-f0-9]+:	62 f9 7e 08 2c 48 04 	vcvttss2si 0x10\(%r16\),%ecx
[ 	]*[a-f0-9]+:	62 e1 fe 08 2d c4    	vcvtss2si %xmm4,%r16
[ 	]*[a-f0-9]+:	62 f9 fe 08 2d 48 04 	vcvtss2si 0x10\(%r16\),%rcx
[ 	]*[a-f0-9]+:	62 e1 fe 08 2c c4    	vcvttss2si %xmm4,%r16
[ 	]*[a-f0-9]+:	62 f9 fe 08 2c 48 04 	vcvttss2si 0x10\(%r16\),%rcx
[ 	]*[a-f0-9]+:	62 fb fd 08 17 e0 64 	vextractps \$0x64,%xmm4,%r16d
[ 	]*[a-f0-9]+:	62 fb 7d 08 16 e0 64 	vpextrd \$0x64,%xmm4,%r16d
[ 	]*[a-f0-9]+:	62 fb 7d 08 16 60 04 64 	vpextrd \$0x64,%xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 fb 7d 08 17 e0 64 	vextractps \$0x64,%xmm4,%r16d
[ 	]*[a-f0-9]+:	62 fb 7d 08 17 60 04 64 	vextractps \$0x64,%xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 fb 5d 08 22 e0 64 	vpinsrd \$0x64,%r16d,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 fb 5d 08 22 60 04 64 	vpinsrd \$0x64,0x10\(%r16\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f9 5f 08 2a e0    	vcvtsi2sd %r16d,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f9 5f 08 2a 60 04 	vcvtsi2sdl 0x10\(%r16\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f9 5e 08 2a e0    	vcvtsi2ss %r16d,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f9 5e 08 2a 60 04 	vcvtsi2ssl 0x10\(%r16\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 fb 4d 08 21 70 04 64 	vinsertps \$0x64,0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fb 4d 08 0a 70 04 04 	vrndscaless \$(0x)?4,0x10\(%r16\),%xmm6,%xmm6
[ 	]*[a-f0-9]+:	62 fa 7d 08 22 60 08 	vpmovsxbq 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 fa 7d 08 32 60 08 	vpmovzxbq 0x10\(%r16\),%xmm4
[ 	]*[a-f0-9]+:	62 e1 7d 08 c5 c4 64 	vpextrw \$0x64,%xmm4,%r16d
[ 	]*[a-f0-9]+:	62 e1 7d 08 c5 c4 64 	vpextrw \$0x64,%xmm4,%r16d
[ 	]*[a-f0-9]+:	62 fb 7d 08 15 60 08 64 	vpextrw \$0x64,%xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 f9 5d 08 c4 e0 64 	vpinsrw \$0x64,%r16d,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f9 5d 08 c4 e0 64 	vpinsrw \$0x64,%r16d,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 f9 5d 08 c4 60 08 64 	vpinsrw \$0x64,0x10\(%r16\),%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 fb 7d 08 14 e0 64 	vpextrb \$0x64,%xmm4,%r16d
[ 	]*[a-f0-9]+:	62 fb 7d 08 14 e0 64 	vpextrb \$0x64,%xmm4,%r16d
[ 	]*[a-f0-9]+:	62 fb 7d 08 14 60 10 64 	vpextrb \$0x64,%xmm4,0x10\(%r16\)
[ 	]*[a-f0-9]+:	62 fb 5d 08 20 e0 64 	vpinsrb \$0x64,%r16d,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 fb 5d 08 20 e0 64 	vpinsrb \$0x64,%r16d,%xmm4,%xmm4
[ 	]*[a-f0-9]+:	62 fb 5d 08 20 60 10 64 	vpinsrb \$0x64,0x10\(%r16\),%xmm4,%xmm4
#pass
