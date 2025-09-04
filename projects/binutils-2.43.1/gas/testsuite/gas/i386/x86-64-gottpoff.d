#as:
#objdump: -dwr
#name: x86-64 gottpoff

.*: +file format .*


Disassembly of section .text:

0+ <_start>:
 +[a-f0-9]+:	48 03 05 00 00 00 00 	add    0x0\(%rip\),%rax        # 7 <_start\+0x7>	3: R_X86_64_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	48 8b 05 00 00 00 00 	mov    0x0\(%rip\),%rax        # e <_start\+0xe>	a: R_X86_64_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	d5 48 03 05 00 00 00 00 	add    0x0\(%rip\),%r16        # 16 <_start\+0x16>	12: R_X86_64_CODE_4_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	d5 48 8b 25 00 00 00 00 	mov    0x0\(%rip\),%r20        # 1e <_start\+0x1e>	1a: R_X86_64_CODE_4_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	48 03 05 00 00 00 00 	add    0x0\(%rip\),%rax        # 25 <_start\+0x25>	21: R_X86_64_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	48 8b 05 00 00 00 00 	mov    0x0\(%rip\),%rax        # 2c <_start\+0x2c>	28: R_X86_64_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	d5 48 03 05 00 00 00 00 	add    0x0\(%rip\),%r16        # 34 <_start\+0x34>	30: R_X86_64_CODE_4_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	d5 48 8b 25 00 00 00 00 	mov    0x0\(%rip\),%r20        # 3c <_start\+0x3c>	38: R_X86_64_CODE_4_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	62 74 fc 10 01 05 00 00 00 00 	add    %r8,0x0\(%rip\),%r16        # 46 <_start\+0x46>	42: R_X86_64_CODE_6_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	62 f4 9c 18 03 05 00 00 00 00 	add    0x0\(%rip\),%rax,%r12        # 50 <_start\+0x50>	4c: R_X86_64_CODE_6_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	62 74 fc 14 01 05 00 00 00 00 	\{nf\} add %r8,0x0\(%rip\),%r16        # 5a <_start\+0x5a>	56: R_X86_64_CODE_6_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	62 f4 9c 1c 03 05 00 00 00 00 	\{nf\} add 0x0\(%rip\),%rax,%r12        # 64 <_start\+0x64>	60: R_X86_64_CODE_6_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	62 74 fc 10 01 05 00 00 00 00 	add    %r8,0x0\(%rip\),%r16        # 6e <_start\+0x6e>	6a: R_X86_64_CODE_6_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	62 f4 9c 18 03 05 00 00 00 00 	add    0x0\(%rip\),%rax,%r12        # 78 <_start\+0x78>	74: R_X86_64_CODE_6_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	62 74 fc 14 01 05 00 00 00 00 	\{nf\} add %r8,0x0\(%rip\),%r16        # 82 <_start\+0x82>	7e: R_X86_64_CODE_6_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	62 f4 9c 1c 03 05 00 00 00 00 	\{nf\} add 0x0\(%rip\),%rax,%r12        # 8c <_start\+0x8c>	88: R_X86_64_CODE_6_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	62 f4 fc 0c 03 05 00 00 00 00 	\{nf\} add 0x0\(%rip\),%rax        # 96 <_start\+0x96>	92: R_X86_64_CODE_6_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	62 e4 fc 0c 03 05 00 00 00 00 	\{nf\} add 0x0\(%rip\),%r16        # a0 <_start\+0xa0>	9c: R_X86_64_CODE_6_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	62 f4 fc 0c 03 05 00 00 00 00 	\{nf\} add 0x0\(%rip\),%rax        # aa <_start\+0xaa>	a6: R_X86_64_CODE_6_GOTTPOFF	foo-0x4
 +[a-f0-9]+:	62 e4 fc 0c 03 05 00 00 00 00 	\{nf\} add 0x0\(%rip\),%r16        # b4 <_start\+0xb4>	b0: R_X86_64_CODE_6_GOTTPOFF	foo-0x4
#pass
