#as:
#objdump: -dw
#name: x86_64 APX_F insns with nf pseudo prefix
#source: x86-64-apx-nf.s

.*: +file format .*

Disassembly of section \.text:

0+ <_start>:
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 c3 7b\s+\{nf\} add\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 c3 7b\s+\{nf\} add\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 c2 7b\s+\{nf\} add\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 c2 7b\s+\{nf\} add\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 c1 7b\s+\{nf\} add\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 c1 7b\s+\{nf\} add\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 c1 7b\s+\{nf\} add\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 83 c1 7b\s+\{nf\} add\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 84 80 23 01 00 00 7b\s+\{nf\} addb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c 80 84 80 23 01 00 00 7b\s+\{nf\} add\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 84 80 23 01 00 00 7b\s+\{nf\} addw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 84 80 23 01 00 00 7b\s+\{nf\} add\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 84 80 23 01 00 00 7b\s+\{nf\} addl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c 83 84 80 23 01 00 00 7b\s+\{nf\} add\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 84 80 23 01 00 00 7b\s+\{nf\} addq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 84 80 23 01 00 00 7b\s+\{nf\} add\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c 00 da\s+\{nf\} add %bl,%dl
\s*[a-f0-9]+:\s*62 f4 3c 1c 00 da\s+\{nf\} add %bl,%dl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 0c 00 9c 80 23 01 00 00\s+\{nf\} add %bl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 00 9c 80 23 01 00 00\s+\{nf\} add %bl,0x123\(%r8,%rax,4\),%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 01 d0\s+\{nf\} add %dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c 01 d0\s+\{nf\} add %dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c 01 94 80 23 01 00 00\s+\{nf\} add %dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c 01 94 80 23 01 00 00\s+\{nf\} add %dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 01 ca\s+\{nf\} add %ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c 01 ca\s+\{nf\} add %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c 01 8c 80 23 01 00 00\s+\{nf\} add %ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 01 8c 80 23 01 00 00\s+\{nf\} add %ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c 01 cf\s+\{nf\} add %r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c 01 cf\s+\{nf\} add %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c 01 8c 80 23 01 00 00\s+\{nf\} add %r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 01 8c 80 23 01 00 00\s+\{nf\} add %r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 02 9c 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 02 9c 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%bl,%dl
\s*[a-f0-9]+:\s*62 d4 7d 0c 03 94 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 03 94 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%dx,%ax
\s*[a-f0-9]+:\s*62 d4 7c 0c 03 8c 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 03 8c 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%ecx,%edx
\s*[a-f0-9]+:\s*62 54 fc 0c 03 8c 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 54 84 14 03 8c 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%r9,%r31
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 e3 7b\s+\{nf\} and\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 e3 7b\s+\{nf\} and\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 e2 7b\s+\{nf\} and\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 e2 7b\s+\{nf\} and\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 e1 7b\s+\{nf\} and\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 e1 7b\s+\{nf\} and\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 e1 7b\s+\{nf\} and\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 83 e1 7b\s+\{nf\} and\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 a4 80 23 01 00 00 7b\s+\{nf\} andb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c 80 a4 80 23 01 00 00 7b\s+\{nf\} and\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 a4 80 23 01 00 00 7b\s+\{nf\} andw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 a4 80 23 01 00 00 7b\s+\{nf\} and\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 a4 80 23 01 00 00 7b\s+\{nf\} andl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c 83 a4 80 23 01 00 00 7b\s+\{nf\} and\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 a4 80 23 01 00 00 7b\s+\{nf\} andq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 a4 80 23 01 00 00 7b\s+\{nf\} and\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c 20 da\s+\{nf\} and %bl,%dl
\s*[a-f0-9]+:\s*62 f4 3c 1c 20 da\s+\{nf\} and %bl,%dl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 0c 20 9c 80 23 01 00 00\s+\{nf\} and %bl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 20 9c 80 23 01 00 00\s+\{nf\} and %bl,0x123\(%r8,%rax,4\),%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 21 d0\s+\{nf\} and %dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c 21 d0\s+\{nf\} and %dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c 21 94 80 23 01 00 00\s+\{nf\} and %dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c 21 94 80 23 01 00 00\s+\{nf\} and %dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 21 ca\s+\{nf\} and %ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c 21 ca\s+\{nf\} and %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c 21 8c 80 23 01 00 00\s+\{nf\} and %ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 21 8c 80 23 01 00 00\s+\{nf\} and %ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c 21 cf\s+\{nf\} and %r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c 21 cf\s+\{nf\} and %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c 21 8c 80 23 01 00 00\s+\{nf\} and %r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 21 8c 80 23 01 00 00\s+\{nf\} and %r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 22 9c 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 22 9c 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%bl,%dl
\s*[a-f0-9]+:\s*62 d4 7d 0c 23 94 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 23 94 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%dx,%ax
\s*[a-f0-9]+:\s*62 d4 7c 0c 23 8c 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 23 8c 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%ecx,%edx
\s*[a-f0-9]+:\s*62 54 fc 0c 23 8c 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 54 84 14 23 8c 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%r9,%r31
\s*[a-f0-9]+:\s*62 72 6c 0c f2 d1\s+\{nf\} andn %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 52 84 04 f2 d9\s+\{nf\} andn %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 d2 74 0c f2 94 80 23 01 00 00\s+\{nf\} andn 0x123\(%r8,%rax,4\),%ecx,%edx
\s*[a-f0-9]+:\s*62 42 b4 0c f2 bc 80 23 01 00 00\s+\{nf\} andn 0x123\(%r8,%rax,4\),%r9,%r31
\s*[a-f0-9]+:\s*62 72 74 0c f7 d2\s+\{nf\} bextr %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d2 74 0c f7 94 80 23 01 00 00\s+\{nf\} bextr %ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5a b4 0c f7 df\s+\{nf\} bextr %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 42 b4 0c f7 bc 80 23 01 00 00\s+\{nf\} bextr %r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 f2 6c 0c f3 d9\s+\{nf\} blsi %ecx,%edx
\s*[a-f0-9]+:\s*62 d2 84 04 f3 d9\s+\{nf\} blsi %r9,%r31
\s*[a-f0-9]+:\s*62 d2 74 0c f3 9c 80 23 01 00 00\s+\{nf\} blsi 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d2 b4 0c f3 9c 80 23 01 00 00\s+\{nf\} blsi 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f2 6c 0c f3 d1\s+\{nf\} blsmsk %ecx,%edx
\s*[a-f0-9]+:\s*62 d2 84 04 f3 d1\s+\{nf\} blsmsk %r9,%r31
\s*[a-f0-9]+:\s*62 d2 74 0c f3 94 80 23 01 00 00\s+\{nf\} blsmsk 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d2 b4 0c f3 94 80 23 01 00 00\s+\{nf\} blsmsk 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f2 6c 0c f3 c9\s+\{nf\} blsr %ecx,%edx
\s*[a-f0-9]+:\s*62 d2 84 04 f3 c9\s+\{nf\} blsr %r9,%r31
\s*[a-f0-9]+:\s*62 d2 74 0c f3 8c 80 23 01 00 00\s+\{nf\} blsr 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d2 b4 0c f3 8c 80 23 01 00 00\s+\{nf\} blsr 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 72 74 0c f5 d2\s+\{nf\} bzhi %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d2 74 0c f5 94 80 23 01 00 00\s+\{nf\} bzhi %ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5a b4 0c f5 df\s+\{nf\} bzhi %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 42 b4 0c f5 bc 80 23 01 00 00\s+\{nf\} bzhi %r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 4c fc 0c 31 ff\s+\{nf\} xor %r31,%r31
\s*[a-f0-9]+:\s*62 f4 7c 0c fe cb\s+\{nf\} dec %bl
\s*[a-f0-9]+:\s*62 f4 6c 1c fe cb\s+\{nf\} dec %bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c ff ca\s+\{nf\} dec %dx
\s*[a-f0-9]+:\s*62 f4 7d 1c ff ca\s+\{nf\} dec %dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c ff c9\s+\{nf\} dec %ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c ff c9\s+\{nf\} dec %ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c ff c9\s+\{nf\} dec %r9
\s*[a-f0-9]+:\s*62 d4 84 14 ff c9\s+\{nf\} dec %r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c fe 8c 80 23 01 00 00\s+\{nf\} decb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c fe 8c 80 23 01 00 00\s+\{nf\} dec 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c ff 8c 80 23 01 00 00\s+\{nf\} decw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c ff 8c 80 23 01 00 00\s+\{nf\} dec 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c ff 8c 80 23 01 00 00\s+\{nf\} decl 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c ff 8c 80 23 01 00 00\s+\{nf\} dec 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c ff 8c 80 23 01 00 00\s+\{nf\} decq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c ff 8c 80 23 01 00 00\s+\{nf\} dec 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 f3\s+\{nf\} div %bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 f2\s+\{nf\} div %dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 f1\s+\{nf\} div %ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 f1\s+\{nf\} div %r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 b4 80 23 01 00 00\s+\{nf\} divb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 b4 80 23 01 00 00\s+\{nf\} divw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 b4 80 23 01 00 00\s+\{nf\} divl 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 b4 80 23 01 00 00\s+\{nf\} divq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 fb\s+\{nf\} idiv %bl
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 fb\s+\{nf\} idiv %bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 fa\s+\{nf\} idiv %dx
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 fa\s+\{nf\} idiv %dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 f9\s+\{nf\} idiv %ecx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 f9\s+\{nf\} idiv %ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 f9\s+\{nf\} idiv %r9
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 f9\s+\{nf\} idiv %r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 bc 80 23 01 00 00\s+\{nf\} idivb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 bc 80 23 01 00 00\s+\{nf\} idivb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 bc 80 23 01 00 00\s+\{nf\} idivw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 bc 80 23 01 00 00\s+\{nf\} idivw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 bc 80 23 01 00 00\s+\{nf\} idivl 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 bc 80 23 01 00 00\s+\{nf\} idivl 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 bc 80 23 01 00 00\s+\{nf\} idivq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 bc 80 23 01 00 00\s+\{nf\} idivq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 eb\s+\{nf\} imul %bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 ea\s+\{nf\} imul %dx
\s*[a-f0-9]+:\s*62 f4 7d 0c af c2\s+\{nf\} imul %dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c af c2\s+\{nf\} imul %dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 e9\s+\{nf\} imul %ecx
\s*[a-f0-9]+:\s*62 f4 7c 0c af d1\s+\{nf\} imul %ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c af d1\s+\{nf\} imul %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 e9\s+\{nf\} imul %r9
\s*[a-f0-9]+:\s*62 44 fc 0c af f9\s+\{nf\} imul %r9,%r31
\s*[a-f0-9]+:\s*62 44 a4 1c af f9\s+\{nf\} imul %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 ac 80 23 01 00 00\s+\{nf\} imulb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 ac 80 23 01 00 00\s+\{nf\} imulw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 0c af 94 80 23 01 00 00\s+\{nf\} imul 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7d 1c af 94 80 23 01 00 00\s+\{nf\} imul 0x123\(%r8,%rax,4\),%dx,%ax
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 ac 80 23 01 00 00\s+\{nf\} imull 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 0c af 8c 80 23 01 00 00\s+\{nf\} imul 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c af 8c 80 23 01 00 00\s+\{nf\} imul 0x123\(%r8,%rax,4\),%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 ac 80 23 01 00 00\s+\{nf\} imulq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 0c af 8c 80 23 01 00 00\s+\{nf\} imul 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 54 84 14 af 8c 80 23 01 00 00\s+\{nf\} imul 0x123\(%r8,%rax,4\),%r9,%r31
\s*[a-f0-9]+:\s*62 f4 7d 0c 6b c2 7b\s+\{nf\} imul \$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 6b d1 7b\s+\{nf\} imul \$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 54 fc 0c 6b f9 7b\s+\{nf\} imul \$0x7b,%r9,%r15
\s*[a-f0-9]+:\s*62 54 fc 0c 6b c9 7b\s+\{nf\} imul \$0x7b,%r9,%r9
\s*[a-f0-9]+:\s*62 d4 7d 0c 6b 94 80 23 01 00 00 7b\s+\{nf\} imul \$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c 6b 8c 80 23 01 00 00 7b\s+\{nf\} imul \$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 54 fc 0c 6b 8c 80 23 01 00 00 7b\s+\{nf\} imul \$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7d 0c 6b c2 90\s+\{nf\} imul \$0xff90,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 69 d1 90 ff 00 00\s+\{nf\} imul \$0xff90,%ecx,%edx
\s*[a-f0-9]+:\s*62 54 fc 0c 69 f9 90 ff 00 00\s+\{nf\} imul \$0xff90,%r9,%r15
\s*[a-f0-9]+:\s*62 54 fc 0c 69 c9 90 ff 00 00\s+\{nf\} imul \$0xff90,%r9,%r9
\s*[a-f0-9]+:\s*62 d4 7d 0c 6b 94 80 23 01 00 00 90\s+\{nf\} imul \$0xff90,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c 69 8c 80 23 01 00 00 90 ff 00 00\s+\{nf\} imul \$0xff90,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 54 fc 0c 69 8c 80 23 01 00 00 90 ff 00 00\s+\{nf\} imul \$0xff90,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c fe c3\s+\{nf\} inc %bl
\s*[a-f0-9]+:\s*62 f4 6c 1c fe c3\s+\{nf\} inc %bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c ff c2\s+\{nf\} inc %dx
\s*[a-f0-9]+:\s*62 f4 7d 1c ff c2\s+\{nf\} inc %dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c ff c1\s+\{nf\} inc %ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c ff c1\s+\{nf\} inc %ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c ff c1\s+\{nf\} inc %r9
\s*[a-f0-9]+:\s*62 d4 84 14 ff c1\s+\{nf\} inc %r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c fe 84 80 23 01 00 00\s+\{nf\} incb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c fe 84 80 23 01 00 00\s+\{nf\} inc 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c ff 84 80 23 01 00 00\s+\{nf\} incw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c ff 84 80 23 01 00 00\s+\{nf\} inc 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c ff 84 80 23 01 00 00\s+\{nf\} incl 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c ff 84 80 23 01 00 00\s+\{nf\} inc 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c ff 84 80 23 01 00 00\s+\{nf\} incq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c ff 84 80 23 01 00 00\s+\{nf\} inc 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7d 0c f5 c2\s+\{nf\} lzcnt %dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c f5 d1\s+\{nf\} lzcnt %ecx,%edx
\s*[a-f0-9]+:\s*62 44 fc 0c f5 f9\s+\{nf\} lzcnt %r9,%r31
\s*[a-f0-9]+:\s*62 d4 7d 0c f5 94 80 23 01 00 00\s+\{nf\} lzcnt 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c f5 8c 80 23 01 00 00\s+\{nf\} lzcnt 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 54 fc 0c f5 8c 80 23 01 00 00\s+\{nf\} lzcnt 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 e3\s+\{nf\} mul %bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 e2\s+\{nf\} mul %dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 e1\s+\{nf\} mul %ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 e1\s+\{nf\} mul %r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 a4 80 23 01 00 00\s+\{nf\} mulb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 a4 80 23 01 00 00\s+\{nf\} mulw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 a4 80 23 01 00 00\s+\{nf\} mull 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 a4 80 23 01 00 00\s+\{nf\} mulq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 db\s+\{nf\} neg %bl
\s*[a-f0-9]+:\s*62 f4 6c 1c f6 db\s+\{nf\} neg %bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 da\s+\{nf\} neg %dx
\s*[a-f0-9]+:\s*62 f4 7d 1c f7 da\s+\{nf\} neg %dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 d9\s+\{nf\} neg %ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c f7 d9\s+\{nf\} neg %ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 d9\s+\{nf\} neg %r9
\s*[a-f0-9]+:\s*62 d4 84 14 f7 d9\s+\{nf\} neg %r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 9c 80 23 01 00 00\s+\{nf\} negb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c f6 9c 80 23 01 00 00\s+\{nf\} neg 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 9c 80 23 01 00 00\s+\{nf\} negw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c f7 9c 80 23 01 00 00\s+\{nf\} neg 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 9c 80 23 01 00 00\s+\{nf\} negl 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c f7 9c 80 23 01 00 00\s+\{nf\} neg 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 9c 80 23 01 00 00\s+\{nf\} negq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c f7 9c 80 23 01 00 00\s+\{nf\} neg 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 cb 7b\s+\{nf\} or\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 cb 7b\s+\{nf\} or\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 ca 7b\s+\{nf\} or\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 ca 7b\s+\{nf\} or\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 c9 7b\s+\{nf\} or\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 c9 7b\s+\{nf\} or\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 c9 7b\s+\{nf\} or\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 83 c9 7b\s+\{nf\} or\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 8c 80 23 01 00 00 7b\s+\{nf\} orb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c 80 8c 80 23 01 00 00 7b\s+\{nf\} or\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 8c 80 23 01 00 00 7b\s+\{nf\} orw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 8c 80 23 01 00 00 7b\s+\{nf\} or\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 8c 80 23 01 00 00 7b\s+\{nf\} orl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c 83 8c 80 23 01 00 00 7b\s+\{nf\} or\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 8c 80 23 01 00 00 7b\s+\{nf\} orq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 8c 80 23 01 00 00 7b\s+\{nf\} or\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c 08 da\s+\{nf\} or %bl,%dl
\s*[a-f0-9]+:\s*62 f4 3c 1c 08 da\s+\{nf\} or %bl,%dl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 0c 08 9c 80 23 01 00 00\s+\{nf\} or %bl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 08 9c 80 23 01 00 00\s+\{nf\} or %bl,0x123\(%r8,%rax,4\),%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 09 d0\s+\{nf\} or %dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c 09 d0\s+\{nf\} or %dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c 09 94 80 23 01 00 00\s+\{nf\} or %dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c 09 94 80 23 01 00 00\s+\{nf\} or %dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 09 ca\s+\{nf\} or %ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c 09 ca\s+\{nf\} or %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c 09 8c 80 23 01 00 00\s+\{nf\} or %ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 09 8c 80 23 01 00 00\s+\{nf\} or %ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c 09 cf\s+\{nf\} or %r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c 09 cf\s+\{nf\} or %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c 09 8c 80 23 01 00 00\s+\{nf\} or %r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 09 8c 80 23 01 00 00\s+\{nf\} or %r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 0a 9c 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 0a 9c 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%bl,%dl
\s*[a-f0-9]+:\s*62 d4 7d 0c 0b 94 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 0b 94 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%dx,%ax
\s*[a-f0-9]+:\s*62 d4 7c 0c 0b 8c 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 0b 8c 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%ecx,%edx
\s*[a-f0-9]+:\s*62 54 fc 0c 0b 8c 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 54 84 14 0b 8c 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%r9,%r31
\s*[a-f0-9]+:\s*62 f4 7d 0c 88 c2\s+\{nf\} popcnt %dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 88 d1\s+\{nf\} popcnt %ecx,%edx
\s*[a-f0-9]+:\s*62 44 fc 0c 88 f9\s+\{nf\} popcnt %r9,%r31
\s*[a-f0-9]+:\s*62 d4 7d 0c 88 94 80 23 01 00 00\s+\{nf\} popcnt 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c 88 8c 80 23 01 00 00\s+\{nf\} popcnt 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 54 fc 0c 88 8c 80 23 01 00 00\s+\{nf\} popcnt 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 c3\s+\{nf\} rol\s+\$1,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 c3\s+\{nf\} rol\s+\$1,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 c2\s+\{nf\} rol\s+\$1,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 c2\s+\{nf\} rol\s+\$1,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 c1\s+\{nf\} rol\s+\$1,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 c1\s+\{nf\} rol\s+\$1,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 c1\s+\{nf\} rol\s+\$1,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d1 c1\s+\{nf\} rol\s+\$1,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 84 80 23 01 00 00\s+\{nf\} rolb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d0 84 80 23 01 00 00\s+\{nf\} rol\s+\$1,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 84 80 23 01 00 00\s+\{nf\} rolw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 84 80 23 01 00 00\s+\{nf\} rol\s+\$1,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 84 80 23 01 00 00\s+\{nf\} roll\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d1 84 80 23 01 00 00\s+\{nf\} rol\s+\$1,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 84 80 23 01 00 00\s+\{nf\} rolq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 84 80 23 01 00 00\s+\{nf\} rol\s+\$1,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 c3 7b\s+\{nf\} rol\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 c3 7b\s+\{nf\} rol\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 c2 7b\s+\{nf\} rol\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 c2 7b\s+\{nf\} rol\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 c1 7b\s+\{nf\} rol\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 c1 7b\s+\{nf\} rol\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 c1 7b\s+\{nf\} rol\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 c1 c1 7b\s+\{nf\} rol\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 84 80 23 01 00 00 7b\s+\{nf\} rolb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c c0 84 80 23 01 00 00 7b\s+\{nf\} rol\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 84 80 23 01 00 00 7b\s+\{nf\} rolw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 84 80 23 01 00 00 7b\s+\{nf\} rol\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 84 80 23 01 00 00 7b\s+\{nf\} roll\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c c1 84 80 23 01 00 00 7b\s+\{nf\} rol\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 84 80 23 01 00 00 7b\s+\{nf\} rolq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 84 80 23 01 00 00 7b\s+\{nf\} rol\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 c3\s+\{nf\} rol %cl,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 c3\s+\{nf\} rol %cl,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 c2\s+\{nf\} rol %cl,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 c2\s+\{nf\} rol %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 c1\s+\{nf\} rol %cl,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 c1\s+\{nf\} rol %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 c1\s+\{nf\} rol %cl,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d3 c1\s+\{nf\} rol %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 84 80 23 01 00 00\s+\{nf\} rolb %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d2 84 80 23 01 00 00\s+\{nf\} rol %cl,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 84 80 23 01 00 00\s+\{nf\} rolw %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 84 80 23 01 00 00\s+\{nf\} rol %cl,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 84 80 23 01 00 00\s+\{nf\} roll %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d3 84 80 23 01 00 00\s+\{nf\} rol %cl,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 84 80 23 01 00 00\s+\{nf\} rolq %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 84 80 23 01 00 00\s+\{nf\} rol %cl,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 cb\s+\{nf\} ror\s+\$1,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 cb\s+\{nf\} ror\s+\$1,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 ca\s+\{nf\} ror\s+\$1,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 ca\s+\{nf\} ror\s+\$1,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 c9\s+\{nf\} ror\s+\$1,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 c9\s+\{nf\} ror\s+\$1,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 c9\s+\{nf\} ror\s+\$1,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d1 c9\s+\{nf\} ror\s+\$1,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 8c 80 23 01 00 00\s+\{nf\} rorb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d0 8c 80 23 01 00 00\s+\{nf\} ror\s+\$1,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 8c 80 23 01 00 00\s+\{nf\} rorw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 8c 80 23 01 00 00\s+\{nf\} ror\s+\$1,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 8c 80 23 01 00 00\s+\{nf\} rorl\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d1 8c 80 23 01 00 00\s+\{nf\} ror\s+\$1,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 8c 80 23 01 00 00\s+\{nf\} rorq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 8c 80 23 01 00 00\s+\{nf\} ror\s+\$1,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 cb 7b\s+\{nf\} ror\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 cb 7b\s+\{nf\} ror\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 ca 7b\s+\{nf\} ror\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 ca 7b\s+\{nf\} ror\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 c9 7b\s+\{nf\} ror\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 c9 7b\s+\{nf\} ror\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 c9 7b\s+\{nf\} ror\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 c1 c9 7b\s+\{nf\} ror\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 8c 80 23 01 00 00 7b\s+\{nf\} rorb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c c0 8c 80 23 01 00 00 7b\s+\{nf\} ror\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 8c 80 23 01 00 00 7b\s+\{nf\} rorw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 8c 80 23 01 00 00 7b\s+\{nf\} rorl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 8c 80 23 01 00 00 7b\s+\{nf\} rorq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 cb\s+\{nf\} ror %cl,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 cb\s+\{nf\} ror %cl,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 ca\s+\{nf\} ror %cl,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 ca\s+\{nf\} ror %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 c9\s+\{nf\} ror %cl,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 c9\s+\{nf\} ror %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 c9\s+\{nf\} ror %cl,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d3 c9\s+\{nf\} ror %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 8c 80 23 01 00 00\s+\{nf\} rorb %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d2 8c 80 23 01 00 00\s+\{nf\} ror %cl,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 8c 80 23 01 00 00\s+\{nf\} rorw %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 8c 80 23 01 00 00\s+\{nf\} ror %cl,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 8c 80 23 01 00 00\s+\{nf\} rorl %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d3 8c 80 23 01 00 00\s+\{nf\} ror %cl,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 8c 80 23 01 00 00\s+\{nf\} rorq %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 8c 80 23 01 00 00\s+\{nf\} ror %cl,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 e3\s+\{nf\} shl\s+\$1,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 e3\s+\{nf\} shl\s+\$1,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 e2\s+\{nf\} shl\s+\$1,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 e2\s+\{nf\} shl\s+\$1,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 e1\s+\{nf\} shl\s+\$1,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 e1\s+\{nf\} shl\s+\$1,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 e1\s+\{nf\} shl\s+\$1,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d1 e1\s+\{nf\} shl\s+\$1,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 a4 80 23 01 00 00\s+\{nf\} shlb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d0 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 a4 80 23 01 00 00\s+\{nf\} shlw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 a4 80 23 01 00 00\s+\{nf\} shll\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d1 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 a4 80 23 01 00 00\s+\{nf\} shlq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 e3 7b\s+\{nf\} shl\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 e3 7b\s+\{nf\} shl\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 e2 7b\s+\{nf\} shl\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 e2 7b\s+\{nf\} shl\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 a4 80 23 01 00 00 7b\s+\{nf\} shlb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c c0 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shlw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shll\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shlq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 e3\s+\{nf\} shl %cl,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 e3\s+\{nf\} shl %cl,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 e2\s+\{nf\} shl %cl,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 e2\s+\{nf\} shl %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 e1\s+\{nf\} shl %cl,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 e1\s+\{nf\} shl %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 e1\s+\{nf\} shl %cl,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d3 e1\s+\{nf\} shl %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 a4 80 23 01 00 00\s+\{nf\} shlb %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d2 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 a4 80 23 01 00 00\s+\{nf\} shlw %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 a4 80 23 01 00 00\s+\{nf\} shll %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d3 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 a4 80 23 01 00 00\s+\{nf\} shlq %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 fb\s+\{nf\} sar\s+\$1,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 fb\s+\{nf\} sar\s+\$1,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 fa\s+\{nf\} sar\s+\$1,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 fa\s+\{nf\} sar\s+\$1,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 f9\s+\{nf\} sar\s+\$1,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 f9\s+\{nf\} sar\s+\$1,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 f9\s+\{nf\} sar\s+\$1,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d1 f9\s+\{nf\} sar\s+\$1,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 bc 80 23 01 00 00\s+\{nf\} sarb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d0 bc 80 23 01 00 00\s+\{nf\} sar\s+\$1,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 bc 80 23 01 00 00\s+\{nf\} sarw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 bc 80 23 01 00 00\s+\{nf\} sar\s+\$1,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 bc 80 23 01 00 00\s+\{nf\} sarl\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d1 bc 80 23 01 00 00\s+\{nf\} sar\s+\$1,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 bc 80 23 01 00 00\s+\{nf\} sarq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 bc 80 23 01 00 00\s+\{nf\} sar\s+\$1,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 fb 7b\s+\{nf\} sar\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 fb 7b\s+\{nf\} sar\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 fa 7b\s+\{nf\} sar\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 fa 7b\s+\{nf\} sar\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 f9 7b\s+\{nf\} sar\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 f9 7b\s+\{nf\} sar\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 f9 7b\s+\{nf\} sar\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 c1 f9 7b\s+\{nf\} sar\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 bc 80 23 01 00 00 7b\s+\{nf\} sarb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c c0 bc 80 23 01 00 00 7b\s+\{nf\} sar\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 bc 80 23 01 00 00 7b\s+\{nf\} sarw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 bc 80 23 01 00 00 7b\s+\{nf\} sarl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 bc 80 23 01 00 00 7b\s+\{nf\} sarq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 fb\s+\{nf\} sar %cl,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 fb\s+\{nf\} sar %cl,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 fa\s+\{nf\} sar %cl,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 fa\s+\{nf\} sar %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 f9\s+\{nf\} sar %cl,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 f9\s+\{nf\} sar %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 f9\s+\{nf\} sar %cl,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d3 f9\s+\{nf\} sar %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 bc 80 23 01 00 00\s+\{nf\} sarb %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d2 bc 80 23 01 00 00\s+\{nf\} sar %cl,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 bc 80 23 01 00 00\s+\{nf\} sarw %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 bc 80 23 01 00 00\s+\{nf\} sar %cl,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 bc 80 23 01 00 00\s+\{nf\} sarl %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d3 bc 80 23 01 00 00\s+\{nf\} sar %cl,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 bc 80 23 01 00 00\s+\{nf\} sarq %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 bc 80 23 01 00 00\s+\{nf\} sar %cl,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 e3\s+\{nf\} shl\s+\$1,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 e3\s+\{nf\} shl\s+\$1,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 e2\s+\{nf\} shl\s+\$1,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 e2\s+\{nf\} shl\s+\$1,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 e1\s+\{nf\} shl\s+\$1,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 e1\s+\{nf\} shl\s+\$1,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 e1\s+\{nf\} shl\s+\$1,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d1 e1\s+\{nf\} shl\s+\$1,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 a4 80 23 01 00 00\s+\{nf\} shlb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d0 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 a4 80 23 01 00 00\s+\{nf\} shlw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 a4 80 23 01 00 00\s+\{nf\} shll\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d1 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 a4 80 23 01 00 00\s+\{nf\} shlq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 e3 7b\s+\{nf\} shl\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 e3 7b\s+\{nf\} shl\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 e2 7b\s+\{nf\} shl\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 e2 7b\s+\{nf\} shl\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 a4 80 23 01 00 00 7b\s+\{nf\} shlb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c c0 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shlw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shll\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shlq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 e3\s+\{nf\} shl %cl,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 e3\s+\{nf\} shl %cl,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 e2\s+\{nf\} shl %cl,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 e2\s+\{nf\} shl %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 e1\s+\{nf\} shl %cl,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 e1\s+\{nf\} shl %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 e1\s+\{nf\} shl %cl,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d3 e1\s+\{nf\} shl %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 a4 80 23 01 00 00\s+\{nf\} shlb %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d2 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 a4 80 23 01 00 00\s+\{nf\} shlw %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 a4 80 23 01 00 00\s+\{nf\} shll %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d3 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 a4 80 23 01 00 00\s+\{nf\} shlq %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7d 0c 24 d0 7b\s+\{nf\} shld\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c 24 d0 7b\s+\{nf\} shld\s+\$0x7b,%dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c 24 94 80 23 01 00 00 7b\s+\{nf\} shld\s+\$0x7b,%dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c 24 94 80 23 01 00 00 7b\s+\{nf\} shld\s+\$0x7b,%dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 24 ca 7b\s+\{nf\} shld\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c 24 ca 7b\s+\{nf\} shld\s+\$0x7b,%ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c 24 8c 80 23 01 00 00 7b\s+\{nf\} shld\s+\$0x7b,%ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 24 8c 80 23 01 00 00 7b\s+\{nf\} shld\s+\$0x7b,%ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c 24 cf 7b\s+\{nf\} shld\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c 24 cf 7b\s+\{nf\} shld\s+\$0x7b,%r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c 24 8c 80 23 01 00 00 7b\s+\{nf\} shld\s+\$0x7b,%r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 24 8c 80 23 01 00 00 7b\s+\{nf\} shld\s+\$0x7b,%r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 f4 7d 0c a5 d0\s+\{nf\} shld %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c a5 d0\s+\{nf\} shld %cl,%dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c a5 94 80 23 01 00 00\s+\{nf\} shld %cl,%dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c a5 94 80 23 01 00 00\s+\{nf\} shld %cl,%dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c a5 ca\s+\{nf\} shld %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c a5 ca\s+\{nf\} shld %cl,%ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c a5 8c 80 23 01 00 00\s+\{nf\} shld %cl,%ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c a5 8c 80 23 01 00 00\s+\{nf\} shld %cl,%ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c a5 cf\s+\{nf\} shld %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c a5 cf\s+\{nf\} shld %cl,%r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c a5 8c 80 23 01 00 00\s+\{nf\} shld %cl,%r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 a5 8c 80 23 01 00 00\s+\{nf\} shld %cl,%r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 eb\s+\{nf\} shr\s+\$1,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 eb\s+\{nf\} shr\s+\$1,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 ea\s+\{nf\} shr\s+\$1,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 ea\s+\{nf\} shr\s+\$1,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 e9\s+\{nf\} shr\s+\$1,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 e9\s+\{nf\} shr\s+\$1,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 e9\s+\{nf\} shr\s+\$1,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d1 e9\s+\{nf\} shr\s+\$1,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 ac 80 23 01 00 00\s+\{nf\} shrb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d0 ac 80 23 01 00 00\s+\{nf\} shr\s+\$1,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 ac 80 23 01 00 00\s+\{nf\} shrw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 ac 80 23 01 00 00\s+\{nf\} shr\s+\$1,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 ac 80 23 01 00 00\s+\{nf\} shrl\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d1 ac 80 23 01 00 00\s+\{nf\} shr\s+\$1,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 ac 80 23 01 00 00\s+\{nf\} shrq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 ac 80 23 01 00 00\s+\{nf\} shr\s+\$1,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 eb 7b\s+\{nf\} shr\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 eb 7b\s+\{nf\} shr\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 ea 7b\s+\{nf\} shr\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 ea 7b\s+\{nf\} shr\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 e9 7b\s+\{nf\} shr\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 e9 7b\s+\{nf\} shr\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 e9 7b\s+\{nf\} shr\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 c1 e9 7b\s+\{nf\} shr\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 ac 80 23 01 00 00 7b\s+\{nf\} shrb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c c0 ac 80 23 01 00 00 7b\s+\{nf\} shr\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 ac 80 23 01 00 00 7b\s+\{nf\} shrw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 ac 80 23 01 00 00 7b\s+\{nf\} shrl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 ac 80 23 01 00 00 7b\s+\{nf\} shrq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 eb\s+\{nf\} shr %cl,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 eb\s+\{nf\} shr %cl,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 ea\s+\{nf\} shr %cl,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 ea\s+\{nf\} shr %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 e9\s+\{nf\} shr %cl,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 e9\s+\{nf\} shr %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 e9\s+\{nf\} shr %cl,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d3 e9\s+\{nf\} shr %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 ac 80 23 01 00 00\s+\{nf\} shrb %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d2 ac 80 23 01 00 00\s+\{nf\} shr %cl,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 ac 80 23 01 00 00\s+\{nf\} shrw %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 ac 80 23 01 00 00\s+\{nf\} shr %cl,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 ac 80 23 01 00 00\s+\{nf\} shrl %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d3 ac 80 23 01 00 00\s+\{nf\} shr %cl,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 ac 80 23 01 00 00\s+\{nf\} shrq %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 ac 80 23 01 00 00\s+\{nf\} shr %cl,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7d 0c 2c d0 7b\s+\{nf\} shrd\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c 2c d0 7b\s+\{nf\} shrd\s+\$0x7b,%dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c 2c 94 80 23 01 00 00 7b\s+\{nf\} shrd\s+\$0x7b,%dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c 2c 94 80 23 01 00 00 7b\s+\{nf\} shrd\s+\$0x7b,%dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 2c ca 7b\s+\{nf\} shrd\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c 2c ca 7b\s+\{nf\} shrd\s+\$0x7b,%ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd\s+\$0x7b,%ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd\s+\$0x7b,%ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c 2c cf 7b\s+\{nf\} shrd\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c 2c cf 7b\s+\{nf\} shrd\s+\$0x7b,%r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd\s+\$0x7b,%r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd\s+\$0x7b,%r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 f4 7d 0c ad d0\s+\{nf\} shrd %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c ad d0\s+\{nf\} shrd %cl,%dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c ad 94 80 23 01 00 00\s+\{nf\} shrd %cl,%dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c ad 94 80 23 01 00 00\s+\{nf\} shrd %cl,%dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c ad ca\s+\{nf\} shrd %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c ad ca\s+\{nf\} shrd %cl,%ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c ad 8c 80 23 01 00 00\s+\{nf\} shrd %cl,%ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c ad 8c 80 23 01 00 00\s+\{nf\} shrd %cl,%ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c ad cf\s+\{nf\} shrd %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c ad cf\s+\{nf\} shrd %cl,%r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c ad 8c 80 23 01 00 00\s+\{nf\} shrd %cl,%r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 ad 8c 80 23 01 00 00\s+\{nf\} shrd %cl,%r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 eb 7b\s+\{nf\} sub\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 eb 7b\s+\{nf\} sub\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 ea 7b\s+\{nf\} sub\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 ea 7b\s+\{nf\} sub\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 e9 7b\s+\{nf\} sub\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 e9 7b\s+\{nf\} sub\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 e9 7b\s+\{nf\} sub\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 83 e9 7b\s+\{nf\} sub\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 ac 80 23 01 00 00 7b\s+\{nf\} subb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c 80 ac 80 23 01 00 00 7b\s+\{nf\} sub\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 ac 80 23 01 00 00 7b\s+\{nf\} subw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 ac 80 23 01 00 00 7b\s+\{nf\} subl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 ac 80 23 01 00 00 7b\s+\{nf\} subq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c 28 da\s+\{nf\} sub %bl,%dl
\s*[a-f0-9]+:\s*62 f4 3c 1c 28 da\s+\{nf\} sub %bl,%dl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 0c 28 9c 80 23 01 00 00\s+\{nf\} sub %bl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 28 9c 80 23 01 00 00\s+\{nf\} sub %bl,0x123\(%r8,%rax,4\),%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 29 d0\s+\{nf\} sub %dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c 29 d0\s+\{nf\} sub %dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c 29 94 80 23 01 00 00\s+\{nf\} sub %dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c 29 94 80 23 01 00 00\s+\{nf\} sub %dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 29 ca\s+\{nf\} sub %ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c 29 ca\s+\{nf\} sub %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c 29 8c 80 23 01 00 00\s+\{nf\} sub %ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 29 8c 80 23 01 00 00\s+\{nf\} sub %ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c 29 cf\s+\{nf\} sub %r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c 29 cf\s+\{nf\} sub %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c 29 8c 80 23 01 00 00\s+\{nf\} sub %r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 29 8c 80 23 01 00 00\s+\{nf\} sub %r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 2a 9c 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 2a 9c 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%bl,%dl
\s*[a-f0-9]+:\s*62 d4 7d 0c 2b 94 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 2b 94 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%dx,%ax
\s*[a-f0-9]+:\s*62 d4 7c 0c 2b 8c 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 2b 8c 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%ecx,%edx
\s*[a-f0-9]+:\s*62 54 fc 0c 2b 8c 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 54 84 14 2b 8c 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%r9,%r31
\s*[a-f0-9]+:\s*62 f4 7d 0c f4 c2\s+\{nf\} tzcnt %dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c f4 d1\s+\{nf\} tzcnt %ecx,%edx
\s*[a-f0-9]+:\s*62 44 fc 0c f4 f9\s+\{nf\} tzcnt %r9,%r31
\s*[a-f0-9]+:\s*62 d4 7d 0c f4 94 80 23 01 00 00\s+\{nf\} tzcnt 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c f4 8c 80 23 01 00 00\s+\{nf\} tzcnt 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 54 fc 0c f4 8c 80 23 01 00 00\s+\{nf\} tzcnt 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 f3 7b\s+\{nf\} xor\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 f3 7b\s+\{nf\} xor\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 f2 7b\s+\{nf\} xor\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 f2 7b\s+\{nf\} xor\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 f1 7b\s+\{nf\} xor\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 f1 7b\s+\{nf\} xor\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 f1 7b\s+\{nf\} xor\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 83 f1 7b\s+\{nf\} xor\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 b4 80 23 01 00 00 7b\s+\{nf\} xorb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c 80 b4 80 23 01 00 00 7b\s+\{nf\} xor\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 b4 80 23 01 00 00 7b\s+\{nf\} xorw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 b4 80 23 01 00 00 7b\s+\{nf\} xorl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 b4 80 23 01 00 00 7b\s+\{nf\} xorq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c 30 da\s+\{nf\} xor %bl,%dl
\s*[a-f0-9]+:\s*62 f4 3c 1c 30 da\s+\{nf\} xor %bl,%dl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 0c 30 9c 80 23 01 00 00\s+\{nf\} xor %bl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 30 9c 80 23 01 00 00\s+\{nf\} xor %bl,0x123\(%r8,%rax,4\),%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 31 d0\s+\{nf\} xor %dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c 31 d0\s+\{nf\} xor %dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c 31 94 80 23 01 00 00\s+\{nf\} xor %dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c 31 94 80 23 01 00 00\s+\{nf\} xor %dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 31 ca\s+\{nf\} xor %ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c 31 ca\s+\{nf\} xor %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c 31 8c 80 23 01 00 00\s+\{nf\} xor %ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 31 8c 80 23 01 00 00\s+\{nf\} xor %ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c 31 cf\s+\{nf\} xor %r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c 31 cf\s+\{nf\} xor %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c 31 8c 80 23 01 00 00\s+\{nf\} xor %r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 31 8c 80 23 01 00 00\s+\{nf\} xor %r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 32 9c 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 32 9c 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%bl,%dl
\s*[a-f0-9]+:\s*62 d4 7d 0c 33 94 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 33 94 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%dx,%ax
\s*[a-f0-9]+:\s*62 d4 7c 0c 33 8c 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 33 8c 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%ecx,%edx
\s*[a-f0-9]+:\s*62 54 fc 0c 33 8c 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 54 84 14 33 8c 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%r9,%r31

0[0-9a-f]+ <intel>:
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 c3 7b\s+\{nf\} add\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 c3 7b\s+\{nf\} add\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 c2 7b\s+\{nf\} add\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 c2 7b\s+\{nf\} add\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 c1 7b\s+\{nf\} add\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 c1 7b\s+\{nf\} add\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 c1 7b\s+\{nf\} add\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 83 c1 7b\s+\{nf\} add\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 84 80 23 01 00 00 7b\s+\{nf\} addb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c 80 84 80 23 01 00 00 7b\s+\{nf\} add\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 84 80 23 01 00 00 7b\s+\{nf\} addw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 84 80 23 01 00 00 7b\s+\{nf\} add\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 84 80 23 01 00 00 7b\s+\{nf\} addl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c 83 84 80 23 01 00 00 7b\s+\{nf\} add\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 84 80 23 01 00 00 7b\s+\{nf\} addq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 84 80 23 01 00 00 7b\s+\{nf\} add\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c 00 da\s+\{nf\} add %bl,%dl
\s*[a-f0-9]+:\s*62 f4 3c 1c 00 da\s+\{nf\} add %bl,%dl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 0c 00 9c 80 23 01 00 00\s+\{nf\} add %bl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 00 9c 80 23 01 00 00\s+\{nf\} add %bl,0x123\(%r8,%rax,4\),%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 01 d0\s+\{nf\} add %dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c 01 d0\s+\{nf\} add %dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c 01 94 80 23 01 00 00\s+\{nf\} add %dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c 01 94 80 23 01 00 00\s+\{nf\} add %dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 01 ca\s+\{nf\} add %ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c 01 ca\s+\{nf\} add %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c 01 8c 80 23 01 00 00\s+\{nf\} add %ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 01 8c 80 23 01 00 00\s+\{nf\} add %ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c 01 cf\s+\{nf\} add %r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c 01 cf\s+\{nf\} add %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c 01 8c 80 23 01 00 00\s+\{nf\} add %r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 01 8c 80 23 01 00 00\s+\{nf\} add %r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 02 9c 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 02 9c 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%bl,%dl
\s*[a-f0-9]+:\s*62 d4 7d 0c 03 94 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 03 94 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%dx,%ax
\s*[a-f0-9]+:\s*62 d4 7c 0c 03 8c 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 03 8c 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%ecx,%edx
\s*[a-f0-9]+:\s*62 54 fc 0c 03 8c 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 54 84 14 03 8c 80 23 01 00 00\s+\{nf\} add 0x123\(%r8,%rax,4\),%r9,%r31
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 e3 7b\s+\{nf\} and\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 e3 7b\s+\{nf\} and\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 e2 7b\s+\{nf\} and\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 e2 7b\s+\{nf\} and\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 e1 7b\s+\{nf\} and\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 e1 7b\s+\{nf\} and\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 e1 7b\s+\{nf\} and\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 83 e1 7b\s+\{nf\} and\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 a4 80 23 01 00 00 7b\s+\{nf\} andb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c 80 a4 80 23 01 00 00 7b\s+\{nf\} and\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 a4 80 23 01 00 00 7b\s+\{nf\} andw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 a4 80 23 01 00 00 7b\s+\{nf\} and\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 a4 80 23 01 00 00 7b\s+\{nf\} andl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c 83 a4 80 23 01 00 00 7b\s+\{nf\} and\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 a4 80 23 01 00 00 7b\s+\{nf\} andq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 a4 80 23 01 00 00 7b\s+\{nf\} and\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c 20 da\s+\{nf\} and %bl,%dl
\s*[a-f0-9]+:\s*62 f4 3c 1c 20 da\s+\{nf\} and %bl,%dl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 0c 20 9c 80 23 01 00 00\s+\{nf\} and %bl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 20 9c 80 23 01 00 00\s+\{nf\} and %bl,0x123\(%r8,%rax,4\),%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 21 d0\s+\{nf\} and %dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c 21 d0\s+\{nf\} and %dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c 21 94 80 23 01 00 00\s+\{nf\} and %dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c 21 94 80 23 01 00 00\s+\{nf\} and %dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 21 ca\s+\{nf\} and %ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c 21 ca\s+\{nf\} and %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c 21 8c 80 23 01 00 00\s+\{nf\} and %ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 21 8c 80 23 01 00 00\s+\{nf\} and %ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c 21 cf\s+\{nf\} and %r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c 21 cf\s+\{nf\} and %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c 21 8c 80 23 01 00 00\s+\{nf\} and %r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 21 8c 80 23 01 00 00\s+\{nf\} and %r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 22 9c 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 22 9c 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%bl,%dl
\s*[a-f0-9]+:\s*62 d4 7d 0c 23 94 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 23 94 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%dx,%ax
\s*[a-f0-9]+:\s*62 d4 7c 0c 23 8c 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 23 8c 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%ecx,%edx
\s*[a-f0-9]+:\s*62 54 fc 0c 23 8c 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 54 84 14 23 8c 80 23 01 00 00\s+\{nf\} and 0x123\(%r8,%rax,4\),%r9,%r31
\s*[a-f0-9]+:\s*62 72 6c 0c f2 d1\s+\{nf\} andn %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 52 84 04 f2 d9\s+\{nf\} andn %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 d2 74 0c f2 94 80 23 01 00 00\s+\{nf\} andn 0x123\(%r8,%rax,4\),%ecx,%edx
\s*[a-f0-9]+:\s*62 42 b4 0c f2 bc 80 23 01 00 00\s+\{nf\} andn 0x123\(%r8,%rax,4\),%r9,%r31
\s*[a-f0-9]+:\s*62 72 74 0c f7 d2\s+\{nf\} bextr %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d2 74 0c f7 94 80 23 01 00 00\s+\{nf\} bextr %ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5a b4 0c f7 df\s+\{nf\} bextr %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 42 b4 0c f7 bc 80 23 01 00 00\s+\{nf\} bextr %r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 f2 6c 0c f3 d9\s+\{nf\} blsi %ecx,%edx
\s*[a-f0-9]+:\s*62 d2 84 04 f3 d9\s+\{nf\} blsi %r9,%r31
\s*[a-f0-9]+:\s*62 d2 74 0c f3 9c 80 23 01 00 00\s+\{nf\} blsi 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d2 b4 0c f3 9c 80 23 01 00 00\s+\{nf\} blsi 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f2 6c 0c f3 d1\s+\{nf\} blsmsk %ecx,%edx
\s*[a-f0-9]+:\s*62 d2 84 04 f3 d1\s+\{nf\} blsmsk %r9,%r31
\s*[a-f0-9]+:\s*62 d2 74 0c f3 94 80 23 01 00 00\s+\{nf\} blsmsk 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d2 b4 0c f3 94 80 23 01 00 00\s+\{nf\} blsmsk 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f2 6c 0c f3 c9\s+\{nf\} blsr %ecx,%edx
\s*[a-f0-9]+:\s*62 d2 84 04 f3 c9\s+\{nf\} blsr %r9,%r31
\s*[a-f0-9]+:\s*62 d2 74 0c f3 8c 80 23 01 00 00\s+\{nf\} blsr 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d2 b4 0c f3 8c 80 23 01 00 00\s+\{nf\} blsr 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 72 74 0c f5 d2\s+\{nf\} bzhi %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d2 74 0c f5 94 80 23 01 00 00\s+\{nf\} bzhi %ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5a b4 0c f5 df\s+\{nf\} bzhi %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 42 b4 0c f5 bc 80 23 01 00 00\s+\{nf\} bzhi %r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 4c fc 0c 31 ff\s+\{nf\} xor %r31,%r31
\s*[a-f0-9]+:\s*62 f4 7c 0c fe cb\s+\{nf\} dec %bl
\s*[a-f0-9]+:\s*62 f4 6c 1c fe cb\s+\{nf\} dec %bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c ff ca\s+\{nf\} dec %dx
\s*[a-f0-9]+:\s*62 f4 7d 1c ff ca\s+\{nf\} dec %dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c ff c9\s+\{nf\} dec %ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c ff c9\s+\{nf\} dec %ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c ff c9\s+\{nf\} dec %r9
\s*[a-f0-9]+:\s*62 d4 84 14 ff c9\s+\{nf\} dec %r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c fe 8c 80 23 01 00 00\s+\{nf\} decb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c fe 8c 80 23 01 00 00\s+\{nf\} dec 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c ff 8c 80 23 01 00 00\s+\{nf\} decw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c ff 8c 80 23 01 00 00\s+\{nf\} dec 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c ff 8c 80 23 01 00 00\s+\{nf\} decl 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c ff 8c 80 23 01 00 00\s+\{nf\} dec 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c ff 8c 80 23 01 00 00\s+\{nf\} decq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c ff 8c 80 23 01 00 00\s+\{nf\} dec 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 f3\s+\{nf\} div %bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 f2\s+\{nf\} div %dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 f1\s+\{nf\} div %ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 f1\s+\{nf\} div %r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 b4 80 23 01 00 00\s+\{nf\} divb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 b4 80 23 01 00 00\s+\{nf\} divw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 b4 80 23 01 00 00\s+\{nf\} divl 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 b4 80 23 01 00 00\s+\{nf\} divq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 fb\s+\{nf\} idiv %bl
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 fb\s+\{nf\} idiv %bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 fa\s+\{nf\} idiv %dx
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 fa\s+\{nf\} idiv %dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 f9\s+\{nf\} idiv %ecx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 f9\s+\{nf\} idiv %ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 f9\s+\{nf\} idiv %r9
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 f9\s+\{nf\} idiv %r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 bc 80 23 01 00 00\s+\{nf\} idivb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 bc 80 23 01 00 00\s+\{nf\} idivb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 bc 80 23 01 00 00\s+\{nf\} idivw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 bc 80 23 01 00 00\s+\{nf\} idivw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 bc 80 23 01 00 00\s+\{nf\} idivl 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 bc 80 23 01 00 00\s+\{nf\} idivl 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 bc 80 23 01 00 00\s+\{nf\} idivq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 bc 80 23 01 00 00\s+\{nf\} idivq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 eb\s+\{nf\} imul %bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 ea\s+\{nf\} imul %dx
\s*[a-f0-9]+:\s*62 f4 7d 0c af c2\s+\{nf\} imul %dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c af c2\s+\{nf\} imul %dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 e9\s+\{nf\} imul %ecx
\s*[a-f0-9]+:\s*62 f4 7c 0c af d1\s+\{nf\} imul %ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c af d1\s+\{nf\} imul %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 e9\s+\{nf\} imul %r9
\s*[a-f0-9]+:\s*62 44 fc 0c af f9\s+\{nf\} imul %r9,%r31
\s*[a-f0-9]+:\s*62 44 a4 1c af f9\s+\{nf\} imul %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 ac 80 23 01 00 00\s+\{nf\} imulb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 ac 80 23 01 00 00\s+\{nf\} imulw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 0c af 94 80 23 01 00 00\s+\{nf\} imul 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7d 1c af 94 80 23 01 00 00\s+\{nf\} imul 0x123\(%r8,%rax,4\),%dx,%ax
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 ac 80 23 01 00 00\s+\{nf\} imull 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 0c af 8c 80 23 01 00 00\s+\{nf\} imul 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c af 8c 80 23 01 00 00\s+\{nf\} imul 0x123\(%r8,%rax,4\),%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 ac 80 23 01 00 00\s+\{nf\} imulq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 0c af 8c 80 23 01 00 00\s+\{nf\} imul 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 54 84 14 af 8c 80 23 01 00 00\s+\{nf\} imul 0x123\(%r8,%rax,4\),%r9,%r31
\s*[a-f0-9]+:\s*62 f4 7c 0c fe c3\s+\{nf\} inc %bl
\s*[a-f0-9]+:\s*62 f4 6c 1c fe c3\s+\{nf\} inc %bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c ff c2\s+\{nf\} inc %dx
\s*[a-f0-9]+:\s*62 f4 7d 1c ff c2\s+\{nf\} inc %dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c ff c1\s+\{nf\} inc %ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c ff c1\s+\{nf\} inc %ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c ff c1\s+\{nf\} inc %r9
\s*[a-f0-9]+:\s*62 d4 84 14 ff c1\s+\{nf\} inc %r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c fe 84 80 23 01 00 00\s+\{nf\} incb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c fe 84 80 23 01 00 00\s+\{nf\} inc 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c ff 84 80 23 01 00 00\s+\{nf\} incw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c ff 84 80 23 01 00 00\s+\{nf\} inc 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c ff 84 80 23 01 00 00\s+\{nf\} incl 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c ff 84 80 23 01 00 00\s+\{nf\} inc 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c ff 84 80 23 01 00 00\s+\{nf\} incq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c ff 84 80 23 01 00 00\s+\{nf\} inc 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7d 0c f5 c2\s+\{nf\} lzcnt %dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c f5 d1\s+\{nf\} lzcnt %ecx,%edx
\s*[a-f0-9]+:\s*62 44 fc 0c f5 f9\s+\{nf\} lzcnt %r9,%r31
\s*[a-f0-9]+:\s*62 d4 7d 0c f5 94 80 23 01 00 00\s+\{nf\} lzcnt 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c f5 8c 80 23 01 00 00\s+\{nf\} lzcnt 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 54 fc 0c f5 8c 80 23 01 00 00\s+\{nf\} lzcnt 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 e3\s+\{nf\} mul %bl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 e2\s+\{nf\} mul %dx
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 e1\s+\{nf\} mul %ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 e1\s+\{nf\} mul %r9
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 a4 80 23 01 00 00\s+\{nf\} mulb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 a4 80 23 01 00 00\s+\{nf\} mulw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 a4 80 23 01 00 00\s+\{nf\} mull 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 a4 80 23 01 00 00\s+\{nf\} mulq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 f4 7c 0c f6 db\s+\{nf\} neg %bl
\s*[a-f0-9]+:\s*62 f4 6c 1c f6 db\s+\{nf\} neg %bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c f7 da\s+\{nf\} neg %dx
\s*[a-f0-9]+:\s*62 f4 7d 1c f7 da\s+\{nf\} neg %dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c f7 d9\s+\{nf\} neg %ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c f7 d9\s+\{nf\} neg %ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 d9\s+\{nf\} neg %r9
\s*[a-f0-9]+:\s*62 d4 84 14 f7 d9\s+\{nf\} neg %r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c f6 9c 80 23 01 00 00\s+\{nf\} negb 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c f6 9c 80 23 01 00 00\s+\{nf\} neg 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c f7 9c 80 23 01 00 00\s+\{nf\} negw 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c f7 9c 80 23 01 00 00\s+\{nf\} neg 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c f7 9c 80 23 01 00 00\s+\{nf\} negl 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c f7 9c 80 23 01 00 00\s+\{nf\} neg 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c f7 9c 80 23 01 00 00\s+\{nf\} negq 0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c f7 9c 80 23 01 00 00\s+\{nf\} neg 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 cb 7b\s+\{nf\} or\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 cb 7b\s+\{nf\} or\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 ca 7b\s+\{nf\} or\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 ca 7b\s+\{nf\} or\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 c9 7b\s+\{nf\} or\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 c9 7b\s+\{nf\} or\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 c9 7b\s+\{nf\} or\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 83 c9 7b\s+\{nf\} or\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 8c 80 23 01 00 00 7b\s+\{nf\} orb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c 80 8c 80 23 01 00 00 7b\s+\{nf\} or\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 8c 80 23 01 00 00 7b\s+\{nf\} orw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 8c 80 23 01 00 00 7b\s+\{nf\} or\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 8c 80 23 01 00 00 7b\s+\{nf\} orl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c 83 8c 80 23 01 00 00 7b\s+\{nf\} or\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 8c 80 23 01 00 00 7b\s+\{nf\} orq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 8c 80 23 01 00 00 7b\s+\{nf\} or\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c 08 da\s+\{nf\} or %bl,%dl
\s*[a-f0-9]+:\s*62 f4 3c 1c 08 da\s+\{nf\} or %bl,%dl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 0c 08 9c 80 23 01 00 00\s+\{nf\} or %bl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 08 9c 80 23 01 00 00\s+\{nf\} or %bl,0x123\(%r8,%rax,4\),%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 09 d0\s+\{nf\} or %dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c 09 d0\s+\{nf\} or %dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c 09 94 80 23 01 00 00\s+\{nf\} or %dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c 09 94 80 23 01 00 00\s+\{nf\} or %dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 09 ca\s+\{nf\} or %ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c 09 ca\s+\{nf\} or %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c 09 8c 80 23 01 00 00\s+\{nf\} or %ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 09 8c 80 23 01 00 00\s+\{nf\} or %ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c 09 cf\s+\{nf\} or %r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c 09 cf\s+\{nf\} or %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c 09 8c 80 23 01 00 00\s+\{nf\} or %r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 09 8c 80 23 01 00 00\s+\{nf\} or %r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 0a 9c 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 0a 9c 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%bl,%dl
\s*[a-f0-9]+:\s*62 d4 7d 0c 0b 94 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 0b 94 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%dx,%ax
\s*[a-f0-9]+:\s*62 d4 7c 0c 0b 8c 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 0b 8c 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%ecx,%edx
\s*[a-f0-9]+:\s*62 54 fc 0c 0b 8c 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 54 84 14 0b 8c 80 23 01 00 00\s+\{nf\} or 0x123\(%r8,%rax,4\),%r9,%r31
\s*[a-f0-9]+:\s*62 f4 7d 0c 88 c2\s+\{nf\} popcnt %dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 88 d1\s+\{nf\} popcnt %ecx,%edx
\s*[a-f0-9]+:\s*62 44 fc 0c 88 f9\s+\{nf\} popcnt %r9,%r31
\s*[a-f0-9]+:\s*62 d4 7d 0c 88 94 80 23 01 00 00\s+\{nf\} popcnt 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c 88 8c 80 23 01 00 00\s+\{nf\} popcnt 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 54 fc 0c 88 8c 80 23 01 00 00\s+\{nf\} popcnt 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 c3\s+\{nf\} rol\s+\$1,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 c3\s+\{nf\} rol\s+\$1,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 c2\s+\{nf\} rol\s+\$1,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 c2\s+\{nf\} rol\s+\$1,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 c1\s+\{nf\} rol\s+\$1,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 c1\s+\{nf\} rol\s+\$1,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 c1\s+\{nf\} rol\s+\$1,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d1 c1\s+\{nf\} rol\s+\$1,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 84 80 23 01 00 00\s+\{nf\} rolb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d0 84 80 23 01 00 00\s+\{nf\} rol\s+\$1,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 84 80 23 01 00 00\s+\{nf\} rolw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 84 80 23 01 00 00\s+\{nf\} rol\s+\$1,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 84 80 23 01 00 00\s+\{nf\} roll\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d1 84 80 23 01 00 00\s+\{nf\} rol\s+\$1,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 84 80 23 01 00 00\s+\{nf\} rolq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 84 80 23 01 00 00\s+\{nf\} rol\s+\$1,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 c3 7b\s+\{nf\} rol\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 c3 7b\s+\{nf\} rol\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 c2 7b\s+\{nf\} rol\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 c2 7b\s+\{nf\} rol\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 c1 7b\s+\{nf\} rol\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 c1 7b\s+\{nf\} rol\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 c1 7b\s+\{nf\} rol\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 c1 c1 7b\s+\{nf\} rol\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 84 80 23 01 00 00 7b\s+\{nf\} rolb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c c0 84 80 23 01 00 00 7b\s+\{nf\} rol\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 84 80 23 01 00 00 7b\s+\{nf\} rolw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 84 80 23 01 00 00 7b\s+\{nf\} rol\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 84 80 23 01 00 00 7b\s+\{nf\} roll\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c c1 84 80 23 01 00 00 7b\s+\{nf\} rol\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 84 80 23 01 00 00 7b\s+\{nf\} rolq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 84 80 23 01 00 00 7b\s+\{nf\} rol\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 c3\s+\{nf\} rol %cl,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 c3\s+\{nf\} rol %cl,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 c2\s+\{nf\} rol %cl,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 c2\s+\{nf\} rol %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 c1\s+\{nf\} rol %cl,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 c1\s+\{nf\} rol %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 c1\s+\{nf\} rol %cl,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d3 c1\s+\{nf\} rol %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 84 80 23 01 00 00\s+\{nf\} rolb %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d2 84 80 23 01 00 00\s+\{nf\} rol %cl,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 84 80 23 01 00 00\s+\{nf\} rolw %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 84 80 23 01 00 00\s+\{nf\} rol %cl,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 84 80 23 01 00 00\s+\{nf\} roll %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d3 84 80 23 01 00 00\s+\{nf\} rol %cl,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 84 80 23 01 00 00\s+\{nf\} rolq %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 84 80 23 01 00 00\s+\{nf\} rol %cl,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 cb\s+\{nf\} ror\s+\$1,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 cb\s+\{nf\} ror\s+\$1,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 ca\s+\{nf\} ror\s+\$1,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 ca\s+\{nf\} ror\s+\$1,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 c9\s+\{nf\} ror\s+\$1,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 c9\s+\{nf\} ror\s+\$1,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 c9\s+\{nf\} ror\s+\$1,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d1 c9\s+\{nf\} ror\s+\$1,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 8c 80 23 01 00 00\s+\{nf\} rorb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d0 8c 80 23 01 00 00\s+\{nf\} ror\s+\$1,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 8c 80 23 01 00 00\s+\{nf\} rorw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 8c 80 23 01 00 00\s+\{nf\} ror\s+\$1,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 8c 80 23 01 00 00\s+\{nf\} rorl\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d1 8c 80 23 01 00 00\s+\{nf\} ror\s+\$1,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 8c 80 23 01 00 00\s+\{nf\} rorq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 8c 80 23 01 00 00\s+\{nf\} ror\s+\$1,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 cb 7b\s+\{nf\} ror\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 cb 7b\s+\{nf\} ror\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 ca 7b\s+\{nf\} ror\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 ca 7b\s+\{nf\} ror\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 c9 7b\s+\{nf\} ror\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 c9 7b\s+\{nf\} ror\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 c9 7b\s+\{nf\} ror\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 c1 c9 7b\s+\{nf\} ror\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 8c 80 23 01 00 00 7b\s+\{nf\} rorb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c c0 8c 80 23 01 00 00 7b\s+\{nf\} ror\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 8c 80 23 01 00 00 7b\s+\{nf\} rorw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 8c 80 23 01 00 00 7b\s+\{nf\} rorl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 8c 80 23 01 00 00 7b\s+\{nf\} rorq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 8c 80 23 01 00 00 7b\s+\{nf\} ror\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 cb\s+\{nf\} ror %cl,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 cb\s+\{nf\} ror %cl,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 ca\s+\{nf\} ror %cl,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 ca\s+\{nf\} ror %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 c9\s+\{nf\} ror %cl,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 c9\s+\{nf\} ror %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 c9\s+\{nf\} ror %cl,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d3 c9\s+\{nf\} ror %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 8c 80 23 01 00 00\s+\{nf\} rorb %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d2 8c 80 23 01 00 00\s+\{nf\} ror %cl,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 8c 80 23 01 00 00\s+\{nf\} rorw %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 8c 80 23 01 00 00\s+\{nf\} ror %cl,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 8c 80 23 01 00 00\s+\{nf\} rorl %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d3 8c 80 23 01 00 00\s+\{nf\} ror %cl,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 8c 80 23 01 00 00\s+\{nf\} rorq %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 8c 80 23 01 00 00\s+\{nf\} ror %cl,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 e3\s+\{nf\} shl\s+\$1,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 e3\s+\{nf\} shl\s+\$1,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 e2\s+\{nf\} shl\s+\$1,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 e2\s+\{nf\} shl\s+\$1,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 e1\s+\{nf\} shl\s+\$1,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 e1\s+\{nf\} shl\s+\$1,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 e1\s+\{nf\} shl\s+\$1,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d1 e1\s+\{nf\} shl\s+\$1,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 a4 80 23 01 00 00\s+\{nf\} shlb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d0 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 a4 80 23 01 00 00\s+\{nf\} shlw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 a4 80 23 01 00 00\s+\{nf\} shll\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d1 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 a4 80 23 01 00 00\s+\{nf\} shlq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 e3 7b\s+\{nf\} shl\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 e3 7b\s+\{nf\} shl\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 e2 7b\s+\{nf\} shl\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 e2 7b\s+\{nf\} shl\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 a4 80 23 01 00 00 7b\s+\{nf\} shlb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c c0 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shlw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shll\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shlq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 e3\s+\{nf\} shl %cl,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 e3\s+\{nf\} shl %cl,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 e2\s+\{nf\} shl %cl,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 e2\s+\{nf\} shl %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 e1\s+\{nf\} shl %cl,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 e1\s+\{nf\} shl %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 e1\s+\{nf\} shl %cl,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d3 e1\s+\{nf\} shl %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 a4 80 23 01 00 00\s+\{nf\} shlb %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d2 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 a4 80 23 01 00 00\s+\{nf\} shlw %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 a4 80 23 01 00 00\s+\{nf\} shll %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d3 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 a4 80 23 01 00 00\s+\{nf\} shlq %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 fb\s+\{nf\} sar\s+\$1,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 fb\s+\{nf\} sar\s+\$1,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 fa\s+\{nf\} sar\s+\$1,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 fa\s+\{nf\} sar\s+\$1,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 f9\s+\{nf\} sar\s+\$1,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 f9\s+\{nf\} sar\s+\$1,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 f9\s+\{nf\} sar\s+\$1,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d1 f9\s+\{nf\} sar\s+\$1,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 bc 80 23 01 00 00\s+\{nf\} sarb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d0 bc 80 23 01 00 00\s+\{nf\} sar\s+\$1,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 bc 80 23 01 00 00\s+\{nf\} sarw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 bc 80 23 01 00 00\s+\{nf\} sar\s+\$1,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 bc 80 23 01 00 00\s+\{nf\} sarl\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d1 bc 80 23 01 00 00\s+\{nf\} sar\s+\$1,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 bc 80 23 01 00 00\s+\{nf\} sarq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 bc 80 23 01 00 00\s+\{nf\} sar\s+\$1,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 fb 7b\s+\{nf\} sar\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 fb 7b\s+\{nf\} sar\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 fa 7b\s+\{nf\} sar\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 fa 7b\s+\{nf\} sar\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 f9 7b\s+\{nf\} sar\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 f9 7b\s+\{nf\} sar\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 f9 7b\s+\{nf\} sar\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 c1 f9 7b\s+\{nf\} sar\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 bc 80 23 01 00 00 7b\s+\{nf\} sarb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c c0 bc 80 23 01 00 00 7b\s+\{nf\} sar\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 bc 80 23 01 00 00 7b\s+\{nf\} sarw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 bc 80 23 01 00 00 7b\s+\{nf\} sarl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 bc 80 23 01 00 00 7b\s+\{nf\} sarq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 bc 80 23 01 00 00 7b\s+\{nf\} sar\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 fb\s+\{nf\} sar %cl,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 fb\s+\{nf\} sar %cl,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 fa\s+\{nf\} sar %cl,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 fa\s+\{nf\} sar %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 f9\s+\{nf\} sar %cl,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 f9\s+\{nf\} sar %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 f9\s+\{nf\} sar %cl,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d3 f9\s+\{nf\} sar %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 bc 80 23 01 00 00\s+\{nf\} sarb %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d2 bc 80 23 01 00 00\s+\{nf\} sar %cl,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 bc 80 23 01 00 00\s+\{nf\} sarw %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 bc 80 23 01 00 00\s+\{nf\} sar %cl,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 bc 80 23 01 00 00\s+\{nf\} sarl %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d3 bc 80 23 01 00 00\s+\{nf\} sar %cl,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 bc 80 23 01 00 00\s+\{nf\} sarq %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 bc 80 23 01 00 00\s+\{nf\} sar %cl,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 e3\s+\{nf\} shl\s+\$1,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 e3\s+\{nf\} shl\s+\$1,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 e2\s+\{nf\} shl\s+\$1,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 e2\s+\{nf\} shl\s+\$1,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 e1\s+\{nf\} shl\s+\$1,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 e1\s+\{nf\} shl\s+\$1,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 e1\s+\{nf\} shl\s+\$1,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d1 e1\s+\{nf\} shl\s+\$1,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 a4 80 23 01 00 00\s+\{nf\} shlb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d0 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 a4 80 23 01 00 00\s+\{nf\} shlw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 a4 80 23 01 00 00\s+\{nf\} shll\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d1 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 a4 80 23 01 00 00\s+\{nf\} shlq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 a4 80 23 01 00 00\s+\{nf\} shl\s+\$1,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 e3 7b\s+\{nf\} shl\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 e3 7b\s+\{nf\} shl\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 e2 7b\s+\{nf\} shl\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 e2 7b\s+\{nf\} shl\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 c1 e1 7b\s+\{nf\} shl\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 a4 80 23 01 00 00 7b\s+\{nf\} shlb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c c0 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shlw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shll\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 a4 80 23 01 00 00 7b\s+\{nf\} shlq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 a4 80 23 01 00 00 7b\s+\{nf\} shl\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 e3\s+\{nf\} shl %cl,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 e3\s+\{nf\} shl %cl,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 e2\s+\{nf\} shl %cl,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 e2\s+\{nf\} shl %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 e1\s+\{nf\} shl %cl,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 e1\s+\{nf\} shl %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 e1\s+\{nf\} shl %cl,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d3 e1\s+\{nf\} shl %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 a4 80 23 01 00 00\s+\{nf\} shlb %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d2 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 a4 80 23 01 00 00\s+\{nf\} shlw %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 a4 80 23 01 00 00\s+\{nf\} shll %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d3 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 a4 80 23 01 00 00\s+\{nf\} shlq %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 a4 80 23 01 00 00\s+\{nf\} shl %cl,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7d 0c 24 d0 7b\s+\{nf\} shld\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c 24 d0 7b\s+\{nf\} shld\s+\$0x7b,%dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c 24 94 80 23 01 00 00 7b\s+\{nf\} shld\s+\$0x7b,%dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c 24 94 80 23 01 00 00 7b\s+\{nf\} shld\s+\$0x7b,%dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 24 ca 7b\s+\{nf\} shld\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c 24 ca 7b\s+\{nf\} shld\s+\$0x7b,%ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c 24 8c 80 23 01 00 00 7b\s+\{nf\} shld\s+\$0x7b,%ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 24 8c 80 23 01 00 00 7b\s+\{nf\} shld\s+\$0x7b,%ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c 24 cf 7b\s+\{nf\} shld\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c 24 cf 7b\s+\{nf\} shld\s+\$0x7b,%r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c 24 8c 80 23 01 00 00 7b\s+\{nf\} shld\s+\$0x7b,%r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 24 8c 80 23 01 00 00 7b\s+\{nf\} shld\s+\$0x7b,%r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 f4 7d 0c a5 d0\s+\{nf\} shld %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c a5 d0\s+\{nf\} shld %cl,%dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c a5 94 80 23 01 00 00\s+\{nf\} shld %cl,%dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c a5 94 80 23 01 00 00\s+\{nf\} shld %cl,%dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c a5 ca\s+\{nf\} shld %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c a5 ca\s+\{nf\} shld %cl,%ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c a5 8c 80 23 01 00 00\s+\{nf\} shld %cl,%ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c a5 8c 80 23 01 00 00\s+\{nf\} shld %cl,%ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c a5 cf\s+\{nf\} shld %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c a5 cf\s+\{nf\} shld %cl,%r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c a5 8c 80 23 01 00 00\s+\{nf\} shld %cl,%r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 a5 8c 80 23 01 00 00\s+\{nf\} shld %cl,%r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 f4 7c 0c d0 eb\s+\{nf\} shr\s+\$1,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d0 eb\s+\{nf\} shr\s+\$1,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d1 ea\s+\{nf\} shr\s+\$1,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d1 ea\s+\{nf\} shr\s+\$1,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d1 e9\s+\{nf\} shr\s+\$1,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d1 e9\s+\{nf\} shr\s+\$1,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 e9\s+\{nf\} shr\s+\$1,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d1 e9\s+\{nf\} shr\s+\$1,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d0 ac 80 23 01 00 00\s+\{nf\} shrb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d0 ac 80 23 01 00 00\s+\{nf\} shr\s+\$1,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d1 ac 80 23 01 00 00\s+\{nf\} shrw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d1 ac 80 23 01 00 00\s+\{nf\} shr\s+\$1,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d1 ac 80 23 01 00 00\s+\{nf\} shrl\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d1 ac 80 23 01 00 00\s+\{nf\} shr\s+\$1,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d1 ac 80 23 01 00 00\s+\{nf\} shrq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d1 ac 80 23 01 00 00\s+\{nf\} shr\s+\$1,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c c0 eb 7b\s+\{nf\} shr\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c c0 eb 7b\s+\{nf\} shr\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c c1 ea 7b\s+\{nf\} shr\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c c1 ea 7b\s+\{nf\} shr\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c c1 e9 7b\s+\{nf\} shr\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c c1 e9 7b\s+\{nf\} shr\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 e9 7b\s+\{nf\} shr\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 c1 e9 7b\s+\{nf\} shr\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c c0 ac 80 23 01 00 00 7b\s+\{nf\} shrb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c c0 ac 80 23 01 00 00 7b\s+\{nf\} shr\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c c1 ac 80 23 01 00 00 7b\s+\{nf\} shrw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c c1 ac 80 23 01 00 00 7b\s+\{nf\} shrl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c c1 ac 80 23 01 00 00 7b\s+\{nf\} shrq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c c1 ac 80 23 01 00 00 7b\s+\{nf\} shr\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c d2 eb\s+\{nf\} shr %cl,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c d2 eb\s+\{nf\} shr %cl,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c d3 ea\s+\{nf\} shr %cl,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c d3 ea\s+\{nf\} shr %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c d3 e9\s+\{nf\} shr %cl,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c d3 e9\s+\{nf\} shr %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 e9\s+\{nf\} shr %cl,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 d3 e9\s+\{nf\} shr %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c d2 ac 80 23 01 00 00\s+\{nf\} shrb %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c d2 ac 80 23 01 00 00\s+\{nf\} shr %cl,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c d3 ac 80 23 01 00 00\s+\{nf\} shrw %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c d3 ac 80 23 01 00 00\s+\{nf\} shr %cl,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c d3 ac 80 23 01 00 00\s+\{nf\} shrl %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c d3 ac 80 23 01 00 00\s+\{nf\} shr %cl,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c d3 ac 80 23 01 00 00\s+\{nf\} shrq %cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c d3 ac 80 23 01 00 00\s+\{nf\} shr %cl,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7d 0c 2c d0 7b\s+\{nf\} shrd\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c 2c d0 7b\s+\{nf\} shrd\s+\$0x7b,%dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c 2c 94 80 23 01 00 00 7b\s+\{nf\} shrd\s+\$0x7b,%dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c 2c 94 80 23 01 00 00 7b\s+\{nf\} shrd\s+\$0x7b,%dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 2c ca 7b\s+\{nf\} shrd\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c 2c ca 7b\s+\{nf\} shrd\s+\$0x7b,%ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd\s+\$0x7b,%ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd\s+\$0x7b,%ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c 2c cf 7b\s+\{nf\} shrd\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c 2c cf 7b\s+\{nf\} shrd\s+\$0x7b,%r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd\s+\$0x7b,%r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 2c 8c 80 23 01 00 00 7b\s+\{nf\} shrd\s+\$0x7b,%r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 f4 7d 0c ad d0\s+\{nf\} shrd %cl,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c ad d0\s+\{nf\} shrd %cl,%dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c ad 94 80 23 01 00 00\s+\{nf\} shrd %cl,%dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c ad 94 80 23 01 00 00\s+\{nf\} shrd %cl,%dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c ad ca\s+\{nf\} shrd %cl,%ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c ad ca\s+\{nf\} shrd %cl,%ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c ad 8c 80 23 01 00 00\s+\{nf\} shrd %cl,%ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c ad 8c 80 23 01 00 00\s+\{nf\} shrd %cl,%ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c ad cf\s+\{nf\} shrd %cl,%r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c ad cf\s+\{nf\} shrd %cl,%r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c ad 8c 80 23 01 00 00\s+\{nf\} shrd %cl,%r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 ad 8c 80 23 01 00 00\s+\{nf\} shrd %cl,%r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 eb 7b\s+\{nf\} sub\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 eb 7b\s+\{nf\} sub\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 ea 7b\s+\{nf\} sub\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 ea 7b\s+\{nf\} sub\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 e9 7b\s+\{nf\} sub\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 e9 7b\s+\{nf\} sub\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 e9 7b\s+\{nf\} sub\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 83 e9 7b\s+\{nf\} sub\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 ac 80 23 01 00 00 7b\s+\{nf\} subb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c 80 ac 80 23 01 00 00 7b\s+\{nf\} sub\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 ac 80 23 01 00 00 7b\s+\{nf\} subw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 ac 80 23 01 00 00 7b\s+\{nf\} subl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 ac 80 23 01 00 00 7b\s+\{nf\} subq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 ac 80 23 01 00 00 7b\s+\{nf\} sub\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c 28 da\s+\{nf\} sub %bl,%dl
\s*[a-f0-9]+:\s*62 f4 3c 1c 28 da\s+\{nf\} sub %bl,%dl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 0c 28 9c 80 23 01 00 00\s+\{nf\} sub %bl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 28 9c 80 23 01 00 00\s+\{nf\} sub %bl,0x123\(%r8,%rax,4\),%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 29 d0\s+\{nf\} sub %dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c 29 d0\s+\{nf\} sub %dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c 29 94 80 23 01 00 00\s+\{nf\} sub %dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c 29 94 80 23 01 00 00\s+\{nf\} sub %dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 29 ca\s+\{nf\} sub %ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c 29 ca\s+\{nf\} sub %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c 29 8c 80 23 01 00 00\s+\{nf\} sub %ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 29 8c 80 23 01 00 00\s+\{nf\} sub %ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c 29 cf\s+\{nf\} sub %r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c 29 cf\s+\{nf\} sub %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c 29 8c 80 23 01 00 00\s+\{nf\} sub %r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 29 8c 80 23 01 00 00\s+\{nf\} sub %r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 2a 9c 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 2a 9c 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%bl,%dl
\s*[a-f0-9]+:\s*62 d4 7d 0c 2b 94 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 2b 94 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%dx,%ax
\s*[a-f0-9]+:\s*62 d4 7c 0c 2b 8c 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 2b 8c 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%ecx,%edx
\s*[a-f0-9]+:\s*62 54 fc 0c 2b 8c 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 54 84 14 2b 8c 80 23 01 00 00\s+\{nf\} sub 0x123\(%r8,%rax,4\),%r9,%r31
\s*[a-f0-9]+:\s*62 f4 7d 0c f4 c2\s+\{nf\} tzcnt %dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c f4 d1\s+\{nf\} tzcnt %ecx,%edx
\s*[a-f0-9]+:\s*62 44 fc 0c f4 f9\s+\{nf\} tzcnt %r9,%r31
\s*[a-f0-9]+:\s*62 d4 7d 0c f4 94 80 23 01 00 00\s+\{nf\} tzcnt 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c f4 8c 80 23 01 00 00\s+\{nf\} tzcnt 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 54 fc 0c f4 8c 80 23 01 00 00\s+\{nf\} tzcnt 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c 80 f3 7b\s+\{nf\} xor\s+\$0x7b,%bl
\s*[a-f0-9]+:\s*62 f4 6c 1c 80 f3 7b\s+\{nf\} xor\s+\$0x7b,%bl,%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 83 f2 7b\s+\{nf\} xor\s+\$0x7b,%dx
\s*[a-f0-9]+:\s*62 f4 7d 1c 83 f2 7b\s+\{nf\} xor\s+\$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 83 f1 7b\s+\{nf\} xor\s+\$0x7b,%ecx
\s*[a-f0-9]+:\s*62 f4 6c 1c 83 f1 7b\s+\{nf\} xor\s+\$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 f1 7b\s+\{nf\} xor\s+\$0x7b,%r9
\s*[a-f0-9]+:\s*62 d4 84 14 83 f1 7b\s+\{nf\} xor\s+\$0x7b,%r9,%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 80 b4 80 23 01 00 00 7b\s+\{nf\} xorb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 64 1c 80 b4 80 23 01 00 00 7b\s+\{nf\} xor\s+\$0x7b,0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 7d 0c 83 b4 80 23 01 00 00 7b\s+\{nf\} xorw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6d 1c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor\s+\$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 0c 83 b4 80 23 01 00 00 7b\s+\{nf\} xorl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 74 1c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor\s+\$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 fc 0c 83 b4 80 23 01 00 00 7b\s+\{nf\} xorq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 b4 1c 83 b4 80 23 01 00 00 7b\s+\{nf\} xor\s+\$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7c 0c 30 da\s+\{nf\} xor %bl,%dl
\s*[a-f0-9]+:\s*62 f4 3c 1c 30 da\s+\{nf\} xor %bl,%dl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 0c 30 9c 80 23 01 00 00\s+\{nf\} xor %bl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 30 9c 80 23 01 00 00\s+\{nf\} xor %bl,0x123\(%r8,%rax,4\),%dl
\s*[a-f0-9]+:\s*62 f4 7d 0c 31 d0\s+\{nf\} xor %dx,%ax
\s*[a-f0-9]+:\s*62 f4 35 1c 31 d0\s+\{nf\} xor %dx,%ax,%r9w
\s*[a-f0-9]+:\s*62 d4 7d 0c 31 94 80 23 01 00 00\s+\{nf\} xor %dx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 1c 31 94 80 23 01 00 00\s+\{nf\} xor %dx,0x123\(%r8,%rax,4\),%ax
\s*[a-f0-9]+:\s*62 f4 7c 0c 31 ca\s+\{nf\} xor %ecx,%edx
\s*[a-f0-9]+:\s*62 f4 2c 1c 31 ca\s+\{nf\} xor %ecx,%edx,%r10d
\s*[a-f0-9]+:\s*62 d4 7c 0c 31 8c 80 23 01 00 00\s+\{nf\} xor %ecx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 6c 1c 31 8c 80 23 01 00 00\s+\{nf\} xor %ecx,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 5c fc 0c 31 cf\s+\{nf\} xor %r9,%r31
\s*[a-f0-9]+:\s*62 5c a4 1c 31 cf\s+\{nf\} xor %r9,%r31,%r11
\s*[a-f0-9]+:\s*62 54 fc 0c 31 8c 80 23 01 00 00\s+\{nf\} xor %r9,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 84 14 31 8c 80 23 01 00 00\s+\{nf\} xor %r9,0x123\(%r8,%rax,4\),%r31
\s*[a-f0-9]+:\s*62 d4 7c 0c 32 9c 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%bl
\s*[a-f0-9]+:\s*62 d4 6c 1c 32 9c 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%bl,%dl
\s*[a-f0-9]+:\s*62 d4 7d 0c 33 94 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7d 1c 33 94 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%dx,%ax
\s*[a-f0-9]+:\s*62 d4 7c 0c 33 8c 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 d4 6c 1c 33 8c 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%ecx,%edx
\s*[a-f0-9]+:\s*62 54 fc 0c 33 8c 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 54 84 14 33 8c 80 23 01 00 00\s+\{nf\} xor 0x123\(%r8,%rax,4\),%r9,%r31
#pass
