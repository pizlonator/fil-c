#as:
#objdump: -dw -Mintel
#name: x86_64 APX_F insns with nf pseudo prefix (Intel disassembly)
#source: x86-64-apx-nf.s

.*: +file format .*

Disassembly of section \.text:

0+ <_start>:
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 c3 7b\s+\{nf\} add bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 c3 7b\s+\{nf\} add dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 c2 7b\s+\{nf\} add dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 c2 7b\s+\{nf\} add ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 c1 7b\s+\{nf\} add ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 c1 7b\s+\{nf\} add edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 c1 7b\s+\{nf\} add r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 83 c1 7b\s+\{nf\} add r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 84 80 23 01 00 00 7b\s+\{nf\} add BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c 80 84 80 23 01 00 00 7b\s+\{nf\} add bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 84 80 23 01 00 00 7b\s+\{nf\} add WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 84 80 23 01 00 00 7b\s+\{nf\} add dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 84 80 23 01 00 00 7b\s+\{nf\} add DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c 83 84 80 23 01 00 00 7b\s+\{nf\} add ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 84 80 23 01 00 00 7b\s+\{nf\} add QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 84 80 23 01 00 00 7b\s+\{nf\} add r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 00 da\s+\{nf\} add dl,bl
\s*[a-f0-9]+:\s*62 f4 3c 1c 00 da\s+\{nf\} add r8b,dl,bl
\s*[a-f0-9]+:\s*62 d4 7c 0c 00 9c 80 23 01 00 00\s+\{nf\} add BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 00 9c 80 23 01 00 00\s+\{nf\} add dl,BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 f4 7d 0c 01 d0\s+\{nf\} add ax,dx
\s*[a-f0-9]+:\s*62 f4 35 1c 01 d0\s+\{nf\} add r9w,ax,dx
\s*[a-f0-9]+:\s*62 d4 7d 0c 01 94 80 23 01 00 00\s+\{nf\} add WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 01 94 80 23 01 00 00\s+\{nf\} add ax,WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 f4 7c 0c 01 ca\s+\{nf\} add edx,ecx
\s*[a-f0-9]+:\s*62 f4 2c 1c 01 ca\s+\{nf\} add r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d4 7c 0c 01 8c 80 23 01 00 00\s+\{nf\} add DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 01 8c 80 23 01 00 00\s+\{nf\} add edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 5c fc 0c 01 cf\s+\{nf\} add r31,r9
\s*[a-f0-9]+:\s*62 5c a4 1c 01 cf\s+\{nf\} add r11,r31,r9
\s*[a-f0-9]+:\s*62 54 fc 0c 01 8c 80 23 01 00 00\s+\{nf\} add QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 54 84 14 01 8c 80 23 01 00 00\s+\{nf\} add r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 d4 7c 0c 02 9c 80 23 01 00 00\s+\{nf\} add bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 02 9c 80 23 01 00 00\s+\{nf\} add dl,bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c 03 94 80 23 01 00 00\s+\{nf\} add dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 1c 03 94 80 23 01 00 00\s+\{nf\} add ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c 03 8c 80 23 01 00 00\s+\{nf\} add ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 03 8c 80 23 01 00 00\s+\{nf\} add edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c 03 8c 80 23 01 00 00\s+\{nf\} add r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 84 14 03 8c 80 23 01 00 00\s+\{nf\} add r31,r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 e3 7b\s+\{nf\} and bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 e3 7b\s+\{nf\} and dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 e2 7b\s+\{nf\} and dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 e2 7b\s+\{nf\} and ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 e1 7b\s+\{nf\} and ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 e1 7b\s+\{nf\} and edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 e1 7b\s+\{nf\} and r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 83 e1 7b\s+\{nf\} and r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 a4 80 23 01 00 00 7b\s+\{nf\} and BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c 80 a4 80 23 01 00 00 7b\s+\{nf\} and bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 a4 80 23 01 00 00 7b\s+\{nf\} and WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 a4 80 23 01 00 00 7b\s+\{nf\} and dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 a4 80 23 01 00 00 7b\s+\{nf\} and DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c 83 a4 80 23 01 00 00 7b\s+\{nf\} and ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 a4 80 23 01 00 00 7b\s+\{nf\} and QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 a4 80 23 01 00 00 7b\s+\{nf\} and r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 20 da\s+\{nf\} and dl,bl
\s*[a-f0-9]+:\s*62 f4 3c 1c 20 da\s+\{nf\} and r8b,dl,bl
\s*[a-f0-9]+:\s*62 d4 7c 0c 20 9c 80 23 01 00 00\s+\{nf\} and BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 20 9c 80 23 01 00 00\s+\{nf\} and dl,BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 f4 7d 0c 21 d0\s+\{nf\} and ax,dx
\s*[a-f0-9]+:\s*62 f4 35 1c 21 d0\s+\{nf\} and r9w,ax,dx
\s*[a-f0-9]+:\s*62 d4 7d 0c 21 94 80 23 01 00 00\s+\{nf\} and WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 21 94 80 23 01 00 00\s+\{nf\} and ax,WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 f4 7c 0c 21 ca\s+\{nf\} and edx,ecx
\s*[a-f0-9]+:\s*62 f4 2c 1c 21 ca\s+\{nf\} and r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d4 7c 0c 21 8c 80 23 01 00 00\s+\{nf\} and DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 21 8c 80 23 01 00 00\s+\{nf\} and edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 5c fc 0c 21 cf\s+\{nf\} and r31,r9
\s*[a-f0-9]+:\s*62 5c a4 1c 21 cf\s+\{nf\} and r11,r31,r9
\s*[a-f0-9]+:\s*62 54 fc 0c 21 8c 80 23 01 00 00\s+\{nf\} and QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 54 84 14 21 8c 80 23 01 00 00\s+\{nf\} and r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 d4 7c 0c 22 9c 80 23 01 00 00\s+\{nf\} and bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 22 9c 80 23 01 00 00\s+\{nf\} and dl,bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c 23 94 80 23 01 00 00\s+\{nf\} and dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 1c 23 94 80 23 01 00 00\s+\{nf\} and ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c 23 8c 80 23 01 00 00\s+\{nf\} and ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 23 8c 80 23 01 00 00\s+\{nf\} and edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c 23 8c 80 23 01 00 00\s+\{nf\} and r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 84 14 23 8c 80 23 01 00 00\s+\{nf\} and r31,r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 72 6c 0c f2 d1\s+\{nf\} andn r10d,edx,ecx
\s*[a-f0-9]+:\s*62 52 84 04 f2 d9\s+\{nf\} andn r11,r31,r9
\s*[a-f0-9]+:\s*62 d2 74 0c f2 94 80 23 01 00 00\s+\{nf\} andn edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 42 b4 0c f2 bc 80 23 01 00 00\s+\{nf\} andn r31,r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 72 74 0c f7 d2\s+\{nf\} bextr r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d2 74 0c f7 94 80 23 01 00 00\s+\{nf\} bextr edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 5a b4 0c f7 df\s+\{nf\} bextr r11,r31,r9
\s*[a-f0-9]+:\s*62 42 b4 0c f7 bc 80 23 01 00 00\s+\{nf\} bextr r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 f2 6c 0c f3 d9\s+\{nf\} blsi edx,ecx
\s*[a-f0-9]+:\s*62 d2 84 04 f3 d9\s+\{nf\} blsi r31,r9
\s*[a-f0-9]+:\s*62 d2 74 0c f3 9c 80 23 01 00 00\s+\{nf\} blsi ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d2 b4 0c f3 9c 80 23 01 00 00\s+\{nf\} blsi r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f2 6c 0c f3 d1\s+\{nf\} blsmsk edx,ecx
\s*[a-f0-9]+:\s*62 d2 84 04 f3 d1\s+\{nf\} blsmsk r31,r9
\s*[a-f0-9]+:\s*62 d2 74 0c f3 94 80 23 01 00 00\s+\{nf\} blsmsk ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d2 b4 0c f3 94 80 23 01 00 00\s+\{nf\} blsmsk r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f2 6c 0c f3 c9\s+\{nf\} blsr edx,ecx
\s*[a-f0-9]+:\s*62 d2 84 04 f3 c9\s+\{nf\} blsr r31,r9
\s*[a-f0-9]+:\s*62 d2 74 0c f3 8c 80 23 01 00 00\s+\{nf\} blsr ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d2 b4 0c f3 8c 80 23 01 00 00\s+\{nf\} blsr r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 72 74 0c f5 d2\s+\{nf\} bzhi r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d2 74 0c f5 94 80 23 01 00 00\s+\{nf\} bzhi edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 5a b4 0c f5 df\s+\{nf\} bzhi r11,r31,r9
\s*[a-f0-9]+:\s*62 42 b4 0c f5 bc 80 23 01 00 00\s+\{nf\} bzhi r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 4c fc 0c 31 ff\s+\{nf\} xor r31,r31
\s*[a-f0-9]+:\s*62 f4 7c 0c fe cb\s+\{nf\} dec bl
\s*[a-f0-9]+:\s*62 f4 6c 1c fe cb\s+\{nf\} dec dl,bl
\s*[a-f0-9]+:\s*62 f4 7d 0c ff ca\s+\{nf\} dec dx
\s*[a-f0-9]+:\s*62 f4 7d 1c ff ca\s+\{nf\} dec ax,dx
\s*[a-f0-9]+:\s*62 f4 7c 0c ff c9\s+\{nf\} dec ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c ff c9\s+\{nf\} dec edx,ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c ff c9\s+\{nf\} dec r9
\s*[a-f0-9]+:\s*62 d4 84 14 ff c9\s+\{nf\} dec r31,r9
\s*[a-f0-9]+:\s*62 d4 7c 0c fe 8c 80 23 01 00 00\s+\{nf\} dec BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 64 1c fe 8c 80 23 01 00 00\s+\{nf\} dec bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c ff 8c 80 23 01 00 00\s+\{nf\} dec WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6d 1c ff 8c 80 23 01 00 00\s+\{nf\} dec dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c ff 8c 80 23 01 00 00\s+\{nf\} dec DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 74 1c ff 8c 80 23 01 00 00\s+\{nf\} dec ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c ff 8c 80 23 01 00 00\s+\{nf\} dec QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 b4 1c ff 8c 80 23 01 00 00\s+\{nf\} dec r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 f3\s+\{nf\} div bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 f2\s+\{nf\} div dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 f1\s+\{nf\} div ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 f1\s+\{nf\} div r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 b4 80 23 01 00 00\s+\{nf\} div BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 b4 80 23 01 00 00\s+\{nf\} div WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 b4 80 23 01 00 00\s+\{nf\} div DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 b4 80 23 01 00 00\s+\{nf\} div QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 fb\s+\{nf\} idiv bl
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 fb\s+\{nf\} idiv bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 fa\s+\{nf\} idiv dx
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 fa\s+\{nf\} idiv dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 f9\s+\{nf\} idiv ecx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 f9\s+\{nf\} idiv ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 f9\s+\{nf\} idiv r9
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 f9\s+\{nf\} idiv r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 bc 80 23 01 00 00\s+\{nf\} idiv BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 bc 80 23 01 00 00\s+\{nf\} idiv BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 bc 80 23 01 00 00\s+\{nf\} idiv WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 bc 80 23 01 00 00\s+\{nf\} idiv WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 bc 80 23 01 00 00\s+\{nf\} idiv DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 bc 80 23 01 00 00\s+\{nf\} idiv DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 bc 80 23 01 00 00\s+\{nf\} idiv QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 bc 80 23 01 00 00\s+\{nf\} idiv QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 eb\s+\{nf\} imul bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 ea\s+\{nf\} imul dx
\s*[a-f0-9]+:\s*62 f4 7d 0c af c2\s+\{nf\} imul ax,dx
\s*[a-f0-9]+:\s*62 f4 35 1c af c2\s+\{nf\} imul r9w,ax,dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 e9\s+\{nf\} imul ecx
\s*[a-f0-9]+:\s*62 f4 7c 0c af d1\s+\{nf\} imul edx,ecx
\s*[a-f0-9]+:\s*62 f4 2c 1c af d1\s+\{nf\} imul r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 e9\s+\{nf\} imul r9
\s*[a-f0-9]+:\s*62 44 fc 0c af f9\s+\{nf\} imul r31,r9
\s*[a-f0-9]+:\s*62 44 a4 1c af f9\s+\{nf\} imul r11,r31,r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 ac 80 23 01 00 00\s+\{nf\} imul BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 ac 80 23 01 00 00\s+\{nf\} imul WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c af 94 80 23 01 00 00\s+\{nf\} imul dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 1c af 94 80 23 01 00 00\s+\{nf\} imul ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 ac 80 23 01 00 00\s+\{nf\} imul DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c af 8c 80 23 01 00 00\s+\{nf\} imul ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c af 8c 80 23 01 00 00\s+\{nf\} imul edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 ac 80 23 01 00 00\s+\{nf\} imul QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c af 8c 80 23 01 00 00\s+\{nf\} imul r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 84 14 af 8c 80 23 01 00 00\s+\{nf\} imul r31,r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7d 0c 6b c2 7b\s+\{nf\} imul ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 6b d1 7b\s+\{nf\} imul edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 54 fc 0c 6b f9 7b\s+\{nf\} imul r15,r9,0x7b
\s*[a-f0-9]+:\s*62 54 fc 0c 6b c9 7b\s+\{nf\} imul r9,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 6b 94 80 23 01 00 00 7b\s+\{nf\} imul dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 6b 8c 80 23 01 00 00 7b\s+\{nf\} imul ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 54 fc 0c 6b 8c 80 23 01 00 00 7b\s+\{nf\} imul r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c 6b c2 90\s+\{nf\} imul ax,dx,0xff90
\s*[a-f0-9]+:\s*62 f4 7c 0c 69 d1 90 ff 00 00\s+\{nf\} imul edx,ecx,0xff90
\s*[a-f0-9]+:\s*62 54 fc 0c 69 f9 90 ff 00 00\s+\{nf\} imul r15,r9,0xff90
\s*[a-f0-9]+:\s*62 54 fc 0c 69 c9 90 ff 00 00\s+\{nf\} imul r9,r9,0xff90
\s*[a-f0-9]+:\s*62 d4 7d 0c 6b 94 80 23 01 00 00 90\s+\{nf\} imul dx,WORD PTR \[r8\+rax\*4\+0x123\],0xff90
\s*[a-f0-9]+:\s*62 d4 7c 0c 69 8c 80 23 01 00 00 90 ff 00 00\s+\{nf\} imul ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0xff90
\s*[a-f0-9]+:\s*62 54 fc 0c 69 8c 80 23 01 00 00 90 ff 00 00\s+\{nf\} imul r9,QWORD PTR \[r8\+rax\*4\+0x123\],0xff90
\s*[a-f0-9]+:\s*62 f4 7c 0c fe c3\s+\{nf\} inc bl
\s*[a-f0-9]+:\s*62 f4 6c 1c fe c3\s+\{nf\} inc dl,bl
\s*[a-f0-9]+:\s*62 f4 7d 0c ff c2\s+\{nf\} inc dx
\s*[a-f0-9]+:\s*62 f4 7d 1c ff c2\s+\{nf\} inc ax,dx
\s*[a-f0-9]+:\s*62 f4 7c 0c ff c1\s+\{nf\} inc ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c ff c1\s+\{nf\} inc edx,ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c ff c1\s+\{nf\} inc r9
\s*[a-f0-9]+:\s*62 d4 84 14 ff c1\s+\{nf\} inc r31,r9
\s*[a-f0-9]+:\s*62 d4 7c 0c fe 84 80 23 01 00 00\s+\{nf\} inc BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 64 1c fe 84 80 23 01 00 00\s+\{nf\} inc bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c ff 84 80 23 01 00 00\s+\{nf\} inc WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6d 1c ff 84 80 23 01 00 00\s+\{nf\} inc dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c ff 84 80 23 01 00 00\s+\{nf\} inc DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 74 1c ff 84 80 23 01 00 00\s+\{nf\} inc ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c ff 84 80 23 01 00 00\s+\{nf\} inc QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 b4 1c ff 84 80 23 01 00 00\s+\{nf\} inc r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7d 0c f5 c2\s+\{nf\} lzcnt ax,dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f5 d1\s+\{nf\} lzcnt edx,ecx
\s*[a-f0-9]+:\s*62 44 fc 0c f5 f9\s+\{nf\} lzcnt r31,r9
\s*[a-f0-9]+:\s*62 d4 7d 0c f5 94 80 23 01 00 00\s+\{nf\} lzcnt dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f5 8c 80 23 01 00 00\s+\{nf\} lzcnt ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c f5 8c 80 23 01 00 00\s+\{nf\} lzcnt r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 e3\s+\{nf\} mul bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 e2\s+\{nf\} mul dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 e1\s+\{nf\} mul ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 e1\s+\{nf\} mul r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 a4 80 23 01 00 00\s+\{nf\} mul BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 a4 80 23 01 00 00\s+\{nf\} mul WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 a4 80 23 01 00 00\s+\{nf\} mul DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 a4 80 23 01 00 00\s+\{nf\} mul QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 db\s+\{nf\} neg bl
\s*[a-f0-9]+:\s*62 f4 6c 1c f6 db\s+\{nf\} neg dl,bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 da\s+\{nf\} neg dx
\s*[a-f0-9]+:\s*62 f4 7d 1c f7 da\s+\{nf\} neg ax,dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 d9\s+\{nf\} neg ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c f7 d9\s+\{nf\} neg edx,ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 d9\s+\{nf\} neg r9
\s*[a-f0-9]+:\s*62 d4 84 14 f7 d9\s+\{nf\} neg r31,r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 9c 80 23 01 00 00\s+\{nf\} neg BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 64 1c f6 9c 80 23 01 00 00\s+\{nf\} neg bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 9c 80 23 01 00 00\s+\{nf\} neg WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6d 1c f7 9c 80 23 01 00 00\s+\{nf\} neg dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 9c 80 23 01 00 00\s+\{nf\} neg DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 74 1c f7 9c 80 23 01 00 00\s+\{nf\} neg ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 9c 80 23 01 00 00\s+\{nf\} neg QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 b4 1c f7 9c 80 23 01 00 00\s+\{nf\} neg r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 cb 7b\s+\{nf\} or bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 cb 7b\s+\{nf\} or dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 ca 7b\s+\{nf\} or dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 ca 7b\s+\{nf\} or ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 c9 7b\s+\{nf\} or ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 c9 7b\s+\{nf\} or edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 c9 7b\s+\{nf\} or r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 83 c9 7b\s+\{nf\} or r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 8c 80 23 01 00 00 7b\s+\{nf\} or BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c 80 8c 80 23 01 00 00 7b\s+\{nf\} or bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 8c 80 23 01 00 00 7b\s+\{nf\} or WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 8c 80 23 01 00 00 7b\s+\{nf\} or dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 8c 80 23 01 00 00 7b\s+\{nf\} or DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c 83 8c 80 23 01 00 00 7b\s+\{nf\} or ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 8c 80 23 01 00 00 7b\s+\{nf\} or QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 8c 80 23 01 00 00 7b\s+\{nf\} or r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 08 da\s+\{nf\} or dl,bl
\s*[a-f0-9]+:\s*62 f4 3c 1c 08 da\s+\{nf\} or r8b,dl,bl
\s*[a-f0-9]+:\s*62 d4 7c 0c 08 9c 80 23 01 00 00\s+\{nf\} or BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 08 9c 80 23 01 00 00\s+\{nf\} or dl,BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 f4 7d 0c 09 d0\s+\{nf\} or ax,dx
\s*[a-f0-9]+:\s*62 f4 35 1c 09 d0\s+\{nf\} or r9w,ax,dx
\s*[a-f0-9]+:\s*62 d4 7d 0c 09 94 80 23 01 00 00\s+\{nf\} or WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 09 94 80 23 01 00 00\s+\{nf\} or ax,WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 f4 7c 0c 09 ca\s+\{nf\} or edx,ecx
\s*[a-f0-9]+:\s*62 f4 2c 1c 09 ca\s+\{nf\} or r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d4 7c 0c 09 8c 80 23 01 00 00\s+\{nf\} or DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 09 8c 80 23 01 00 00\s+\{nf\} or edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 5c fc 0c 09 cf\s+\{nf\} or r31,r9
\s*[a-f0-9]+:\s*62 5c a4 1c 09 cf\s+\{nf\} or r11,r31,r9
\s*[a-f0-9]+:\s*62 54 fc 0c 09 8c 80 23 01 00 00\s+\{nf\} or QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 54 84 14 09 8c 80 23 01 00 00\s+\{nf\} or r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 d4 7c 0c 0a 9c 80 23 01 00 00\s+\{nf\} or bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 0a 9c 80 23 01 00 00\s+\{nf\} or dl,bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c 0b 94 80 23 01 00 00\s+\{nf\} or dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 1c 0b 94 80 23 01 00 00\s+\{nf\} or ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c 0b 8c 80 23 01 00 00\s+\{nf\} or ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 0b 8c 80 23 01 00 00\s+\{nf\} or edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c 0b 8c 80 23 01 00 00\s+\{nf\} or r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 84 14 0b 8c 80 23 01 00 00\s+\{nf\} or r31,r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7d 0c 88 c2\s+\{nf\} popcnt ax,dx
\s*[a-f0-9]+:\s*62 f4 7c 0c 88 d1\s+\{nf\} popcnt edx,ecx
\s*[a-f0-9]+:\s*62 44 fc 0c 88 f9\s+\{nf\} popcnt r31,r9
\s*[a-f0-9]+:\s*62 d4 7d 0c 88 94 80 23 01 00 00\s+\{nf\} popcnt dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c 88 8c 80 23 01 00 00\s+\{nf\} popcnt ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c 88 8c 80 23 01 00 00\s+\{nf\} popcnt r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 c3\s+\{nf\} rol bl,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 c3\s+\{nf\} rol dl,bl,1
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 c2\s+\{nf\} rol dx,1
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 c2\s+\{nf\} rol ax,dx,1
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 c1\s+\{nf\} rol ecx,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 c1\s+\{nf\} rol edx,ecx,1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 c1\s+\{nf\} rol r9,1
\s*[a-f0-9]+:\s*62 d4 84 14 d1 c1\s+\{nf\} rol r31,r9,1
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 84 80 23 01 00 00\s+\{nf\} rol BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 64 1c d0 84 80 23 01 00 00\s+\{nf\} rol bl,BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 84 80 23 01 00 00\s+\{nf\} rol WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 84 80 23 01 00 00\s+\{nf\} rol dx,WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 84 80 23 01 00 00\s+\{nf\} rol DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 74 1c d1 84 80 23 01 00 00\s+\{nf\} rol ecx,DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 84 80 23 01 00 00\s+\{nf\} rol QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 84 80 23 01 00 00\s+\{nf\} rol r9,QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 c3 7b\s+\{nf\} rol bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 c3 7b\s+\{nf\} rol dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 c2 7b\s+\{nf\} rol dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 c2 7b\s+\{nf\} rol ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 c1 7b\s+\{nf\} rol ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 c1 7b\s+\{nf\} rol edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 c1 7b\s+\{nf\} rol r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 c1 c1 7b\s+\{nf\} rol r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 84 80 23 01 00 00 7b\s+\{nf\} rol BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c c0 84 80 23 01 00 00 7b\s+\{nf\} rol bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 84 80 23 01 00 00 7b\s+\{nf\} rol WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 84 80 23 01 00 00 7b\s+\{nf\} rol dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 84 80 23 01 00 00 7b\s+\{nf\} rol DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c c1 84 80 23 01 00 00 7b\s+\{nf\} rol ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 84 80 23 01 00 00 7b\s+\{nf\} rol QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 84 80 23 01 00 00 7b\s+\{nf\} rol r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 c3\s+\{nf\} rol bl,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 c3\s+\{nf\} rol dl,bl,cl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 c2\s+\{nf\} rol dx,cl
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 c2\s+\{nf\} rol ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 c1\s+\{nf\} rol ecx,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 c1\s+\{nf\} rol edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 c1\s+\{nf\} rol r9,cl
\s*[a-f0-9]+:\s*62 d4 84 14 d3 c1\s+\{nf\} rol r31,r9,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 84 80 23 01 00 00\s+\{nf\} rol BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 64 1c d2 84 80 23 01 00 00\s+\{nf\} rol bl,BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 84 80 23 01 00 00\s+\{nf\} rol WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 84 80 23 01 00 00\s+\{nf\} rol dx,WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 84 80 23 01 00 00\s+\{nf\} rol DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 74 1c d3 84 80 23 01 00 00\s+\{nf\} rol ecx,DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 84 80 23 01 00 00\s+\{nf\} rol QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 84 80 23 01 00 00\s+\{nf\} rol r9,QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 cb\s+\{nf\} ror bl,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 cb\s+\{nf\} ror dl,bl,1
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 ca\s+\{nf\} ror dx,1
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 ca\s+\{nf\} ror ax,dx,1
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 c9\s+\{nf\} ror ecx,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 c9\s+\{nf\} ror edx,ecx,1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 c9\s+\{nf\} ror r9,1
\s*[a-f0-9]+:\s*62 d4 84 14 d1 c9\s+\{nf\} ror r31,r9,1
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 8c 80 23 01 00 00\s+\{nf\} ror BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 64 1c d0 8c 80 23 01 00 00\s+\{nf\} ror bl,BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 8c 80 23 01 00 00\s+\{nf\} ror WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 8c 80 23 01 00 00\s+\{nf\} ror dx,WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 8c 80 23 01 00 00\s+\{nf\} ror DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 74 1c d1 8c 80 23 01 00 00\s+\{nf\} ror ecx,DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 8c 80 23 01 00 00\s+\{nf\} ror QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 8c 80 23 01 00 00\s+\{nf\} ror r9,QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 cb 7b\s+\{nf\} ror bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 cb 7b\s+\{nf\} ror dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 ca 7b\s+\{nf\} ror dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 ca 7b\s+\{nf\} ror ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 c9 7b\s+\{nf\} ror ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 c9 7b\s+\{nf\} ror edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 c9 7b\s+\{nf\} ror r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 c1 c9 7b\s+\{nf\} ror r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 8c 80 23 01 00 00 7b\s+\{nf\} ror BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c c0 8c 80 23 01 00 00 7b\s+\{nf\} ror bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 cb\s+\{nf\} ror bl,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 cb\s+\{nf\} ror dl,bl,cl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 ca\s+\{nf\} ror dx,cl
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 ca\s+\{nf\} ror ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 c9\s+\{nf\} ror ecx,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 c9\s+\{nf\} ror edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 c9\s+\{nf\} ror r9,cl
\s*[a-f0-9]+:\s*62 d4 84 14 d3 c9\s+\{nf\} ror r31,r9,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 8c 80 23 01 00 00\s+\{nf\} ror BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 64 1c d2 8c 80 23 01 00 00\s+\{nf\} ror bl,BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 8c 80 23 01 00 00\s+\{nf\} ror WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 8c 80 23 01 00 00\s+\{nf\} ror dx,WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 8c 80 23 01 00 00\s+\{nf\} ror DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 74 1c d3 8c 80 23 01 00 00\s+\{nf\} ror ecx,DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 8c 80 23 01 00 00\s+\{nf\} ror QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 8c 80 23 01 00 00\s+\{nf\} ror r9,QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 e3\s+\{nf\} shl bl,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 e3\s+\{nf\} shl dl,bl,1
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 e2\s+\{nf\} shl dx,1
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 e2\s+\{nf\} shl ax,dx,1
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 e1\s+\{nf\} shl ecx,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 e1\s+\{nf\} shl edx,ecx,1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 e1\s+\{nf\} shl r9,1
\s*[a-f0-9]+:\s*62 d4 84 14 d1 e1\s+\{nf\} shl r31,r9,1
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 a4 80 23 01 00 00\s+\{nf\} shl BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 64 1c d0 a4 80 23 01 00 00\s+\{nf\} shl bl,BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 a4 80 23 01 00 00\s+\{nf\} shl WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 a4 80 23 01 00 00\s+\{nf\} shl dx,WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 a4 80 23 01 00 00\s+\{nf\} shl DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 74 1c d1 a4 80 23 01 00 00\s+\{nf\} shl ecx,DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 a4 80 23 01 00 00\s+\{nf\} shl QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 a4 80 23 01 00 00\s+\{nf\} shl r9,QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 e3 7b\s+\{nf\} shl bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 e3 7b\s+\{nf\} shl dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 e2 7b\s+\{nf\} shl dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 e2 7b\s+\{nf\} shl ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 e1 7b\s+\{nf\} shl ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 e1 7b\s+\{nf\} shl edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 e1 7b\s+\{nf\} shl r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 c1 e1 7b\s+\{nf\} shl r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 a4 80 23 01 00 00 7b\s+\{nf\} shl BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c c0 a4 80 23 01 00 00 7b\s+\{nf\} shl bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 e3\s+\{nf\} shl bl,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 e3\s+\{nf\} shl dl,bl,cl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 e2\s+\{nf\} shl dx,cl
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 e2\s+\{nf\} shl ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 e1\s+\{nf\} shl ecx,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 e1\s+\{nf\} shl edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 e1\s+\{nf\} shl r9,cl
\s*[a-f0-9]+:\s*62 d4 84 14 d3 e1\s+\{nf\} shl r31,r9,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 a4 80 23 01 00 00\s+\{nf\} shl BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 64 1c d2 a4 80 23 01 00 00\s+\{nf\} shl bl,BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 a4 80 23 01 00 00\s+\{nf\} shl WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 a4 80 23 01 00 00\s+\{nf\} shl dx,WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 a4 80 23 01 00 00\s+\{nf\} shl DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 74 1c d3 a4 80 23 01 00 00\s+\{nf\} shl ecx,DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 a4 80 23 01 00 00\s+\{nf\} shl QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 a4 80 23 01 00 00\s+\{nf\} shl r9,QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 fb\s+\{nf\} sar bl,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 fb\s+\{nf\} sar dl,bl,1
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 fa\s+\{nf\} sar dx,1
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 fa\s+\{nf\} sar ax,dx,1
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 f9\s+\{nf\} sar ecx,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 f9\s+\{nf\} sar edx,ecx,1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 f9\s+\{nf\} sar r9,1
\s*[a-f0-9]+:\s*62 d4 84 14 d1 f9\s+\{nf\} sar r31,r9,1
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 bc 80 23 01 00 00\s+\{nf\} sar BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 64 1c d0 bc 80 23 01 00 00\s+\{nf\} sar bl,BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 bc 80 23 01 00 00\s+\{nf\} sar WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 bc 80 23 01 00 00\s+\{nf\} sar dx,WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 bc 80 23 01 00 00\s+\{nf\} sar DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 74 1c d1 bc 80 23 01 00 00\s+\{nf\} sar ecx,DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 bc 80 23 01 00 00\s+\{nf\} sar QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 bc 80 23 01 00 00\s+\{nf\} sar r9,QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 fb 7b\s+\{nf\} sar bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 fb 7b\s+\{nf\} sar dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 fa 7b\s+\{nf\} sar dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 fa 7b\s+\{nf\} sar ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 f9 7b\s+\{nf\} sar ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 f9 7b\s+\{nf\} sar edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 f9 7b\s+\{nf\} sar r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 c1 f9 7b\s+\{nf\} sar r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 bc 80 23 01 00 00 7b\s+\{nf\} sar BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c c0 bc 80 23 01 00 00 7b\s+\{nf\} sar bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 fb\s+\{nf\} sar bl,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 fb\s+\{nf\} sar dl,bl,cl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 fa\s+\{nf\} sar dx,cl
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 fa\s+\{nf\} sar ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 f9\s+\{nf\} sar ecx,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 f9\s+\{nf\} sar edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 f9\s+\{nf\} sar r9,cl
\s*[a-f0-9]+:\s*62 d4 84 14 d3 f9\s+\{nf\} sar r31,r9,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 bc 80 23 01 00 00\s+\{nf\} sar BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 64 1c d2 bc 80 23 01 00 00\s+\{nf\} sar bl,BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 bc 80 23 01 00 00\s+\{nf\} sar WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 bc 80 23 01 00 00\s+\{nf\} sar dx,WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 bc 80 23 01 00 00\s+\{nf\} sar DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 74 1c d3 bc 80 23 01 00 00\s+\{nf\} sar ecx,DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 bc 80 23 01 00 00\s+\{nf\} sar QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 bc 80 23 01 00 00\s+\{nf\} sar r9,QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 e3\s+\{nf\} shl bl,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 e3\s+\{nf\} shl dl,bl,1
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 e2\s+\{nf\} shl dx,1
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 e2\s+\{nf\} shl ax,dx,1
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 e1\s+\{nf\} shl ecx,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 e1\s+\{nf\} shl edx,ecx,1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 e1\s+\{nf\} shl r9,1
\s*[a-f0-9]+:\s*62 d4 84 14 d1 e1\s+\{nf\} shl r31,r9,1
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 a4 80 23 01 00 00\s+\{nf\} shl BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 64 1c d0 a4 80 23 01 00 00\s+\{nf\} shl bl,BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 a4 80 23 01 00 00\s+\{nf\} shl WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 a4 80 23 01 00 00\s+\{nf\} shl dx,WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 a4 80 23 01 00 00\s+\{nf\} shl DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 74 1c d1 a4 80 23 01 00 00\s+\{nf\} shl ecx,DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 a4 80 23 01 00 00\s+\{nf\} shl QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 a4 80 23 01 00 00\s+\{nf\} shl r9,QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 e3 7b\s+\{nf\} shl bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 e3 7b\s+\{nf\} shl dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 e2 7b\s+\{nf\} shl dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 e2 7b\s+\{nf\} shl ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 e1 7b\s+\{nf\} shl ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 e1 7b\s+\{nf\} shl edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 e1 7b\s+\{nf\} shl r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 c1 e1 7b\s+\{nf\} shl r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 a4 80 23 01 00 00 7b\s+\{nf\} shl BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c c0 a4 80 23 01 00 00 7b\s+\{nf\} shl bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 e3\s+\{nf\} shl bl,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 e3\s+\{nf\} shl dl,bl,cl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 e2\s+\{nf\} shl dx,cl
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 e2\s+\{nf\} shl ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 e1\s+\{nf\} shl ecx,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 e1\s+\{nf\} shl edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 e1\s+\{nf\} shl r9,cl
\s*[a-f0-9]+:\s*62 d4 84 14 d3 e1\s+\{nf\} shl r31,r9,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 a4 80 23 01 00 00\s+\{nf\} shl BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 64 1c d2 a4 80 23 01 00 00\s+\{nf\} shl bl,BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 a4 80 23 01 00 00\s+\{nf\} shl WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 a4 80 23 01 00 00\s+\{nf\} shl dx,WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 a4 80 23 01 00 00\s+\{nf\} shl DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 74 1c d3 a4 80 23 01 00 00\s+\{nf\} shl ecx,DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 a4 80 23 01 00 00\s+\{nf\} shl QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 a4 80 23 01 00 00\s+\{nf\} shl r9,QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 f4 7d 0c 24 d0 7b\s+\{nf\} shld ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 35 1c 24 d0 7b\s+\{nf\} shld r9w,ax,dx,0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 24 94 80 23 01 00 00 7b\s+\{nf\} shld WORD PTR \[r8\+rax\*4\+0x123\],dx,0x7b
\s*[a-f0-9]+:\s*62 d4 7d 1c 24 94 80 23 01 00 00 7b\s+\{nf\} shld ax,WORD PTR \[r8\+rax\*4\+0x123\],dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 24 ca 7b\s+\{nf\} shld edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 2c 1c 24 ca 7b\s+\{nf\} shld r10d,edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 24 8c 80 23 01 00 00 7b\s+\{nf\} shld DWORD PTR \[r8\+rax\*4\+0x123\],ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 6c 1c 24 8c 80 23 01 00 00 7b\s+\{nf\} shld edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx,0x7b
\s*[a-f0-9]+:\s*62 5c fc 0c 24 cf 7b\s+\{nf\} shld r31,r9,0x7b
\s*[a-f0-9]+:\s*62 5c a4 1c 24 cf 7b\s+\{nf\} shld r11,r31,r9,0x7b
\s*[a-f0-9]+:\s*62 54 fc 0c 24 8c 80 23 01 00 00 7b\s+\{nf\} shld QWORD PTR \[r8\+rax\*4\+0x123\],r9,0x7b
\s*[a-f0-9]+:\s*62 54 84 14 24 8c 80 23 01 00 00 7b\s+\{nf\} shld r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c a5 d0\s+\{nf\} shld ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 35 1c a5 d0\s+\{nf\} shld r9w,ax,dx,cl
\s*[a-f0-9]+:\s*62 d4 7d 0c a5 94 80 23 01 00 00\s+\{nf\} shld WORD PTR \[r8\+rax\*4\+0x123\],dx,cl
\s*[a-f0-9]+:\s*62 d4 7d 1c a5 94 80 23 01 00 00\s+\{nf\} shld ax,WORD PTR \[r8\+rax\*4\+0x123\],dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c a5 ca\s+\{nf\} shld edx,ecx,cl
\s*[a-f0-9]+:\s*62 f4 2c 1c a5 ca\s+\{nf\} shld r10d,edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c a5 8c 80 23 01 00 00\s+\{nf\} shld DWORD PTR \[r8\+rax\*4\+0x123\],ecx,cl
\s*[a-f0-9]+:\s*62 d4 6c 1c a5 8c 80 23 01 00 00\s+\{nf\} shld edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx,cl
\s*[a-f0-9]+:\s*62 5c fc 0c a5 cf\s+\{nf\} shld r31,r9,cl
\s*[a-f0-9]+:\s*62 5c a4 1c a5 cf\s+\{nf\} shld r11,r31,r9,cl
\s*[a-f0-9]+:\s*62 54 fc 0c a5 8c 80 23 01 00 00\s+\{nf\} shld QWORD PTR \[r8\+rax\*4\+0x123\],r9,cl
\s*[a-f0-9]+:\s*62 54 84 14 a5 8c 80 23 01 00 00\s+\{nf\} shld r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 eb\s+\{nf\} shr bl,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 eb\s+\{nf\} shr dl,bl,1
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 ea\s+\{nf\} shr dx,1
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 ea\s+\{nf\} shr ax,dx,1
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 e9\s+\{nf\} shr ecx,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 e9\s+\{nf\} shr edx,ecx,1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 e9\s+\{nf\} shr r9,1
\s*[a-f0-9]+:\s*62 d4 84 14 d1 e9\s+\{nf\} shr r31,r9,1
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 ac 80 23 01 00 00\s+\{nf\} shr BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 64 1c d0 ac 80 23 01 00 00\s+\{nf\} shr bl,BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 ac 80 23 01 00 00\s+\{nf\} shr WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 ac 80 23 01 00 00\s+\{nf\} shr dx,WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 ac 80 23 01 00 00\s+\{nf\} shr DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 74 1c d1 ac 80 23 01 00 00\s+\{nf\} shr ecx,DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 ac 80 23 01 00 00\s+\{nf\} shr QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 ac 80 23 01 00 00\s+\{nf\} shr r9,QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 eb 7b\s+\{nf\} shr bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 eb 7b\s+\{nf\} shr dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 ea 7b\s+\{nf\} shr dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 ea 7b\s+\{nf\} shr ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 e9 7b\s+\{nf\} shr ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 e9 7b\s+\{nf\} shr edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 e9 7b\s+\{nf\} shr r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 c1 e9 7b\s+\{nf\} shr r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 ac 80 23 01 00 00 7b\s+\{nf\} shr BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c c0 ac 80 23 01 00 00 7b\s+\{nf\} shr bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 eb\s+\{nf\} shr bl,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 eb\s+\{nf\} shr dl,bl,cl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 ea\s+\{nf\} shr dx,cl
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 ea\s+\{nf\} shr ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 e9\s+\{nf\} shr ecx,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 e9\s+\{nf\} shr edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 e9\s+\{nf\} shr r9,cl
\s*[a-f0-9]+:\s*62 d4 84 14 d3 e9\s+\{nf\} shr r31,r9,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 ac 80 23 01 00 00\s+\{nf\} shr BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 64 1c d2 ac 80 23 01 00 00\s+\{nf\} shr bl,BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 ac 80 23 01 00 00\s+\{nf\} shr WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 ac 80 23 01 00 00\s+\{nf\} shr dx,WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 ac 80 23 01 00 00\s+\{nf\} shr DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 74 1c d3 ac 80 23 01 00 00\s+\{nf\} shr ecx,DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 ac 80 23 01 00 00\s+\{nf\} shr QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 ac 80 23 01 00 00\s+\{nf\} shr r9,QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 f4 7d 0c 2c d0 7b\s+\{nf\} shrd ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 35 1c 2c d0 7b\s+\{nf\} shrd r9w,ax,dx,0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 2c 94 80 23 01 00 00 7b\s+\{nf\} shrd WORD PTR \[r8\+rax\*4\+0x123\],dx,0x7b
\s*[a-f0-9]+:\s*62 d4 7d 1c 2c 94 80 23 01 00 00 7b\s+\{nf\} shrd ax,WORD PTR \[r8\+rax\*4\+0x123\],dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 2c ca 7b\s+\{nf\} shrd edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 2c 1c 2c ca 7b\s+\{nf\} shrd r10d,edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd DWORD PTR \[r8\+rax\*4\+0x123\],ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 6c 1c 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx,0x7b
\s*[a-f0-9]+:\s*62 5c fc 0c 2c cf 7b\s+\{nf\} shrd r31,r9,0x7b
\s*[a-f0-9]+:\s*62 5c a4 1c 2c cf 7b\s+\{nf\} shrd r11,r31,r9,0x7b
\s*[a-f0-9]+:\s*62 54 fc 0c 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd QWORD PTR \[r8\+rax\*4\+0x123\],r9,0x7b
\s*[a-f0-9]+:\s*62 54 84 14 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c ad d0\s+\{nf\} shrd ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 35 1c ad d0\s+\{nf\} shrd r9w,ax,dx,cl
\s*[a-f0-9]+:\s*62 d4 7d 0c ad 94 80 23 01 00 00\s+\{nf\} shrd WORD PTR \[r8\+rax\*4\+0x123\],dx,cl
\s*[a-f0-9]+:\s*62 d4 7d 1c ad 94 80 23 01 00 00\s+\{nf\} shrd ax,WORD PTR \[r8\+rax\*4\+0x123\],dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c ad ca\s+\{nf\} shrd edx,ecx,cl
\s*[a-f0-9]+:\s*62 f4 2c 1c ad ca\s+\{nf\} shrd r10d,edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c ad 8c 80 23 01 00 00\s+\{nf\} shrd DWORD PTR \[r8\+rax\*4\+0x123\],ecx,cl
\s*[a-f0-9]+:\s*62 d4 6c 1c ad 8c 80 23 01 00 00\s+\{nf\} shrd edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx,cl
\s*[a-f0-9]+:\s*62 5c fc 0c ad cf\s+\{nf\} shrd r31,r9,cl
\s*[a-f0-9]+:\s*62 5c a4 1c ad cf\s+\{nf\} shrd r11,r31,r9,cl
\s*[a-f0-9]+:\s*62 54 fc 0c ad 8c 80 23 01 00 00\s+\{nf\} shrd QWORD PTR \[r8\+rax\*4\+0x123\],r9,cl
\s*[a-f0-9]+:\s*62 54 84 14 ad 8c 80 23 01 00 00\s+\{nf\} shrd r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 eb 7b\s+\{nf\} sub bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 eb 7b\s+\{nf\} sub dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 ea 7b\s+\{nf\} sub dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 ea 7b\s+\{nf\} sub ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 e9 7b\s+\{nf\} sub ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 e9 7b\s+\{nf\} sub edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 e9 7b\s+\{nf\} sub r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 83 e9 7b\s+\{nf\} sub r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 ac 80 23 01 00 00 7b\s+\{nf\} sub BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c 80 ac 80 23 01 00 00 7b\s+\{nf\} sub bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 28 da\s+\{nf\} sub dl,bl
\s*[a-f0-9]+:\s*62 f4 3c 1c 28 da\s+\{nf\} sub r8b,dl,bl
\s*[a-f0-9]+:\s*62 d4 7c 0c 28 9c 80 23 01 00 00\s+\{nf\} sub BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 28 9c 80 23 01 00 00\s+\{nf\} sub dl,BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 f4 7d 0c 29 d0\s+\{nf\} sub ax,dx
\s*[a-f0-9]+:\s*62 f4 35 1c 29 d0\s+\{nf\} sub r9w,ax,dx
\s*[a-f0-9]+:\s*62 d4 7d 0c 29 94 80 23 01 00 00\s+\{nf\} sub WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 29 94 80 23 01 00 00\s+\{nf\} sub ax,WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 f4 7c 0c 29 ca\s+\{nf\} sub edx,ecx
\s*[a-f0-9]+:\s*62 f4 2c 1c 29 ca\s+\{nf\} sub r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d4 7c 0c 29 8c 80 23 01 00 00\s+\{nf\} sub DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 29 8c 80 23 01 00 00\s+\{nf\} sub edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 5c fc 0c 29 cf\s+\{nf\} sub r31,r9
\s*[a-f0-9]+:\s*62 5c a4 1c 29 cf\s+\{nf\} sub r11,r31,r9
\s*[a-f0-9]+:\s*62 54 fc 0c 29 8c 80 23 01 00 00\s+\{nf\} sub QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 54 84 14 29 8c 80 23 01 00 00\s+\{nf\} sub r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 d4 7c 0c 2a 9c 80 23 01 00 00\s+\{nf\} sub bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 2a 9c 80 23 01 00 00\s+\{nf\} sub dl,bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c 2b 94 80 23 01 00 00\s+\{nf\} sub dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 1c 2b 94 80 23 01 00 00\s+\{nf\} sub ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c 2b 8c 80 23 01 00 00\s+\{nf\} sub ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 2b 8c 80 23 01 00 00\s+\{nf\} sub edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c 2b 8c 80 23 01 00 00\s+\{nf\} sub r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 84 14 2b 8c 80 23 01 00 00\s+\{nf\} sub r31,r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7d 0c f4 c2\s+\{nf\} tzcnt ax,dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f4 d1\s+\{nf\} tzcnt edx,ecx
\s*[a-f0-9]+:\s*62 44 fc 0c f4 f9\s+\{nf\} tzcnt r31,r9
\s*[a-f0-9]+:\s*62 d4 7d 0c f4 94 80 23 01 00 00\s+\{nf\} tzcnt dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f4 8c 80 23 01 00 00\s+\{nf\} tzcnt ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c f4 8c 80 23 01 00 00\s+\{nf\} tzcnt r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 f3 7b\s+\{nf\} xor bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 f3 7b\s+\{nf\} xor dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 f2 7b\s+\{nf\} xor dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 f2 7b\s+\{nf\} xor ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 f1 7b\s+\{nf\} xor ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 f1 7b\s+\{nf\} xor edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 f1 7b\s+\{nf\} xor r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 83 f1 7b\s+\{nf\} xor r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 b4 80 23 01 00 00 7b\s+\{nf\} xor BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c 80 b4 80 23 01 00 00 7b\s+\{nf\} xor bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 30 da\s+\{nf\} xor dl,bl
\s*[a-f0-9]+:\s*62 f4 3c 1c 30 da\s+\{nf\} xor r8b,dl,bl
\s*[a-f0-9]+:\s*62 d4 7c 0c 30 9c 80 23 01 00 00\s+\{nf\} xor BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 30 9c 80 23 01 00 00\s+\{nf\} xor dl,BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 f4 7d 0c 31 d0\s+\{nf\} xor ax,dx
\s*[a-f0-9]+:\s*62 f4 35 1c 31 d0\s+\{nf\} xor r9w,ax,dx
\s*[a-f0-9]+:\s*62 d4 7d 0c 31 94 80 23 01 00 00\s+\{nf\} xor WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 31 94 80 23 01 00 00\s+\{nf\} xor ax,WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 f4 7c 0c 31 ca\s+\{nf\} xor edx,ecx
\s*[a-f0-9]+:\s*62 f4 2c 1c 31 ca\s+\{nf\} xor r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d4 7c 0c 31 8c 80 23 01 00 00\s+\{nf\} xor DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 31 8c 80 23 01 00 00\s+\{nf\} xor edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 5c fc 0c 31 cf\s+\{nf\} xor r31,r9
\s*[a-f0-9]+:\s*62 5c a4 1c 31 cf\s+\{nf\} xor r11,r31,r9
\s*[a-f0-9]+:\s*62 54 fc 0c 31 8c 80 23 01 00 00\s+\{nf\} xor QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 54 84 14 31 8c 80 23 01 00 00\s+\{nf\} xor r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 d4 7c 0c 32 9c 80 23 01 00 00\s+\{nf\} xor bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 32 9c 80 23 01 00 00\s+\{nf\} xor dl,bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c 33 94 80 23 01 00 00\s+\{nf\} xor dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 1c 33 94 80 23 01 00 00\s+\{nf\} xor ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c 33 8c 80 23 01 00 00\s+\{nf\} xor ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 33 8c 80 23 01 00 00\s+\{nf\} xor edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c 33 8c 80 23 01 00 00\s+\{nf\} xor r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 84 14 33 8c 80 23 01 00 00\s+\{nf\} xor r31,r9,QWORD PTR \[r8\+rax\*4\+0x123\]

0[0-9a-f]+ <intel>:
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 c3 7b\s+\{nf\} add bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 c3 7b\s+\{nf\} add dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 c2 7b\s+\{nf\} add dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 c2 7b\s+\{nf\} add ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 c1 7b\s+\{nf\} add ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 c1 7b\s+\{nf\} add edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 c1 7b\s+\{nf\} add r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 83 c1 7b\s+\{nf\} add r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 84 80 23 01 00 00 7b\s+\{nf\} add BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c 80 84 80 23 01 00 00 7b\s+\{nf\} add bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 84 80 23 01 00 00 7b\s+\{nf\} add WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 84 80 23 01 00 00 7b\s+\{nf\} add dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 84 80 23 01 00 00 7b\s+\{nf\} add DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c 83 84 80 23 01 00 00 7b\s+\{nf\} add ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 84 80 23 01 00 00 7b\s+\{nf\} add QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 84 80 23 01 00 00 7b\s+\{nf\} add r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 00 da\s+\{nf\} add dl,bl
\s*[a-f0-9]+:\s*62 f4 3c 1c 00 da\s+\{nf\} add r8b,dl,bl
\s*[a-f0-9]+:\s*62 d4 7c 0c 00 9c 80 23 01 00 00\s+\{nf\} add BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 00 9c 80 23 01 00 00\s+\{nf\} add dl,BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 f4 7d 0c 01 d0\s+\{nf\} add ax,dx
\s*[a-f0-9]+:\s*62 f4 35 1c 01 d0\s+\{nf\} add r9w,ax,dx
\s*[a-f0-9]+:\s*62 d4 7d 0c 01 94 80 23 01 00 00\s+\{nf\} add WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 01 94 80 23 01 00 00\s+\{nf\} add ax,WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 f4 7c 0c 01 ca\s+\{nf\} add edx,ecx
\s*[a-f0-9]+:\s*62 f4 2c 1c 01 ca\s+\{nf\} add r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d4 7c 0c 01 8c 80 23 01 00 00\s+\{nf\} add DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 01 8c 80 23 01 00 00\s+\{nf\} add edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 5c fc 0c 01 cf\s+\{nf\} add r31,r9
\s*[a-f0-9]+:\s*62 5c a4 1c 01 cf\s+\{nf\} add r11,r31,r9
\s*[a-f0-9]+:\s*62 54 fc 0c 01 8c 80 23 01 00 00\s+\{nf\} add QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 54 84 14 01 8c 80 23 01 00 00\s+\{nf\} add r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 d4 7c 0c 02 9c 80 23 01 00 00\s+\{nf\} add bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 02 9c 80 23 01 00 00\s+\{nf\} add dl,bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c 03 94 80 23 01 00 00\s+\{nf\} add dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 1c 03 94 80 23 01 00 00\s+\{nf\} add ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c 03 8c 80 23 01 00 00\s+\{nf\} add ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 03 8c 80 23 01 00 00\s+\{nf\} add edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c 03 8c 80 23 01 00 00\s+\{nf\} add r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 84 14 03 8c 80 23 01 00 00\s+\{nf\} add r31,r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 e3 7b\s+\{nf\} and bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 e3 7b\s+\{nf\} and dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 e2 7b\s+\{nf\} and dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 e2 7b\s+\{nf\} and ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 e1 7b\s+\{nf\} and ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 e1 7b\s+\{nf\} and edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 e1 7b\s+\{nf\} and r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 83 e1 7b\s+\{nf\} and r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 a4 80 23 01 00 00 7b\s+\{nf\} and BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c 80 a4 80 23 01 00 00 7b\s+\{nf\} and bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 a4 80 23 01 00 00 7b\s+\{nf\} and WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 a4 80 23 01 00 00 7b\s+\{nf\} and dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 a4 80 23 01 00 00 7b\s+\{nf\} and DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c 83 a4 80 23 01 00 00 7b\s+\{nf\} and ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 a4 80 23 01 00 00 7b\s+\{nf\} and QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 a4 80 23 01 00 00 7b\s+\{nf\} and r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 20 da\s+\{nf\} and dl,bl
\s*[a-f0-9]+:\s*62 f4 3c 1c 20 da\s+\{nf\} and r8b,dl,bl
\s*[a-f0-9]+:\s*62 d4 7c 0c 20 9c 80 23 01 00 00\s+\{nf\} and BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 20 9c 80 23 01 00 00\s+\{nf\} and dl,BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 f4 7d 0c 21 d0\s+\{nf\} and ax,dx
\s*[a-f0-9]+:\s*62 f4 35 1c 21 d0\s+\{nf\} and r9w,ax,dx
\s*[a-f0-9]+:\s*62 d4 7d 0c 21 94 80 23 01 00 00\s+\{nf\} and WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 21 94 80 23 01 00 00\s+\{nf\} and ax,WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 f4 7c 0c 21 ca\s+\{nf\} and edx,ecx
\s*[a-f0-9]+:\s*62 f4 2c 1c 21 ca\s+\{nf\} and r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d4 7c 0c 21 8c 80 23 01 00 00\s+\{nf\} and DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 21 8c 80 23 01 00 00\s+\{nf\} and edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 5c fc 0c 21 cf\s+\{nf\} and r31,r9
\s*[a-f0-9]+:\s*62 5c a4 1c 21 cf\s+\{nf\} and r11,r31,r9
\s*[a-f0-9]+:\s*62 54 fc 0c 21 8c 80 23 01 00 00\s+\{nf\} and QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 54 84 14 21 8c 80 23 01 00 00\s+\{nf\} and r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 d4 7c 0c 22 9c 80 23 01 00 00\s+\{nf\} and bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 22 9c 80 23 01 00 00\s+\{nf\} and dl,bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c 23 94 80 23 01 00 00\s+\{nf\} and dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 1c 23 94 80 23 01 00 00\s+\{nf\} and ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c 23 8c 80 23 01 00 00\s+\{nf\} and ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 23 8c 80 23 01 00 00\s+\{nf\} and edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c 23 8c 80 23 01 00 00\s+\{nf\} and r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 84 14 23 8c 80 23 01 00 00\s+\{nf\} and r31,r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 72 6c 0c f2 d1\s+\{nf\} andn r10d,edx,ecx
\s*[a-f0-9]+:\s*62 52 84 04 f2 d9\s+\{nf\} andn r11,r31,r9
\s*[a-f0-9]+:\s*62 d2 74 0c f2 94 80 23 01 00 00\s+\{nf\} andn edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 42 b4 0c f2 bc 80 23 01 00 00\s+\{nf\} andn r31,r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 72 74 0c f7 d2\s+\{nf\} bextr r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d2 74 0c f7 94 80 23 01 00 00\s+\{nf\} bextr edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 5a b4 0c f7 df\s+\{nf\} bextr r11,r31,r9
\s*[a-f0-9]+:\s*62 42 b4 0c f7 bc 80 23 01 00 00\s+\{nf\} bextr r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 f2 6c 0c f3 d9\s+\{nf\} blsi edx,ecx
\s*[a-f0-9]+:\s*62 d2 84 04 f3 d9\s+\{nf\} blsi r31,r9
\s*[a-f0-9]+:\s*62 d2 74 0c f3 9c 80 23 01 00 00\s+\{nf\} blsi ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d2 b4 0c f3 9c 80 23 01 00 00\s+\{nf\} blsi r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f2 6c 0c f3 d1\s+\{nf\} blsmsk edx,ecx
\s*[a-f0-9]+:\s*62 d2 84 04 f3 d1\s+\{nf\} blsmsk r31,r9
\s*[a-f0-9]+:\s*62 d2 74 0c f3 94 80 23 01 00 00\s+\{nf\} blsmsk ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d2 b4 0c f3 94 80 23 01 00 00\s+\{nf\} blsmsk r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f2 6c 0c f3 c9\s+\{nf\} blsr edx,ecx
\s*[a-f0-9]+:\s*62 d2 84 04 f3 c9\s+\{nf\} blsr r31,r9
\s*[a-f0-9]+:\s*62 d2 74 0c f3 8c 80 23 01 00 00\s+\{nf\} blsr ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d2 b4 0c f3 8c 80 23 01 00 00\s+\{nf\} blsr r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 72 74 0c f5 d2\s+\{nf\} bzhi r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d2 74 0c f5 94 80 23 01 00 00\s+\{nf\} bzhi edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 5a b4 0c f5 df\s+\{nf\} bzhi r11,r31,r9
\s*[a-f0-9]+:\s*62 42 b4 0c f5 bc 80 23 01 00 00\s+\{nf\} bzhi r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 4c fc 0c 31 ff\s+\{nf\} xor r31,r31
\s*[a-f0-9]+:\s*62 f4 7c 0c fe cb\s+\{nf\} dec bl
\s*[a-f0-9]+:\s*62 f4 6c 1c fe cb\s+\{nf\} dec dl,bl
\s*[a-f0-9]+:\s*62 f4 7d 0c ff ca\s+\{nf\} dec dx
\s*[a-f0-9]+:\s*62 f4 7d 1c ff ca\s+\{nf\} dec ax,dx
\s*[a-f0-9]+:\s*62 f4 7c 0c ff c9\s+\{nf\} dec ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c ff c9\s+\{nf\} dec edx,ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c ff c9\s+\{nf\} dec r9
\s*[a-f0-9]+:\s*62 d4 84 14 ff c9\s+\{nf\} dec r31,r9
\s*[a-f0-9]+:\s*62 d4 7c 0c fe 8c 80 23 01 00 00\s+\{nf\} dec BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 64 1c fe 8c 80 23 01 00 00\s+\{nf\} dec bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c ff 8c 80 23 01 00 00\s+\{nf\} dec WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6d 1c ff 8c 80 23 01 00 00\s+\{nf\} dec dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c ff 8c 80 23 01 00 00\s+\{nf\} dec DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 74 1c ff 8c 80 23 01 00 00\s+\{nf\} dec ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c ff 8c 80 23 01 00 00\s+\{nf\} dec QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 b4 1c ff 8c 80 23 01 00 00\s+\{nf\} dec r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 f3\s+\{nf\} div bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 f2\s+\{nf\} div dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 f1\s+\{nf\} div ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 f1\s+\{nf\} div r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 b4 80 23 01 00 00\s+\{nf\} div BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 b4 80 23 01 00 00\s+\{nf\} div WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 b4 80 23 01 00 00\s+\{nf\} div DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 b4 80 23 01 00 00\s+\{nf\} div QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 fb\s+\{nf\} idiv bl
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 fb\s+\{nf\} idiv bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 fa\s+\{nf\} idiv dx
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 fa\s+\{nf\} idiv dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 f9\s+\{nf\} idiv ecx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 f9\s+\{nf\} idiv ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 f9\s+\{nf\} idiv r9
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 f9\s+\{nf\} idiv r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 bc 80 23 01 00 00\s+\{nf\} idiv BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 bc 80 23 01 00 00\s+\{nf\} idiv BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 bc 80 23 01 00 00\s+\{nf\} idiv WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 bc 80 23 01 00 00\s+\{nf\} idiv WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 bc 80 23 01 00 00\s+\{nf\} idiv DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 bc 80 23 01 00 00\s+\{nf\} idiv DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 bc 80 23 01 00 00\s+\{nf\} idiv QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 bc 80 23 01 00 00\s+\{nf\} idiv QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 eb\s+\{nf\} imul bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 ea\s+\{nf\} imul dx
\s*[a-f0-9]+:\s*62 f4 7d 0c af c2\s+\{nf\} imul ax,dx
\s*[a-f0-9]+:\s*62 f4 35 1c af c2\s+\{nf\} imul r9w,ax,dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 e9\s+\{nf\} imul ecx
\s*[a-f0-9]+:\s*62 f4 7c 0c af d1\s+\{nf\} imul edx,ecx
\s*[a-f0-9]+:\s*62 f4 2c 1c af d1\s+\{nf\} imul r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 e9\s+\{nf\} imul r9
\s*[a-f0-9]+:\s*62 44 fc 0c af f9\s+\{nf\} imul r31,r9
\s*[a-f0-9]+:\s*62 44 a4 1c af f9\s+\{nf\} imul r11,r31,r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 ac 80 23 01 00 00\s+\{nf\} imul BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 ac 80 23 01 00 00\s+\{nf\} imul WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c af 94 80 23 01 00 00\s+\{nf\} imul dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 1c af 94 80 23 01 00 00\s+\{nf\} imul ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 ac 80 23 01 00 00\s+\{nf\} imul DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c af 8c 80 23 01 00 00\s+\{nf\} imul ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c af 8c 80 23 01 00 00\s+\{nf\} imul edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 ac 80 23 01 00 00\s+\{nf\} imul QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c af 8c 80 23 01 00 00\s+\{nf\} imul r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 84 14 af 8c 80 23 01 00 00\s+\{nf\} imul r31,r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c fe c3\s+\{nf\} inc bl
\s*[a-f0-9]+:\s*62 f4 6c 1c fe c3\s+\{nf\} inc dl,bl
\s*[a-f0-9]+:\s*62 f4 7d 0c ff c2\s+\{nf\} inc dx
\s*[a-f0-9]+:\s*62 f4 7d 1c ff c2\s+\{nf\} inc ax,dx
\s*[a-f0-9]+:\s*62 f4 7c 0c ff c1\s+\{nf\} inc ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c ff c1\s+\{nf\} inc edx,ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c ff c1\s+\{nf\} inc r9
\s*[a-f0-9]+:\s*62 d4 84 14 ff c1\s+\{nf\} inc r31,r9
\s*[a-f0-9]+:\s*62 d4 7c 0c fe 84 80 23 01 00 00\s+\{nf\} inc BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 64 1c fe 84 80 23 01 00 00\s+\{nf\} inc bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c ff 84 80 23 01 00 00\s+\{nf\} inc WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6d 1c ff 84 80 23 01 00 00\s+\{nf\} inc dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c ff 84 80 23 01 00 00\s+\{nf\} inc DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 74 1c ff 84 80 23 01 00 00\s+\{nf\} inc ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c ff 84 80 23 01 00 00\s+\{nf\} inc QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 b4 1c ff 84 80 23 01 00 00\s+\{nf\} inc r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7d 0c f5 c2\s+\{nf\} lzcnt ax,dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f5 d1\s+\{nf\} lzcnt edx,ecx
\s*[a-f0-9]+:\s*62 44 fc 0c f5 f9\s+\{nf\} lzcnt r31,r9
\s*[a-f0-9]+:\s*62 d4 7d 0c f5 94 80 23 01 00 00\s+\{nf\} lzcnt dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f5 8c 80 23 01 00 00\s+\{nf\} lzcnt ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c f5 8c 80 23 01 00 00\s+\{nf\} lzcnt r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 e3\s+\{nf\} mul bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 e2\s+\{nf\} mul dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 e1\s+\{nf\} mul ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 e1\s+\{nf\} mul r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 a4 80 23 01 00 00\s+\{nf\} mul BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 a4 80 23 01 00 00\s+\{nf\} mul WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 a4 80 23 01 00 00\s+\{nf\} mul DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 a4 80 23 01 00 00\s+\{nf\} mul QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 db\s+\{nf\} neg bl
\s*[a-f0-9]+:\s*62 f4 6c 1c f6 db\s+\{nf\} neg dl,bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 da\s+\{nf\} neg dx
\s*[a-f0-9]+:\s*62 f4 7d 1c f7 da\s+\{nf\} neg ax,dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 d9\s+\{nf\} neg ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c f7 d9\s+\{nf\} neg edx,ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 d9\s+\{nf\} neg r9
\s*[a-f0-9]+:\s*62 d4 84 14 f7 d9\s+\{nf\} neg r31,r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 9c 80 23 01 00 00\s+\{nf\} neg BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 64 1c f6 9c 80 23 01 00 00\s+\{nf\} neg bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 9c 80 23 01 00 00\s+\{nf\} neg WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6d 1c f7 9c 80 23 01 00 00\s+\{nf\} neg dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 9c 80 23 01 00 00\s+\{nf\} neg DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 74 1c f7 9c 80 23 01 00 00\s+\{nf\} neg ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 9c 80 23 01 00 00\s+\{nf\} neg QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 b4 1c f7 9c 80 23 01 00 00\s+\{nf\} neg r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 cb 7b\s+\{nf\} or bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 cb 7b\s+\{nf\} or dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 ca 7b\s+\{nf\} or dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 ca 7b\s+\{nf\} or ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 c9 7b\s+\{nf\} or ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 c9 7b\s+\{nf\} or edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 c9 7b\s+\{nf\} or r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 83 c9 7b\s+\{nf\} or r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 8c 80 23 01 00 00 7b\s+\{nf\} or BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c 80 8c 80 23 01 00 00 7b\s+\{nf\} or bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 8c 80 23 01 00 00 7b\s+\{nf\} or WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 8c 80 23 01 00 00 7b\s+\{nf\} or dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 8c 80 23 01 00 00 7b\s+\{nf\} or DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c 83 8c 80 23 01 00 00 7b\s+\{nf\} or ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 8c 80 23 01 00 00 7b\s+\{nf\} or QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 8c 80 23 01 00 00 7b\s+\{nf\} or r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 08 da\s+\{nf\} or dl,bl
\s*[a-f0-9]+:\s*62 f4 3c 1c 08 da\s+\{nf\} or r8b,dl,bl
\s*[a-f0-9]+:\s*62 d4 7c 0c 08 9c 80 23 01 00 00\s+\{nf\} or BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 08 9c 80 23 01 00 00\s+\{nf\} or dl,BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 f4 7d 0c 09 d0\s+\{nf\} or ax,dx
\s*[a-f0-9]+:\s*62 f4 35 1c 09 d0\s+\{nf\} or r9w,ax,dx
\s*[a-f0-9]+:\s*62 d4 7d 0c 09 94 80 23 01 00 00\s+\{nf\} or WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 09 94 80 23 01 00 00\s+\{nf\} or ax,WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 f4 7c 0c 09 ca\s+\{nf\} or edx,ecx
\s*[a-f0-9]+:\s*62 f4 2c 1c 09 ca\s+\{nf\} or r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d4 7c 0c 09 8c 80 23 01 00 00\s+\{nf\} or DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 09 8c 80 23 01 00 00\s+\{nf\} or edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 5c fc 0c 09 cf\s+\{nf\} or r31,r9
\s*[a-f0-9]+:\s*62 5c a4 1c 09 cf\s+\{nf\} or r11,r31,r9
\s*[a-f0-9]+:\s*62 54 fc 0c 09 8c 80 23 01 00 00\s+\{nf\} or QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 54 84 14 09 8c 80 23 01 00 00\s+\{nf\} or r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 d4 7c 0c 0a 9c 80 23 01 00 00\s+\{nf\} or bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 0a 9c 80 23 01 00 00\s+\{nf\} or dl,bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c 0b 94 80 23 01 00 00\s+\{nf\} or dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 1c 0b 94 80 23 01 00 00\s+\{nf\} or ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c 0b 8c 80 23 01 00 00\s+\{nf\} or ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 0b 8c 80 23 01 00 00\s+\{nf\} or edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c 0b 8c 80 23 01 00 00\s+\{nf\} or r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 84 14 0b 8c 80 23 01 00 00\s+\{nf\} or r31,r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7d 0c 88 c2\s+\{nf\} popcnt ax,dx
\s*[a-f0-9]+:\s*62 f4 7c 0c 88 d1\s+\{nf\} popcnt edx,ecx
\s*[a-f0-9]+:\s*62 44 fc 0c 88 f9\s+\{nf\} popcnt r31,r9
\s*[a-f0-9]+:\s*62 d4 7d 0c 88 94 80 23 01 00 00\s+\{nf\} popcnt dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c 88 8c 80 23 01 00 00\s+\{nf\} popcnt ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c 88 8c 80 23 01 00 00\s+\{nf\} popcnt r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 c3\s+\{nf\} rol bl,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 c3\s+\{nf\} rol dl,bl,1
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 c2\s+\{nf\} rol dx,1
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 c2\s+\{nf\} rol ax,dx,1
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 c1\s+\{nf\} rol ecx,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 c1\s+\{nf\} rol edx,ecx,1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 c1\s+\{nf\} rol r9,1
\s*[a-f0-9]+:\s*62 d4 84 14 d1 c1\s+\{nf\} rol r31,r9,1
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 84 80 23 01 00 00\s+\{nf\} rol BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 64 1c d0 84 80 23 01 00 00\s+\{nf\} rol bl,BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 84 80 23 01 00 00\s+\{nf\} rol WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 84 80 23 01 00 00\s+\{nf\} rol dx,WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 84 80 23 01 00 00\s+\{nf\} rol DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 74 1c d1 84 80 23 01 00 00\s+\{nf\} rol ecx,DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 84 80 23 01 00 00\s+\{nf\} rol QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 84 80 23 01 00 00\s+\{nf\} rol r9,QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 c3 7b\s+\{nf\} rol bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 c3 7b\s+\{nf\} rol dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 c2 7b\s+\{nf\} rol dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 c2 7b\s+\{nf\} rol ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 c1 7b\s+\{nf\} rol ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 c1 7b\s+\{nf\} rol edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 c1 7b\s+\{nf\} rol r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 c1 c1 7b\s+\{nf\} rol r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 84 80 23 01 00 00 7b\s+\{nf\} rol BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c c0 84 80 23 01 00 00 7b\s+\{nf\} rol bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 84 80 23 01 00 00 7b\s+\{nf\} rol WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 84 80 23 01 00 00 7b\s+\{nf\} rol dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 84 80 23 01 00 00 7b\s+\{nf\} rol DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c c1 84 80 23 01 00 00 7b\s+\{nf\} rol ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 84 80 23 01 00 00 7b\s+\{nf\} rol QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 84 80 23 01 00 00 7b\s+\{nf\} rol r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 c3\s+\{nf\} rol bl,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 c3\s+\{nf\} rol dl,bl,cl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 c2\s+\{nf\} rol dx,cl
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 c2\s+\{nf\} rol ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 c1\s+\{nf\} rol ecx,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 c1\s+\{nf\} rol edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 c1\s+\{nf\} rol r9,cl
\s*[a-f0-9]+:\s*62 d4 84 14 d3 c1\s+\{nf\} rol r31,r9,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 84 80 23 01 00 00\s+\{nf\} rol BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 64 1c d2 84 80 23 01 00 00\s+\{nf\} rol bl,BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 84 80 23 01 00 00\s+\{nf\} rol WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 84 80 23 01 00 00\s+\{nf\} rol dx,WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 84 80 23 01 00 00\s+\{nf\} rol DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 74 1c d3 84 80 23 01 00 00\s+\{nf\} rol ecx,DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 84 80 23 01 00 00\s+\{nf\} rol QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 84 80 23 01 00 00\s+\{nf\} rol r9,QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 cb\s+\{nf\} ror bl,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 cb\s+\{nf\} ror dl,bl,1
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 ca\s+\{nf\} ror dx,1
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 ca\s+\{nf\} ror ax,dx,1
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 c9\s+\{nf\} ror ecx,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 c9\s+\{nf\} ror edx,ecx,1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 c9\s+\{nf\} ror r9,1
\s*[a-f0-9]+:\s*62 d4 84 14 d1 c9\s+\{nf\} ror r31,r9,1
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 8c 80 23 01 00 00\s+\{nf\} ror BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 64 1c d0 8c 80 23 01 00 00\s+\{nf\} ror bl,BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 8c 80 23 01 00 00\s+\{nf\} ror WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 8c 80 23 01 00 00\s+\{nf\} ror dx,WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 8c 80 23 01 00 00\s+\{nf\} ror DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 74 1c d1 8c 80 23 01 00 00\s+\{nf\} ror ecx,DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 8c 80 23 01 00 00\s+\{nf\} ror QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 8c 80 23 01 00 00\s+\{nf\} ror r9,QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 cb 7b\s+\{nf\} ror bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 cb 7b\s+\{nf\} ror dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 ca 7b\s+\{nf\} ror dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 ca 7b\s+\{nf\} ror ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 c9 7b\s+\{nf\} ror ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 c9 7b\s+\{nf\} ror edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 c9 7b\s+\{nf\} ror r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 c1 c9 7b\s+\{nf\} ror r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 8c 80 23 01 00 00 7b\s+\{nf\} ror BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c c0 8c 80 23 01 00 00 7b\s+\{nf\} ror bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 cb\s+\{nf\} ror bl,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 cb\s+\{nf\} ror dl,bl,cl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 ca\s+\{nf\} ror dx,cl
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 ca\s+\{nf\} ror ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 c9\s+\{nf\} ror ecx,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 c9\s+\{nf\} ror edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 c9\s+\{nf\} ror r9,cl
\s*[a-f0-9]+:\s*62 d4 84 14 d3 c9\s+\{nf\} ror r31,r9,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 8c 80 23 01 00 00\s+\{nf\} ror BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 64 1c d2 8c 80 23 01 00 00\s+\{nf\} ror bl,BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 8c 80 23 01 00 00\s+\{nf\} ror WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 8c 80 23 01 00 00\s+\{nf\} ror dx,WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 8c 80 23 01 00 00\s+\{nf\} ror DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 74 1c d3 8c 80 23 01 00 00\s+\{nf\} ror ecx,DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 8c 80 23 01 00 00\s+\{nf\} ror QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 8c 80 23 01 00 00\s+\{nf\} ror r9,QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 e3\s+\{nf\} shl bl,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 e3\s+\{nf\} shl dl,bl,1
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 e2\s+\{nf\} shl dx,1
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 e2\s+\{nf\} shl ax,dx,1
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 e1\s+\{nf\} shl ecx,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 e1\s+\{nf\} shl edx,ecx,1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 e1\s+\{nf\} shl r9,1
\s*[a-f0-9]+:\s*62 d4 84 14 d1 e1\s+\{nf\} shl r31,r9,1
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 a4 80 23 01 00 00\s+\{nf\} shl BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 64 1c d0 a4 80 23 01 00 00\s+\{nf\} shl bl,BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 a4 80 23 01 00 00\s+\{nf\} shl WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 a4 80 23 01 00 00\s+\{nf\} shl dx,WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 a4 80 23 01 00 00\s+\{nf\} shl DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 74 1c d1 a4 80 23 01 00 00\s+\{nf\} shl ecx,DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 a4 80 23 01 00 00\s+\{nf\} shl QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 a4 80 23 01 00 00\s+\{nf\} shl r9,QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 e3 7b\s+\{nf\} shl bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 e3 7b\s+\{nf\} shl dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 e2 7b\s+\{nf\} shl dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 e2 7b\s+\{nf\} shl ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 e1 7b\s+\{nf\} shl ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 e1 7b\s+\{nf\} shl edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 e1 7b\s+\{nf\} shl r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 c1 e1 7b\s+\{nf\} shl r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 a4 80 23 01 00 00 7b\s+\{nf\} shl BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c c0 a4 80 23 01 00 00 7b\s+\{nf\} shl bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 e3\s+\{nf\} shl bl,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 e3\s+\{nf\} shl dl,bl,cl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 e2\s+\{nf\} shl dx,cl
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 e2\s+\{nf\} shl ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 e1\s+\{nf\} shl ecx,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 e1\s+\{nf\} shl edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 e1\s+\{nf\} shl r9,cl
\s*[a-f0-9]+:\s*62 d4 84 14 d3 e1\s+\{nf\} shl r31,r9,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 a4 80 23 01 00 00\s+\{nf\} shl BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 64 1c d2 a4 80 23 01 00 00\s+\{nf\} shl bl,BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 a4 80 23 01 00 00\s+\{nf\} shl WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 a4 80 23 01 00 00\s+\{nf\} shl dx,WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 a4 80 23 01 00 00\s+\{nf\} shl DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 74 1c d3 a4 80 23 01 00 00\s+\{nf\} shl ecx,DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 a4 80 23 01 00 00\s+\{nf\} shl QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 a4 80 23 01 00 00\s+\{nf\} shl r9,QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 fb\s+\{nf\} sar bl,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 fb\s+\{nf\} sar dl,bl,1
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 fa\s+\{nf\} sar dx,1
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 fa\s+\{nf\} sar ax,dx,1
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 f9\s+\{nf\} sar ecx,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 f9\s+\{nf\} sar edx,ecx,1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 f9\s+\{nf\} sar r9,1
\s*[a-f0-9]+:\s*62 d4 84 14 d1 f9\s+\{nf\} sar r31,r9,1
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 bc 80 23 01 00 00\s+\{nf\} sar BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 64 1c d0 bc 80 23 01 00 00\s+\{nf\} sar bl,BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 bc 80 23 01 00 00\s+\{nf\} sar WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 bc 80 23 01 00 00\s+\{nf\} sar dx,WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 bc 80 23 01 00 00\s+\{nf\} sar DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 74 1c d1 bc 80 23 01 00 00\s+\{nf\} sar ecx,DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 bc 80 23 01 00 00\s+\{nf\} sar QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 bc 80 23 01 00 00\s+\{nf\} sar r9,QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 fb 7b\s+\{nf\} sar bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 fb 7b\s+\{nf\} sar dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 fa 7b\s+\{nf\} sar dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 fa 7b\s+\{nf\} sar ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 f9 7b\s+\{nf\} sar ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 f9 7b\s+\{nf\} sar edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 f9 7b\s+\{nf\} sar r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 c1 f9 7b\s+\{nf\} sar r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 bc 80 23 01 00 00 7b\s+\{nf\} sar BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c c0 bc 80 23 01 00 00 7b\s+\{nf\} sar bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 fb\s+\{nf\} sar bl,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 fb\s+\{nf\} sar dl,bl,cl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 fa\s+\{nf\} sar dx,cl
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 fa\s+\{nf\} sar ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 f9\s+\{nf\} sar ecx,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 f9\s+\{nf\} sar edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 f9\s+\{nf\} sar r9,cl
\s*[a-f0-9]+:\s*62 d4 84 14 d3 f9\s+\{nf\} sar r31,r9,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 bc 80 23 01 00 00\s+\{nf\} sar BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 64 1c d2 bc 80 23 01 00 00\s+\{nf\} sar bl,BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 bc 80 23 01 00 00\s+\{nf\} sar WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 bc 80 23 01 00 00\s+\{nf\} sar dx,WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 bc 80 23 01 00 00\s+\{nf\} sar DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 74 1c d3 bc 80 23 01 00 00\s+\{nf\} sar ecx,DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 bc 80 23 01 00 00\s+\{nf\} sar QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 bc 80 23 01 00 00\s+\{nf\} sar r9,QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 e3\s+\{nf\} shl bl,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 e3\s+\{nf\} shl dl,bl,1
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 e2\s+\{nf\} shl dx,1
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 e2\s+\{nf\} shl ax,dx,1
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 e1\s+\{nf\} shl ecx,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 e1\s+\{nf\} shl edx,ecx,1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 e1\s+\{nf\} shl r9,1
\s*[a-f0-9]+:\s*62 d4 84 14 d1 e1\s+\{nf\} shl r31,r9,1
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 a4 80 23 01 00 00\s+\{nf\} shl BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 64 1c d0 a4 80 23 01 00 00\s+\{nf\} shl bl,BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 a4 80 23 01 00 00\s+\{nf\} shl WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 a4 80 23 01 00 00\s+\{nf\} shl dx,WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 a4 80 23 01 00 00\s+\{nf\} shl DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 74 1c d1 a4 80 23 01 00 00\s+\{nf\} shl ecx,DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 a4 80 23 01 00 00\s+\{nf\} shl QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 a4 80 23 01 00 00\s+\{nf\} shl r9,QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 e3 7b\s+\{nf\} shl bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 e3 7b\s+\{nf\} shl dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 e2 7b\s+\{nf\} shl dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 e2 7b\s+\{nf\} shl ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 e1 7b\s+\{nf\} shl ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 e1 7b\s+\{nf\} shl edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 e1 7b\s+\{nf\} shl r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 c1 e1 7b\s+\{nf\} shl r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 a4 80 23 01 00 00 7b\s+\{nf\} shl BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c c0 a4 80 23 01 00 00 7b\s+\{nf\} shl bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 e3\s+\{nf\} shl bl,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 e3\s+\{nf\} shl dl,bl,cl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 e2\s+\{nf\} shl dx,cl
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 e2\s+\{nf\} shl ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 e1\s+\{nf\} shl ecx,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 e1\s+\{nf\} shl edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 e1\s+\{nf\} shl r9,cl
\s*[a-f0-9]+:\s*62 d4 84 14 d3 e1\s+\{nf\} shl r31,r9,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 a4 80 23 01 00 00\s+\{nf\} shl BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 64 1c d2 a4 80 23 01 00 00\s+\{nf\} shl bl,BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 a4 80 23 01 00 00\s+\{nf\} shl WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 a4 80 23 01 00 00\s+\{nf\} shl dx,WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 a4 80 23 01 00 00\s+\{nf\} shl DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 74 1c d3 a4 80 23 01 00 00\s+\{nf\} shl ecx,DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 a4 80 23 01 00 00\s+\{nf\} shl QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 a4 80 23 01 00 00\s+\{nf\} shl r9,QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 f4 7d 0c 24 d0 7b\s+\{nf\} shld ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 35 1c 24 d0 7b\s+\{nf\} shld r9w,ax,dx,0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 24 94 80 23 01 00 00 7b\s+\{nf\} shld WORD PTR \[r8\+rax\*4\+0x123\],dx,0x7b
\s*[a-f0-9]+:\s*62 d4 7d 1c 24 94 80 23 01 00 00 7b\s+\{nf\} shld ax,WORD PTR \[r8\+rax\*4\+0x123\],dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 24 ca 7b\s+\{nf\} shld edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 2c 1c 24 ca 7b\s+\{nf\} shld r10d,edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 24 8c 80 23 01 00 00 7b\s+\{nf\} shld DWORD PTR \[r8\+rax\*4\+0x123\],ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 6c 1c 24 8c 80 23 01 00 00 7b\s+\{nf\} shld edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx,0x7b
\s*[a-f0-9]+:\s*62 5c fc 0c 24 cf 7b\s+\{nf\} shld r31,r9,0x7b
\s*[a-f0-9]+:\s*62 5c a4 1c 24 cf 7b\s+\{nf\} shld r11,r31,r9,0x7b
\s*[a-f0-9]+:\s*62 54 fc 0c 24 8c 80 23 01 00 00 7b\s+\{nf\} shld QWORD PTR \[r8\+rax\*4\+0x123\],r9,0x7b
\s*[a-f0-9]+:\s*62 54 84 14 24 8c 80 23 01 00 00 7b\s+\{nf\} shld r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c a5 d0\s+\{nf\} shld ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 35 1c a5 d0\s+\{nf\} shld r9w,ax,dx,cl
\s*[a-f0-9]+:\s*62 d4 7d 0c a5 94 80 23 01 00 00\s+\{nf\} shld WORD PTR \[r8\+rax\*4\+0x123\],dx,cl
\s*[a-f0-9]+:\s*62 d4 7d 1c a5 94 80 23 01 00 00\s+\{nf\} shld ax,WORD PTR \[r8\+rax\*4\+0x123\],dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c a5 ca\s+\{nf\} shld edx,ecx,cl
\s*[a-f0-9]+:\s*62 f4 2c 1c a5 ca\s+\{nf\} shld r10d,edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c a5 8c 80 23 01 00 00\s+\{nf\} shld DWORD PTR \[r8\+rax\*4\+0x123\],ecx,cl
\s*[a-f0-9]+:\s*62 d4 6c 1c a5 8c 80 23 01 00 00\s+\{nf\} shld edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx,cl
\s*[a-f0-9]+:\s*62 5c fc 0c a5 cf\s+\{nf\} shld r31,r9,cl
\s*[a-f0-9]+:\s*62 5c a4 1c a5 cf\s+\{nf\} shld r11,r31,r9,cl
\s*[a-f0-9]+:\s*62 54 fc 0c a5 8c 80 23 01 00 00\s+\{nf\} shld QWORD PTR \[r8\+rax\*4\+0x123\],r9,cl
\s*[a-f0-9]+:\s*62 54 84 14 a5 8c 80 23 01 00 00\s+\{nf\} shld r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 eb\s+\{nf\} shr bl,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 eb\s+\{nf\} shr dl,bl,1
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 ea\s+\{nf\} shr dx,1
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 ea\s+\{nf\} shr ax,dx,1
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 e9\s+\{nf\} shr ecx,1
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 e9\s+\{nf\} shr edx,ecx,1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 e9\s+\{nf\} shr r9,1
\s*[a-f0-9]+:\s*62 d4 84 14 d1 e9\s+\{nf\} shr r31,r9,1
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 ac 80 23 01 00 00\s+\{nf\} shr BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 64 1c d0 ac 80 23 01 00 00\s+\{nf\} shr bl,BYTE PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 ac 80 23 01 00 00\s+\{nf\} shr WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 ac 80 23 01 00 00\s+\{nf\} shr dx,WORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 ac 80 23 01 00 00\s+\{nf\} shr DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 74 1c d1 ac 80 23 01 00 00\s+\{nf\} shr ecx,DWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 ac 80 23 01 00 00\s+\{nf\} shr QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 ac 80 23 01 00 00\s+\{nf\} shr r9,QWORD PTR \[r8\+rax\*4\+0x123\],1
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 eb 7b\s+\{nf\} shr bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 eb 7b\s+\{nf\} shr dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 ea 7b\s+\{nf\} shr dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 ea 7b\s+\{nf\} shr ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 e9 7b\s+\{nf\} shr ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 e9 7b\s+\{nf\} shr edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 e9 7b\s+\{nf\} shr r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 c1 e9 7b\s+\{nf\} shr r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 ac 80 23 01 00 00 7b\s+\{nf\} shr BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c c0 ac 80 23 01 00 00 7b\s+\{nf\} shr bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 eb\s+\{nf\} shr bl,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 eb\s+\{nf\} shr dl,bl,cl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 ea\s+\{nf\} shr dx,cl
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 ea\s+\{nf\} shr ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 e9\s+\{nf\} shr ecx,cl
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 e9\s+\{nf\} shr edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 e9\s+\{nf\} shr r9,cl
\s*[a-f0-9]+:\s*62 d4 84 14 d3 e9\s+\{nf\} shr r31,r9,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 ac 80 23 01 00 00\s+\{nf\} shr BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 64 1c d2 ac 80 23 01 00 00\s+\{nf\} shr bl,BYTE PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 ac 80 23 01 00 00\s+\{nf\} shr WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 ac 80 23 01 00 00\s+\{nf\} shr dx,WORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 ac 80 23 01 00 00\s+\{nf\} shr DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 74 1c d3 ac 80 23 01 00 00\s+\{nf\} shr ecx,DWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 ac 80 23 01 00 00\s+\{nf\} shr QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 ac 80 23 01 00 00\s+\{nf\} shr r9,QWORD PTR \[r8\+rax\*4\+0x123\],cl
\s*[a-f0-9]+:\s*62 f4 7d 0c 2c d0 7b\s+\{nf\} shrd ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 35 1c 2c d0 7b\s+\{nf\} shrd r9w,ax,dx,0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 2c 94 80 23 01 00 00 7b\s+\{nf\} shrd WORD PTR \[r8\+rax\*4\+0x123\],dx,0x7b
\s*[a-f0-9]+:\s*62 d4 7d 1c 2c 94 80 23 01 00 00 7b\s+\{nf\} shrd ax,WORD PTR \[r8\+rax\*4\+0x123\],dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 2c ca 7b\s+\{nf\} shrd edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 2c 1c 2c ca 7b\s+\{nf\} shrd r10d,edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd DWORD PTR \[r8\+rax\*4\+0x123\],ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 6c 1c 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx,0x7b
\s*[a-f0-9]+:\s*62 5c fc 0c 2c cf 7b\s+\{nf\} shrd r31,r9,0x7b
\s*[a-f0-9]+:\s*62 5c a4 1c 2c cf 7b\s+\{nf\} shrd r11,r31,r9,0x7b
\s*[a-f0-9]+:\s*62 54 fc 0c 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd QWORD PTR \[r8\+rax\*4\+0x123\],r9,0x7b
\s*[a-f0-9]+:\s*62 54 84 14 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c ad d0\s+\{nf\} shrd ax,dx,cl
\s*[a-f0-9]+:\s*62 f4 35 1c ad d0\s+\{nf\} shrd r9w,ax,dx,cl
\s*[a-f0-9]+:\s*62 d4 7d 0c ad 94 80 23 01 00 00\s+\{nf\} shrd WORD PTR \[r8\+rax\*4\+0x123\],dx,cl
\s*[a-f0-9]+:\s*62 d4 7d 1c ad 94 80 23 01 00 00\s+\{nf\} shrd ax,WORD PTR \[r8\+rax\*4\+0x123\],dx,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c ad ca\s+\{nf\} shrd edx,ecx,cl
\s*[a-f0-9]+:\s*62 f4 2c 1c ad ca\s+\{nf\} shrd r10d,edx,ecx,cl
\s*[a-f0-9]+:\s*62 d4 7c 0c ad 8c 80 23 01 00 00\s+\{nf\} shrd DWORD PTR \[r8\+rax\*4\+0x123\],ecx,cl
\s*[a-f0-9]+:\s*62 d4 6c 1c ad 8c 80 23 01 00 00\s+\{nf\} shrd edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx,cl
\s*[a-f0-9]+:\s*62 5c fc 0c ad cf\s+\{nf\} shrd r31,r9,cl
\s*[a-f0-9]+:\s*62 5c a4 1c ad cf\s+\{nf\} shrd r11,r31,r9,cl
\s*[a-f0-9]+:\s*62 54 fc 0c ad 8c 80 23 01 00 00\s+\{nf\} shrd QWORD PTR \[r8\+rax\*4\+0x123\],r9,cl
\s*[a-f0-9]+:\s*62 54 84 14 ad 8c 80 23 01 00 00\s+\{nf\} shrd r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9,cl
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 eb 7b\s+\{nf\} sub bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 eb 7b\s+\{nf\} sub dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 ea 7b\s+\{nf\} sub dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 ea 7b\s+\{nf\} sub ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 e9 7b\s+\{nf\} sub ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 e9 7b\s+\{nf\} sub edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 e9 7b\s+\{nf\} sub r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 83 e9 7b\s+\{nf\} sub r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 ac 80 23 01 00 00 7b\s+\{nf\} sub BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c 80 ac 80 23 01 00 00 7b\s+\{nf\} sub bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 28 da\s+\{nf\} sub dl,bl
\s*[a-f0-9]+:\s*62 f4 3c 1c 28 da\s+\{nf\} sub r8b,dl,bl
\s*[a-f0-9]+:\s*62 d4 7c 0c 28 9c 80 23 01 00 00\s+\{nf\} sub BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 28 9c 80 23 01 00 00\s+\{nf\} sub dl,BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 f4 7d 0c 29 d0\s+\{nf\} sub ax,dx
\s*[a-f0-9]+:\s*62 f4 35 1c 29 d0\s+\{nf\} sub r9w,ax,dx
\s*[a-f0-9]+:\s*62 d4 7d 0c 29 94 80 23 01 00 00\s+\{nf\} sub WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 29 94 80 23 01 00 00\s+\{nf\} sub ax,WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 f4 7c 0c 29 ca\s+\{nf\} sub edx,ecx
\s*[a-f0-9]+:\s*62 f4 2c 1c 29 ca\s+\{nf\} sub r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d4 7c 0c 29 8c 80 23 01 00 00\s+\{nf\} sub DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 29 8c 80 23 01 00 00\s+\{nf\} sub edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 5c fc 0c 29 cf\s+\{nf\} sub r31,r9
\s*[a-f0-9]+:\s*62 5c a4 1c 29 cf\s+\{nf\} sub r11,r31,r9
\s*[a-f0-9]+:\s*62 54 fc 0c 29 8c 80 23 01 00 00\s+\{nf\} sub QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 54 84 14 29 8c 80 23 01 00 00\s+\{nf\} sub r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 d4 7c 0c 2a 9c 80 23 01 00 00\s+\{nf\} sub bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 2a 9c 80 23 01 00 00\s+\{nf\} sub dl,bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c 2b 94 80 23 01 00 00\s+\{nf\} sub dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 1c 2b 94 80 23 01 00 00\s+\{nf\} sub ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c 2b 8c 80 23 01 00 00\s+\{nf\} sub ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 2b 8c 80 23 01 00 00\s+\{nf\} sub edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c 2b 8c 80 23 01 00 00\s+\{nf\} sub r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 84 14 2b 8c 80 23 01 00 00\s+\{nf\} sub r31,r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7d 0c f4 c2\s+\{nf\} tzcnt ax,dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f4 d1\s+\{nf\} tzcnt edx,ecx
\s*[a-f0-9]+:\s*62 44 fc 0c f4 f9\s+\{nf\} tzcnt r31,r9
\s*[a-f0-9]+:\s*62 d4 7d 0c f4 94 80 23 01 00 00\s+\{nf\} tzcnt dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c f4 8c 80 23 01 00 00\s+\{nf\} tzcnt ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c f4 8c 80 23 01 00 00\s+\{nf\} tzcnt r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 f3 7b\s+\{nf\} xor bl,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 f3 7b\s+\{nf\} xor dl,bl,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 f2 7b\s+\{nf\} xor dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 f2 7b\s+\{nf\} xor ax,dx,0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 f1 7b\s+\{nf\} xor ecx,0x7b
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 f1 7b\s+\{nf\} xor edx,ecx,0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 f1 7b\s+\{nf\} xor r9,0x7b
\s*[a-f0-9]+:\s*62 d4 84 14 83 f1 7b\s+\{nf\} xor r31,r9,0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 b4 80 23 01 00 00 7b\s+\{nf\} xor BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 64 1c 80 b4 80 23 01 00 00 7b\s+\{nf\} xor bl,BYTE PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor dx,WORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 74 1c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor ecx,DWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor r9,QWORD PTR \[r8\+rax\*4\+0x123\],0x7b
\s*[a-f0-9]+:\s*62 f4 7c 0c 30 da\s+\{nf\} xor dl,bl
\s*[a-f0-9]+:\s*62 f4 3c 1c 30 da\s+\{nf\} xor r8b,dl,bl
\s*[a-f0-9]+:\s*62 d4 7c 0c 30 9c 80 23 01 00 00\s+\{nf\} xor BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 30 9c 80 23 01 00 00\s+\{nf\} xor dl,BYTE PTR \[r8\+rax\*4\+0x123\],bl
\s*[a-f0-9]+:\s*62 f4 7d 0c 31 d0\s+\{nf\} xor ax,dx
\s*[a-f0-9]+:\s*62 f4 35 1c 31 d0\s+\{nf\} xor r9w,ax,dx
\s*[a-f0-9]+:\s*62 d4 7d 0c 31 94 80 23 01 00 00\s+\{nf\} xor WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 31 94 80 23 01 00 00\s+\{nf\} xor ax,WORD PTR \[r8\+rax\*4\+0x123\],dx
\s*[a-f0-9]+:\s*62 f4 7c 0c 31 ca\s+\{nf\} xor edx,ecx
\s*[a-f0-9]+:\s*62 f4 2c 1c 31 ca\s+\{nf\} xor r10d,edx,ecx
\s*[a-f0-9]+:\s*62 d4 7c 0c 31 8c 80 23 01 00 00\s+\{nf\} xor DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 31 8c 80 23 01 00 00\s+\{nf\} xor edx,DWORD PTR \[r8\+rax\*4\+0x123\],ecx
\s*[a-f0-9]+:\s*62 5c fc 0c 31 cf\s+\{nf\} xor r31,r9
\s*[a-f0-9]+:\s*62 5c a4 1c 31 cf\s+\{nf\} xor r11,r31,r9
\s*[a-f0-9]+:\s*62 54 fc 0c 31 8c 80 23 01 00 00\s+\{nf\} xor QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 54 84 14 31 8c 80 23 01 00 00\s+\{nf\} xor r31,QWORD PTR \[r8\+rax\*4\+0x123\],r9
\s*[a-f0-9]+:\s*62 d4 7c 0c 32 9c 80 23 01 00 00\s+\{nf\} xor bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 32 9c 80 23 01 00 00\s+\{nf\} xor dl,bl,BYTE PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 0c 33 94 80 23 01 00 00\s+\{nf\} xor dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7d 1c 33 94 80 23 01 00 00\s+\{nf\} xor ax,dx,WORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 7c 0c 33 8c 80 23 01 00 00\s+\{nf\} xor ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 d4 6c 1c 33 8c 80 23 01 00 00\s+\{nf\} xor edx,ecx,DWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 fc 0c 33 8c 80 23 01 00 00\s+\{nf\} xor r9,QWORD PTR \[r8\+rax\*4\+0x123\]
\s*[a-f0-9]+:\s*62 54 84 14 33 8c 80 23 01 00 00\s+\{nf\} xor r31,r9,QWORD PTR \[r8\+rax\*4\+0x123\]
#pass
