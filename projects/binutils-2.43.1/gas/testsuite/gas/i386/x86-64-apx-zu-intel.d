#as:
#objdump: -dw -Mintel
#name: x86-64 APX ZU instructions with evex prefix encoding(Intel disassembly)
#source: x86-64-apx-zu.s

.*: +file format .*


Disassembly of section .text:

0+ <_start>:
\s*[a-f0-9]+:\s*62 f4 7d 18 6b c3 0a[ 	]+imulzu ax,bx,0xa
\s*[a-f0-9]+:\s*67 62 f4 7d 18 6b 00 0a[ 	]+imulzu ax,WORD PTR \[eax\],0xa
\s*[a-f0-9]+:\s*62 ec 7d 18 6b c1 0a[ 	]+imulzu r16w,r17w,0xa
\s*[a-f0-9]+:\s*67 62 6c 7d 18 6b 38 0a[ 	]+imulzu r31w,WORD PTR \[r16d\],0xa
\s*[a-f0-9]+:\s*62 4c 7d 18 6b ff 0a[ 	]+imulzu r31w,r31w,0xa
\s*[a-f0-9]+:\s*62 f4 7d 18 69 c3 82 23[ 	]+imulzu ax,bx,0x2382
\s*[a-f0-9]+:\s*67 62 f4 7d 18 69 00 82 23[ 	]+imulzu ax,WORD PTR \[eax\],0x2382
\s*[a-f0-9]+:\s*62 ec 7d 18 69 c1 82 23[ 	]+imulzu r16w,r17w,0x2382
\s*[a-f0-9]+:\s*67 62 6c 7d 18 69 38 82 23[ 	]+imulzu r31w,WORD PTR \[r16d\],0x2382
\s*[a-f0-9]+:\s*62 4c 7d 18 69 ff 82 23[ 	]+imulzu r31w,r31w,0x2382
\s*[a-f0-9]+:\s*62 f4 7d 1c 6b c3 0a[ 	]+\{nf\} imulzu ax,bx,0xa
\s*[a-f0-9]+:\s*67 62 f4 7d 1c 6b 00 0a[ 	]+\{nf\} imulzu ax,WORD PTR \[eax\],0xa
\s*[a-f0-9]+:\s*62 f4 7d 0c 6b c3 0a[ 	]+\{nf\} imul ax,bx,0xa
\s*[a-f0-9]+:\s*67 62 f4 7d 0c 6b 00 0a[ 	]+\{nf\} imul ax,WORD PTR \[eax\],0xa
\s*[a-f0-9]+:\s*62 f4 7d 08 6b c3 0a[ 	]+\{evex\} imul ax,bx,0xa
\s*[a-f0-9]+:\s*67 62 f4 7d 08 6b 00 0a[ 	]+\{evex\} imul ax,WORD PTR \[eax\],0xa
\s*[a-f0-9]+:\s*62 f4 7f 18 41 c0[ 	]+setzuno al
\s*[a-f0-9]+:\s*62 f4 7f 18 42 c3[ 	]+setzub bl
\s*[a-f0-9]+:\s*62 d4 7f 18 43 c0[ 	]+setzuae r8b
\s*[a-f0-9]+:\s*62 d4 7f 18 44 c1[ 	]+setzue r9b
\s*[a-f0-9]+:\s*62 d4 7f 18 45 c2[ 	]+setzune r10b
\s*[a-f0-9]+:\s*62 fc 7f 18 46 c0[ 	]+setzube r16b
\s*[a-f0-9]+:\s*62 fc 7f 18 47 c1[ 	]+setzua r17b
\s*[a-f0-9]+:\s*62 fc 7f 18 48 c2[ 	]+setzus r18b
\s*[a-f0-9]+:\s*62 fc 7f 18 49 c3[ 	]+setzuns r19b
\s*[a-f0-9]+:\s*62 fc 7f 18 4a c4[ 	]+setzup r20b
\s*[a-f0-9]+:\s*62 fc 7f 18 4b c5[ 	]+setzunp r21b
\s*[a-f0-9]+:\s*62 fc 7f 18 4c c6[ 	]+setzul r22b
\s*[a-f0-9]+:\s*62 fc 7f 18 4d c7[ 	]+setzuge r23b
\s*[a-f0-9]+:\s*62 dc 7f 18 4e c0[ 	]+setzule r24b
\s*[a-f0-9]+:\s*62 dc 7f 18 4f c1[ 	]+setzug r25b
\s*[a-f0-9]+:\s*d5 91 9f c1[ 	]+setg   r25b
\s*[a-f0-9]+:\s*62 f4 7f 08 4f c0[ 	]+\{evex\} setg al
\s*[a-f0-9]+:\s*62 f4 7f 08 4f 00[ 	]+\{evex\} setg (BYTE PTR )?\[rax\]
\s*[a-f0-9]+:\s*62 f4 7f 18 4a c0[ 	]+setzup al
\s*[a-f0-9]+:\s*62 f4 ff 18 4b c0[ 	]+setzunp al
\s*[a-f0-9]+:\s*62 f4 7d 18 6b c3 0a[ 	]+imulzu ax,bx,0xa
\s*[a-f0-9]+:\s*67 62 f4 7d 18 6b 00 0a[ 	]+imulzu ax,WORD PTR \[eax\],0xa
\s*[a-f0-9]+:\s*62 ec 7d 18 6b c1 0a[ 	]+imulzu r16w,r17w,0xa
\s*[a-f0-9]+:\s*67 62 6c 7d 18 6b 38 0a[ 	]+imulzu r31w,WORD PTR \[r16d\],0xa
\s*[a-f0-9]+:\s*62 4c 7d 18 6b ff 0a[ 	]+imulzu r31w,r31w,0xa
\s*[a-f0-9]+:\s*62 f4 7d 18 69 c3 82 23[ 	]+imulzu ax,bx,0x2382
\s*[a-f0-9]+:\s*67 62 f4 7d 18 69 00 82 23[ 	]+imulzu ax,WORD PTR \[eax\],0x2382
\s*[a-f0-9]+:\s*62 ec 7d 18 69 c1 82 23[ 	]+imulzu r16w,r17w,0x2382
\s*[a-f0-9]+:\s*67 62 6c 7d 18 69 38 82 23[ 	]+imulzu r31w,WORD PTR \[r16d\],0x2382
\s*[a-f0-9]+:\s*62 4c 7d 18 69 ff 82 23[ 	]+imulzu r31w,r31w,0x2382
\s*[a-f0-9]+:\s*62 f4 7d 1c 6b c3 0a[ 	]+\{nf\} imulzu ax,bx,0xa
\s*[a-f0-9]+:\s*67 62 f4 7d 1c 6b 00 0a[ 	]+\{nf\} imulzu ax,WORD PTR \[eax\],0xa
\s*[a-f0-9]+:\s*62 f4 7d 0c 6b c3 0a[ 	]+\{nf\} imul ax,bx,0xa
\s*[a-f0-9]+:\s*67 62 f4 7d 0c 6b 00 0a[ 	]+\{nf\} imul ax,WORD PTR \[eax\],0xa
\s*[a-f0-9]+:\s*62 f4 7d 08 6b c3 0a[ 	]+\{evex\} imul ax,bx,0xa
\s*[a-f0-9]+:\s*67 62 f4 7d 08 6b 00 0a[ 	]+\{evex\} imul ax,WORD PTR \[eax\],0xa
\s*[a-f0-9]+:\s*62 f4 7f 18 41 c0[ 	]+setzuno al
\s*[a-f0-9]+:\s*62 f4 7f 18 42 c3[ 	]+setzub bl
\s*[a-f0-9]+:\s*62 d4 7f 18 43 c0[ 	]+setzuae r8b
\s*[a-f0-9]+:\s*62 d4 7f 18 44 c1[ 	]+setzue r9b
\s*[a-f0-9]+:\s*62 d4 7f 18 45 c2[ 	]+setzune r10b
\s*[a-f0-9]+:\s*62 fc 7f 18 46 c0[ 	]+setzube r16b
\s*[a-f0-9]+:\s*62 fc 7f 18 47 c1[ 	]+setzua r17b
\s*[a-f0-9]+:\s*62 fc 7f 18 48 c2[ 	]+setzus r18b
\s*[a-f0-9]+:\s*62 fc 7f 18 49 c3[ 	]+setzuns r19b
\s*[a-f0-9]+:\s*62 fc 7f 18 4a c4[ 	]+setzup r20b
\s*[a-f0-9]+:\s*62 fc 7f 18 4b c5[ 	]+setzunp r21b
\s*[a-f0-9]+:\s*62 fc 7f 18 4c c6[ 	]+setzul r22b
\s*[a-f0-9]+:\s*62 fc 7f 18 4d c7[ 	]+setzuge r23b
\s*[a-f0-9]+:\s*62 dc 7f 18 4e c0[ 	]+setzule r24b
\s*[a-f0-9]+:\s*62 dc 7f 18 4f c1[ 	]+setzug r25b
\s*[a-f0-9]+:\s*d5 91 9f c1[ 	]+setg   r25b
\s*[a-f0-9]+:\s*62 f4 7f 08 4f c0[ 	]+\{evex\} setg al
#pass
