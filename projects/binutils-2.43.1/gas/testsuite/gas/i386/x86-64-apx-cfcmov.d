#as:
#objdump: -dw
#name: x86_64 APX_F insns
#source: x86-64-apx-cfcmov.s

.*: +file format .*

Disassembly of section \.text:

0+ <_start>:
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 42 c2[ 	]+cfcmovb %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 14 42 c2[ 	]+cfcmovb %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 0c 42 94 80 23 01 00 00[ 	]+cfcmovb %dx,0x123\(%r8,%rax,4\)
[ 	]*[a-f0-9]+:[ 	]*62 f4 7c 08 42 d1[ 	]+cfcmovb %ecx,%edx
[ 	]*[a-f0-9]+:[ 	]*62 f4 2c 1c 42 d1[ 	]+cfcmovb %ecx,%edx,%r10d
[ 	]*[a-f0-9]+:[ 	]*62 d4 7c 0c 42 8c 80 23 01 00 00[ 	]+cfcmovb %ecx,0x123\(%r8,%rax,4\)
[ 	]*[a-f0-9]+:[ 	]*62 5c fc 08 42 ff[ 	]+cfcmovb %r31,%r15
[ 	]*[a-f0-9]+:[ 	]*62 5c a4 1c 42 ff[ 	]+cfcmovb %r31,%r15,%r11
[ 	]*[a-f0-9]+:[ 	]*62 44 fc 0c 42 bc 80 23 01 00 00[ 	]+cfcmovb %r31,0x123\(%r8,%rax,4\)
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 08 42 94 80 23 01 00 00[ 	]+cfcmovb 0x123\(%r8,%rax,4\),%dx
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 1c 42 94 80 23 01 00 00[ 	]+cfcmovb 0x123\(%r8,%rax,4\),%dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 d4 7c 08 42 8c 80 23 01 00 00[ 	]+cfcmovb 0x123\(%r8,%rax,4\),%ecx
[ 	]*[a-f0-9]+:[ 	]*62 d4 6c 1c 42 8c 80 23 01 00 00[ 	]+cfcmovb 0x123\(%r8,%rax,4\),%ecx,%edx
[ 	]*[a-f0-9]+:[ 	]*62 44 fc 08 42 bc 80 23 01 00 00[ 	]+cfcmovb 0x123\(%r8,%rax,4\),%r31
[ 	]*[a-f0-9]+:[ 	]*62 44 84 1c 42 bc 80 23 01 00 00[ 	]+cfcmovb 0x123\(%r8,%rax,4\),%r31,%r15
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 42 c2[ 	]+cmovb  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 2c 18 42 d1[ 	]+cmovb  %ecx,%edx,%r10d
[ 	]*[a-f0-9]+:[ 	]*62 5c a4 18 42 ff[ 	]+cmovb  %r31,%r15,%r11
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 18 42 94 80 23 01 00 00[ 	]+cmovb  0x123\(%r8,%rax,4\),%dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 d4 6c 18 42 8c 80 23 01 00 00[ 	]+cmovb  0x123\(%r8,%rax,4\),%ecx,%edx
[ 	]*[a-f0-9]+:[ 	]*62 44 84 18 42 bc 80 23 01 00 00[ 	]+cmovb  0x123\(%r8,%rax,4\),%r31,%r15
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 40 c2[ 	]+cfcmovo %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 40 c2[ 	]+cmovo  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 41 c2[ 	]+cfcmovno %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 41 c2[ 	]+cmovno %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 42 c2[ 	]+cfcmovb %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 42 c2[ 	]+cmovb  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 42 c2[ 	]+cfcmovb %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 42 c2[ 	]+cmovb  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 43 c2[ 	]+cfcmovae %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 43 c2[ 	]+cmovae %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 43 c2[ 	]+cfcmovae %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 43 c2[ 	]+cmovae %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 44 c2[ 	]+cfcmove %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 44 c2[ 	]+cmove  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 44 c2[ 	]+cfcmove %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 44 c2[ 	]+cmove  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 45 c2[ 	]+cfcmovne %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 45 c2[ 	]+cmovne %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 45 c2[ 	]+cfcmovne %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 45 c2[ 	]+cmovne %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 46 c2[ 	]+cfcmovbe %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 46 c2[ 	]+cmovbe %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 46 c2[ 	]+cfcmovbe %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 46 c2[ 	]+cmovbe %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 47 c2[ 	]+cfcmova %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 47 c2[ 	]+cmova  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 47 c2[ 	]+cfcmova %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 47 c2[ 	]+cmova  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 48 c2[ 	]+cfcmovs %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 48 c2[ 	]+cmovs  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 49 c2[ 	]+cfcmovns %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 49 c2[ 	]+cmovns %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4a c2[ 	]+cfcmovp %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4a c2[ 	]+cmovp  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4a c2[ 	]+cfcmovp %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4a c2[ 	]+cmovp  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4b c2[ 	]+cfcmovnp %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4b c2[ 	]+cmovnp %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4b c2[ 	]+cfcmovnp %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4b c2[ 	]+cmovnp %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4c c2[ 	]+cfcmovl %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4c c2[ 	]+cmovl  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4c c2[ 	]+cfcmovl %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4c c2[ 	]+cmovl  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4d c2[ 	]+cfcmovge %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4d c2[ 	]+cmovge %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4d c2[ 	]+cfcmovge %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4d c2[ 	]+cmovge %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4e c2[ 	]+cfcmovle %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4e c2[ 	]+cmovle %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4e c2[ 	]+cfcmovle %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4e c2[ 	]+cmovle %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4f c2[ 	]+cfcmovg %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4f c2[ 	]+cmovg  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4f c2[ 	]+cfcmovg %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4f c2[ 	]+cmovg  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 42 c2[ 	]+cfcmovb %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 14 42 c2[ 	]+cfcmovb %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 0c 42 94 80 23 01 00 00[ 	]+cfcmovb %dx,0x123\(%r8,%rax,4\)
[ 	]*[a-f0-9]+:[ 	]*62 f4 7c 08 42 d1[ 	]+cfcmovb %ecx,%edx
[ 	]*[a-f0-9]+:[ 	]*62 f4 2c 1c 42 d1[ 	]+cfcmovb %ecx,%edx,%r10d
[ 	]*[a-f0-9]+:[ 	]*62 d4 7c 0c 42 8c 80 23 01 00 00[ 	]+cfcmovb %ecx,0x123\(%r8,%rax,4\)
[ 	]*[a-f0-9]+:[ 	]*62 5c fc 08 42 ff[ 	]+cfcmovb %r31,%r15
[ 	]*[a-f0-9]+:[ 	]*62 5c a4 1c 42 ff[ 	]+cfcmovb %r31,%r15,%r11
[ 	]*[a-f0-9]+:[ 	]*62 44 fc 0c 42 bc 80 23 01 00 00[ 	]+cfcmovb %r31,0x123\(%r8,%rax,4\)
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 08 42 94 80 23 01 00 00[ 	]+cfcmovb 0x123\(%r8,%rax,4\),%dx
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 1c 42 94 80 23 01 00 00[ 	]+cfcmovb 0x123\(%r8,%rax,4\),%dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 d4 7c 08 42 8c 80 23 01 00 00[ 	]+cfcmovb 0x123\(%r8,%rax,4\),%ecx
[ 	]*[a-f0-9]+:[ 	]*62 d4 6c 1c 42 8c 80 23 01 00 00[ 	]+cfcmovb 0x123\(%r8,%rax,4\),%ecx,%edx
[ 	]*[a-f0-9]+:[ 	]*62 44 fc 08 42 bc 80 23 01 00 00[ 	]+cfcmovb 0x123\(%r8,%rax,4\),%r31
[ 	]*[a-f0-9]+:[ 	]*62 44 84 1c 42 bc 80 23 01 00 00[ 	]+cfcmovb 0x123\(%r8,%rax,4\),%r31,%r15
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 42 c2[ 	]+cmovb  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 2c 18 42 d1[ 	]+cmovb  %ecx,%edx,%r10d
[ 	]*[a-f0-9]+:[ 	]*62 5c a4 18 42 ff[ 	]+cmovb  %r31,%r15,%r11
[ 	]*[a-f0-9]+:[ 	]*62 d4 7d 18 42 94 80 23 01 00 00[ 	]+cmovb  0x123\(%r8,%rax,4\),%dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 d4 6c 18 42 8c 80 23 01 00 00[ 	]+cmovb  0x123\(%r8,%rax,4\),%ecx,%edx
[ 	]*[a-f0-9]+:[ 	]*62 44 84 18 42 bc 80 23 01 00 00[ 	]+cmovb  0x123\(%r8,%rax,4\),%r31,%r15
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 40 c2[ 	]+cfcmovo %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 40 c2[ 	]+cmovo  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 41 c2[ 	]+cfcmovno %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 41 c2[ 	]+cmovno %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 42 c2[ 	]+cfcmovb %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 42 c2[ 	]+cmovb  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 42 c2[ 	]+cfcmovb %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 42 c2[ 	]+cmovb  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 43 c2[ 	]+cfcmovae %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 43 c2[ 	]+cmovae %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 43 c2[ 	]+cfcmovae %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 43 c2[ 	]+cmovae %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 44 c2[ 	]+cfcmove %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 44 c2[ 	]+cmove  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 44 c2[ 	]+cfcmove %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 44 c2[ 	]+cmove  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 45 c2[ 	]+cfcmovne %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 45 c2[ 	]+cmovne %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 45 c2[ 	]+cfcmovne %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 45 c2[ 	]+cmovne %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 46 c2[ 	]+cfcmovbe %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 46 c2[ 	]+cmovbe %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 46 c2[ 	]+cfcmovbe %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 46 c2[ 	]+cmovbe %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 47 c2[ 	]+cfcmova %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 47 c2[ 	]+cmova  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 47 c2[ 	]+cfcmova %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 47 c2[ 	]+cmova  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 48 c2[ 	]+cfcmovs %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 48 c2[ 	]+cmovs  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 49 c2[ 	]+cfcmovns %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 49 c2[ 	]+cmovns %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4a c2[ 	]+cfcmovp %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4a c2[ 	]+cmovp  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4a c2[ 	]+cfcmovp %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4a c2[ 	]+cmovp  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4b c2[ 	]+cfcmovnp %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4b c2[ 	]+cmovnp %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4b c2[ 	]+cfcmovnp %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4b c2[ 	]+cmovnp %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4c c2[ 	]+cfcmovl %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4c c2[ 	]+cmovl  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4c c2[ 	]+cfcmovl %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4c c2[ 	]+cmovl  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4d c2[ 	]+cfcmovge %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4d c2[ 	]+cmovge %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4d c2[ 	]+cfcmovge %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4d c2[ 	]+cmovge %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4e c2[ 	]+cfcmovle %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4e c2[ 	]+cmovle %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4e c2[ 	]+cfcmovle %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4e c2[ 	]+cmovle %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4f c2[ 	]+cfcmovg %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4f c2[ 	]+cmovg  %dx,%ax,%r31w
[ 	]*[a-f0-9]+:[ 	]*62 f4 7d 08 4f c2[ 	]+cfcmovg %dx,%ax
[ 	]*[a-f0-9]+:[ 	]*62 f4 05 10 4f c2[ 	]+cmovg  %dx,%ax,%r31w
#pass
