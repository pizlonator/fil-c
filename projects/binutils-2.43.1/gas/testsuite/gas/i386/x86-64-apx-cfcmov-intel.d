#as:
#objdump: -dw -Mintel
#name: x86_64 APX_F insns (Intel disassembly)
#source: x86-64-apx-cfcmov.s

.*: +file format .*

Disassembly of section \.text:

0+ <_start>:
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 42 c2[ 	]+cfcmovb ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 14 42 c2[ 	]+cfcmovb r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 0c 42 94 80 23 01 00 00[ 	]+cfcmovb WORD PTR \[r8\+rax\*4\+0x123\],dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7c 08 42 d1[ 	]+cfcmovb edx,ecx
[ 	]*[a-f0-9]+:[ 	]*62 f4 2c 1c 42 d1[ 	]+cfcmovb r10d,edx,ecx
[ 	]*[a-f0-9]+:[ 	]*62 d4 7c 0c 42 8c 80 23 01 00 00[ 	]+cfcmovb DWORD PTR \[r8\+rax\*4\+0x123\],ecx
[ 	]*[a-f0-9]+:[ 	]*62 5c fc 08 42 ff[ 	]+cfcmovb r15,r31
[ 	]*[a-f0-9]+:[ 	]*62 5c a4 1c 42 ff[ 	]+cfcmovb r11,r15,r31
[ 	]*[a-f0-9]+:[ 	]*62 44 fc 0c 42 bc 80 23 01 00 00[ 	]+cfcmovb QWORD PTR \[r8\+rax\*4\+0x123\],r31
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 08 42 94 80 23 01 00 00[ 	]+cfcmovb dx,WORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 1c 42 94 80 23 01 00 00[ 	]+cfcmovb ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 d4 7c 08 42 8c 80 23 01 00 00[ 	]+cfcmovb ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 d4 6c 1c 42 8c 80 23 01 00 00[ 	]+cfcmovb edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 44 fc 08 42 bc 80 23 01 00 00[ 	]+cfcmovb r31,QWORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 44 84 1c 42 bc 80 23 01 00 00[ 	]+cfcmovb r15,r31,QWORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 42 c2[ 	]+cmovb  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 2c 18 42 d1[ 	]+cmovb  r10d,edx,ecx
[ 	]*[a-f0-9]+:[ 	]*62 5c a4 18 42 ff[ 	]+cmovb  r11,r15,r31
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 18 42 94 80 23 01 00 00[ 	]+cmovb  ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 d4 6c 18 42 8c 80 23 01 00 00[ 	]+cmovb  edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 44 84 18 42 bc 80 23 01 00 00[ 	]+cmovb  r15,r31,QWORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 40 c2[ 	]+cfcmovo ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 40 c2[ 	]+cmovo  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 41 c2[ 	]+cfcmovno ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 41 c2[ 	]+cmovno r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 42 c2[ 	]+cfcmovb ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 42 c2[ 	]+cmovb  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 42 c2[ 	]+cfcmovb ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 42 c2[ 	]+cmovb  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 43 c2[ 	]+cfcmovae ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 43 c2[ 	]+cmovae r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 43 c2[ 	]+cfcmovae ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 43 c2[ 	]+cmovae r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 44 c2[ 	]+cfcmove ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 44 c2[ 	]+cmove  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 44 c2[ 	]+cfcmove ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 44 c2[ 	]+cmove  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 45 c2[ 	]+cfcmovne ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 45 c2[ 	]+cmovne r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 45 c2[ 	]+cfcmovne ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 45 c2[ 	]+cmovne r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 46 c2[ 	]+cfcmovbe ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 46 c2[ 	]+cmovbe r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 46 c2[ 	]+cfcmovbe ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 46 c2[ 	]+cmovbe r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 47 c2[ 	]+cfcmova ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 47 c2[ 	]+cmova  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 47 c2[ 	]+cfcmova ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 47 c2[ 	]+cmova  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 48 c2[ 	]+cfcmovs ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 48 c2[ 	]+cmovs  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 49 c2[ 	]+cfcmovns ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 49 c2[ 	]+cmovns r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4a c2[ 	]+cfcmovp ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4a c2[ 	]+cmovp  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4a c2[ 	]+cfcmovp ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4a c2[ 	]+cmovp  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4b c2[ 	]+cfcmovnp ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4b c2[ 	]+cmovnp r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4b c2[ 	]+cfcmovnp ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4b c2[ 	]+cmovnp r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4c c2[ 	]+cfcmovl ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4c c2[ 	]+cmovl  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4c c2[ 	]+cfcmovl ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4c c2[ 	]+cmovl  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4d c2[ 	]+cfcmovge ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4d c2[ 	]+cmovge r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4d c2[ 	]+cfcmovge ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4d c2[ 	]+cmovge r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4e c2[ 	]+cfcmovle ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4e c2[ 	]+cmovle r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4e c2[ 	]+cfcmovle ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4e c2[ 	]+cmovle r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4f c2[ 	]+cfcmovg ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4f c2[ 	]+cmovg  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4f c2[ 	]+cfcmovg ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4f c2[ 	]+cmovg  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 42 c2[ 	]+cfcmovb ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 14 42 c2[ 	]+cfcmovb r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 0c 42 94 80 23 01 00 00[ 	]+cfcmovb WORD PTR \[r8\+rax\*4\+0x123\],dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7c 08 42 d1[ 	]+cfcmovb edx,ecx
[ 	]*[a-f0-9]+:[ 	]*62 f4 2c 1c 42 d1[ 	]+cfcmovb r10d,edx,ecx
[ 	]*[a-f0-9]+:[ 	]*62 d4 7c 0c 42 8c 80 23 01 00 00[ 	]+cfcmovb DWORD PTR \[r8\+rax\*4\+0x123\],ecx
[ 	]*[a-f0-9]+:[ 	]*62 5c fc 08 42 ff[ 	]+cfcmovb r15,r31
[ 	]*[a-f0-9]+:[ 	]*62 5c a4 1c 42 ff[ 	]+cfcmovb r11,r15,r31
[ 	]*[a-f0-9]+:[ 	]*62 44 fc 0c 42 bc 80 23 01 00 00[ 	]+cfcmovb QWORD PTR \[r8\+rax\*4\+0x123\],r31
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 08 42 94 80 23 01 00 00[ 	]+cfcmovb dx,WORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 1c 42 94 80 23 01 00 00[ 	]+cfcmovb ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 d4 7c 08 42 8c 80 23 01 00 00[ 	]+cfcmovb ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 d4 6c 1c 42 8c 80 23 01 00 00[ 	]+cfcmovb edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 44 fc 08 42 bc 80 23 01 00 00[ 	]+cfcmovb r31,QWORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 44 84 1c 42 bc 80 23 01 00 00[ 	]+cfcmovb r15,r31,QWORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 42 c2[ 	]+cmovb  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 2c 18 42 d1[ 	]+cmovb  r10d,edx,ecx
[ 	]*[a-f0-9]+:[ 	]*62 5c a4 18 42 ff[ 	]+cmovb  r11,r15,r31
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 18 42 94 80 23 01 00 00[ 	]+cmovb  ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 d4 6c 18 42 8c 80 23 01 00 00[ 	]+cmovb  edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 44 84 18 42 bc 80 23 01 00 00[ 	]+cmovb  r15,r31,QWORD PTR \[r8\+rax\*4\+0x123\]
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 40 c2[ 	]+cfcmovo ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 40 c2[ 	]+cmovo  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 41 c2[ 	]+cfcmovno ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 41 c2[ 	]+cmovno r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 42 c2[ 	]+cfcmovb ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 42 c2[ 	]+cmovb  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 42 c2[ 	]+cfcmovb ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 42 c2[ 	]+cmovb  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 43 c2[ 	]+cfcmovae ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 43 c2[ 	]+cmovae r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 43 c2[ 	]+cfcmovae ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 43 c2[ 	]+cmovae r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 44 c2[ 	]+cfcmove ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 44 c2[ 	]+cmove  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 44 c2[ 	]+cfcmove ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 44 c2[ 	]+cmove  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 45 c2[ 	]+cfcmovne ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 45 c2[ 	]+cmovne r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 45 c2[ 	]+cfcmovne ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 45 c2[ 	]+cmovne r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 46 c2[ 	]+cfcmovbe ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 46 c2[ 	]+cmovbe r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 46 c2[ 	]+cfcmovbe ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 46 c2[ 	]+cmovbe r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 47 c2[ 	]+cfcmova ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 47 c2[ 	]+cmova  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 47 c2[ 	]+cfcmova ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 47 c2[ 	]+cmova  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 48 c2[ 	]+cfcmovs ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 48 c2[ 	]+cmovs  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 49 c2[ 	]+cfcmovns ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 49 c2[ 	]+cmovns r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4a c2[ 	]+cfcmovp ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4a c2[ 	]+cmovp  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4a c2[ 	]+cfcmovp ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4a c2[ 	]+cmovp  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4b c2[ 	]+cfcmovnp ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4b c2[ 	]+cmovnp r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4b c2[ 	]+cfcmovnp ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4b c2[ 	]+cmovnp r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4c c2[ 	]+cfcmovl ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4c c2[ 	]+cmovl  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4c c2[ 	]+cfcmovl ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4c c2[ 	]+cmovl  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4d c2[ 	]+cfcmovge ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4d c2[ 	]+cmovge r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4d c2[ 	]+cfcmovge ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4d c2[ 	]+cmovge r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4e c2[ 	]+cfcmovle ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4e c2[ 	]+cmovle r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4e c2[ 	]+cfcmovle ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4e c2[ 	]+cmovle r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4f c2[ 	]+cfcmovg ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4f c2[ 	]+cmovg  r31w,ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4f c2[ 	]+cfcmovg ax,dx
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4f c2[ 	]+cmovg  r31w,ax,dx
#pass
