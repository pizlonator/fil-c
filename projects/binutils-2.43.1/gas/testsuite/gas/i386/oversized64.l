.*: Assembler messages:
.*:5: Error: instruction length.*
.*:6: Error: instruction length.*
.*:7: Error: instruction length.*
.*:9: Error: instruction length.*
.*:10: Error: instruction length.*
.*:13: Warning: instruction length.*
GAS LISTING .*


[ 	]*1[ 	]+\.text
[ 	]*2[ 	]+\.intel_syntax noprefix
[ 	]*3[ 	]+
[ 	]*4[ 	]+long64:
[ 	]*5[ 	]+\?\?\?\? 64678FEA[ 	]+lwpins	rax, fs:\[eax\*8\], 0x11223344
\*\*\*\*  Error: instruction length of 16 bytes exceeds the limit of 15
[ 	]*5[ 	]+F81204C5[ 	]*
[ 	]*5[ 	]+00000000[ 	]*
[ 	]*5[ 	]+44332211[ 	]*
[ 	]*6[ 	]+\?\?\?\? 64678FEA[ 	]+lwpval	eax, fs:\[eax\*4\], 0x11223344
\*\*\*\*  Error: instruction length of 16 bytes exceeds the limit of 15
[ 	]*6[ 	]+78120C85[ 	]*
[ 	]*6[ 	]+00000000[ 	]*
[ 	]*6[ 	]+44332211[ 	]*
[ 	]*7[ 	]+\?\?\?\? 64678FEA[ 	]+bextr	rax, fs:\[eax\*2\], 0x11223344
\*\*\*\*  Error: instruction length of 16 bytes exceeds the limit of 15
[ 	]*7[ 	]+F8100445[ 	]*
[ 	]*7[ 	]+00000000[ 	]*
[ 	]*7[ 	]+44332211[ 	]*
[ 	]*8[ 	]+
[ 	]*9[ 	]+\?\?\?\? 6567F2F0[ 	]+xacquire lock add qword ptr gs:\[eax\*8\], 0x11223344
\*\*\*\*  Error: instruction length of 16 bytes exceeds the limit of 15
[ 	]*9[ 	]+488104C5[ 	]*
[ 	]*9[ 	]+00000000[ 	]*
[ 	]*9[ 	]+44332211[ 	]*
[ 	]*10[ 	]+\?\?\?\? 6567F3F0[ 	]+xrelease lock sub qword ptr gs:\[eax\*8\], 0x11223344
\*\*\*\*  Error: instruction length of 16 bytes exceeds the limit of 15
[ 	]*10[ 	]+48812CC5[ 	]*
[ 	]*10[ 	]+00000000[ 	]*
[ 	]*10[ 	]+44332211[ 	]*
[ 	]*11[ 	]+
[ 	]*12[ 	]+\.att_syntax prefix
[ 	]*13[ 	]+\?\?\?\? 6762F1FC[ 	]+.insn EVEX.L0.0f 12/0, \$0x11223344,\(,%eax,8\),%rax
\*\*\*\*  Warning: instruction length of 16 bytes exceeds the limit of 15
[ 	]*13[ 	]+080C04C5[ 	]*
[ 	]*13[ 	]+00000000[ 	]*
[ 	]*13[ 	]+44332211[ 	]*
#pass
