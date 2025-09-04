#as:
#objdump: -dw -Msuffix
#name: APX x86-64 pseudo prefixes

.*: +file format .*

Disassembly of section \.text:

0+ <_start>:
[	 ]*[a-f0-9]+:[	 ]*d5 11 89 cf[	 ]+movl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 44 8b f9[	 ]+movl.s %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 11 89 cf[	 ]+movl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 8b 39[	 ]+movl   \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 89 39[	 ]+movl   %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*d5 11 11 cf[	 ]+adcl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 44 13 f9[	 ]+adcl.s %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 11 11 cf[	 ]+adcl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 13 39[	 ]+adcl   \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 11 39[	 ]+adcl   %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*d5 11 01 cf[	 ]+addl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 44 03 f9[	 ]+addl.s %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 11 01 cf[	 ]+addl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 03 39[	 ]+addl   \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 01 39[	 ]+addl   %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*d5 11 21 cf[	 ]+andl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 44 23 f9[	 ]+andl.s %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 11 21 cf[	 ]+andl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 23 39[	 ]+andl   \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 21 39[	 ]+andl   %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*d5 11 39 cf[	 ]+cmpl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 44 3b f9[	 ]+cmpl.s %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 11 39 cf[	 ]+cmpl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 3b 39[	 ]+cmpl   \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 39 39[	 ]+cmpl   %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*d5 11 09 cf[	 ]+orl    %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 44 0b f9[	 ]+orl.s  %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 11 09 cf[	 ]+orl    %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 0b 39[	 ]+orl    \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 09 39[	 ]+orl    %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*d5 11 19 cf[	 ]+sbbl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 44 1b f9[	 ]+sbbl.s %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 11 19 cf[	 ]+sbbl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 1b 39[	 ]+sbbl   \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 19 39[	 ]+sbbl   %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*d5 11 29 cf[	 ]+subl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 44 2b f9[	 ]+subl.s %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 11 29 cf[	 ]+subl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 2b 39[	 ]+subl   \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 29 39[	 ]+subl   %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*d5 11 85 cf[	 ]+testl  %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 44 85 f9[	 ]+testl  %r31d,%ecx
[	 ]*[a-f0-9]+:[	 ]*d5 11 85 cf[	 ]+testl  %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 85 39[	 ]+testl  %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 85 39[	 ]+testl  %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*d5 11 31 cf[	 ]+xorl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 44 33 f9[	 ]+xorl.s %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 11 31 cf[	 ]+xorl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 33 39[	 ]+xorl   \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 31 39[	 ]+xorl   %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*d5 11 87 cf[	 ]+xchgl  %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*d5 44 87 f9[	 ]+xchgl  %r31d,%ecx
[	 ]*[a-f0-9]+:[	 ]*d5 11 87 cf[	 ]+xchgl  %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 87 39[	 ]+xchgl  %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*67 d5 44 87 39[	 ]+xchgl  %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*d5 91 28 17[	 ]+movaps \(%r31\),%xmm2
[	 ]*[a-f0-9]+:[	 ]*d5 91 28 17[	 ]+movaps \(%r31\),%xmm2
[	 ]*[a-f0-9]+:[	 ]*d5 91 29 17[	 ]+movaps %xmm2,\(%r31\)
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 08 11 cf[	 ]+adcl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 64 7c 08 13 f9[	 ]+adcl.s %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 08 11 cf[	 ]+adcl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 7c 08 13 39[	 ]+adcl   \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 7c 08 11 39[	 ]+adcl   %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 08 01 cf[	 ]+addl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 64 7c 08 03 f9[	 ]+addl.s %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 08 01 cf[	 ]+addl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 7c 08 03 39[	 ]+addl   \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 7c 08 01 39[	 ]+addl   %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 08 21 cf[	 ]+andl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 64 7c 08 23 f9[	 ]+andl.s %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 08 21 cf[	 ]+andl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 7c 08 23 39[	 ]+andl   \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 7c 08 21 39[	 ]+andl   %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*62 dc 04 0a 39 cf[	 ]+ccmptl \{dfv=\} %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 64 04 0a 3b f9[	 ]+ccmptl.s \{dfv=\} %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 dc 04 0a 39 cf[	 ]+ccmptl \{dfv=\} %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 04 0a 3b 39[	 ]+ccmptl \{dfv=\} \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 04 0a 39 39[	 ]+ccmptl \{dfv=\} %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 08 09 cf[	 ]+orl    %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 64 7c 08 0b f9[	 ]+orl.s  %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 08 09 cf[	 ]+orl    %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 7c 08 0b 39[	 ]+orl    \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 7c 08 09 39[	 ]+orl    %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 08 19 cf[	 ]+sbbl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 64 7c 08 1b f9[	 ]+sbbl.s %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 08 19 cf[	 ]+sbbl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 7c 08 1b 39[	 ]+sbbl   \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 7c 08 19 39[	 ]+sbbl   %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 08 29 cf[	 ]+subl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 64 7c 08 2b f9[	 ]+subl.s %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 08 29 cf[	 ]+subl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 7c 08 2b 39[	 ]+subl   \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 7c 08 29 39[	 ]+subl   %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*62 dc 04 0a 85 cf[	 ]+ctesttl \{dfv=\} %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 64 04 0a 85 f9[	 ]+ctesttl \{dfv=\} %r31d,%ecx
[	 ]*[a-f0-9]+:[	 ]*62 dc 04 0a 85 cf[	 ]+ctesttl \{dfv=\} %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 04 0a 85 39[	 ]+ctesttl \{dfv=\} %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*67 62 64 04 0a 85 39[	 ]+ctesttl \{dfv=\} %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 08 31 cf[	 ]+xorl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 64 7c 08 33 f9[	 ]+xorl.s %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 08 31 cf[	 ]+xorl   %ecx,%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 7c 08 33 39[	 ]+xorl   \(%ecx\),%r31d
[	 ]*[a-f0-9]+:[	 ]*67 62 64 7c 08 31 39[	 ]+xorl   %r31d,\(%ecx\)
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 18 11 cf[	 ]+adcl   %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 64 7c 18 13 f9[	 ]+adcl.s %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 18 11 cf[	 ]+adcl   %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 11 38[	 ]+adcq   %r31,\(%r8\),%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 13 38[	 ]+adcq   \(%r8\),%r31,%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 11 38[	 ]+adcq   %r31,\(%r8\),%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 13 38[	 ]+adcq   \(%r8\),%r31,%r16
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 18 01 cf[	 ]+addl   %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 64 7c 18 03 f9[	 ]+addl.s %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 18 01 cf[	 ]+addl   %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 01 38[	 ]+addq   %r31,\(%r8\),%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 03 38[	 ]+addq   \(%r8\),%r31,%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 01 38[	 ]+addq   %r31,\(%r8\),%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 03 38[	 ]+addq   \(%r8\),%r31,%r16
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 18 21 cf[	 ]+andl   %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 64 7c 18 23 f9[	 ]+andl.s %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 18 21 cf[	 ]+andl   %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 21 38[	 ]+andq   %r31,\(%r8\),%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 23 38[	 ]+andq   \(%r8\),%r31,%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 21 38[	 ]+andq   %r31,\(%r8\),%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 23 38[	 ]+andq   \(%r8\),%r31,%r16
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 18 09 cf[	 ]+orl    %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 64 7c 18 0b f9[	 ]+orl.s  %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 18 09 cf[	 ]+orl    %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 09 38[	 ]+orq    %r31,\(%r8\),%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 0b 38[	 ]+orq    \(%r8\),%r31,%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 09 38[	 ]+orq    %r31,\(%r8\),%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 0b 38[	 ]+orq    \(%r8\),%r31,%r16
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 18 19 cf[	 ]+sbbl   %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 64 7c 18 1b f9[	 ]+sbbl.s %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 18 19 cf[	 ]+sbbl   %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 19 38[	 ]+sbbq   %r31,\(%r8\),%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 1b 38[	 ]+sbbq   \(%r8\),%r31,%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 19 38[	 ]+sbbq   %r31,\(%r8\),%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 1b 38[	 ]+sbbq   \(%r8\),%r31,%r16
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 18 29 cf[	 ]+subl   %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 64 7c 18 2b f9[	 ]+subl.s %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 18 29 cf[	 ]+subl   %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 29 38[	 ]+subq   %r31,\(%r8\),%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 2b 38[	 ]+subq   \(%r8\),%r31,%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 29 38[	 ]+subq   %r31,\(%r8\),%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 2b 38[	 ]+subq   \(%r8\),%r31,%r16
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 18 31 cf[	 ]+xorl   %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 64 7c 18 33 f9[	 ]+xorl.s %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 dc 7c 18 31 cf[	 ]+xorl   %ecx,%r31d,%eax
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 31 38[	 ]+xorq   %r31,\(%r8\),%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 33 38[	 ]+xorq   \(%r8\),%r31,%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 31 38[	 ]+xorq   %r31,\(%r8\),%r16
[	 ]*[a-f0-9]+:[	 ]*62 44 fc 10 33 38[	 ]+xorq   \(%r8\),%r31,%r16
[	 ]*[a-f0-9]+:[	 ]*62 f4 04 02 39 d0[	 ]+ccmpbl \{dfv=\} %edx,%eax
[	 ]*[a-f0-9]+:[	 ]*62 f4 04 02 3b c2[	 ]+ccmpbl.s \{dfv=\} %edx,%eax
[	 ]*[a-f0-9]+:[	 ]*62 f4 04 02 39 d0[	 ]+ccmpbl \{dfv=\} %edx,%eax
[	 ]*[a-f0-9]+:[	 ]*67 62 f4 04 02 3b 02[	 ]+ccmpbl \{dfv=\} \(%edx\),%eax
[	 ]*[a-f0-9]+:[	 ]*67 62 f4 04 02 39 02[	 ]+ccmpbl \{dfv=\} %eax,\(%edx\)
[	 ]*[a-f0-9]+:[	 ]*62 f4 7c 08 42 c2[	 ]+cfcmovbl %edx,%eax
[	 ]*[a-f0-9]+:[	 ]*62 f4 7c 08 42 c2[	 ]+cfcmovbl %edx,%eax
[	 ]*[a-f0-9]+:[	 ]*62 f4 7c 0c 42 d0[	 ]+cfcmovbl.s %edx,%eax
[	 ]*[a-f0-9]+:[	 ]*67 62 f4 7c 08 42 02[	 ]+cfcmovbl \(%edx\),%eax
[	 ]*[a-f0-9]+:[	 ]*67 62 f4 7c 0c 42 02[	 ]+cfcmovbl %eax,\(%edx\)
[	 ]*[a-f0-9]+:[	 ]*62 f4 04 02 85 d0[	 ]+ctestbl \{dfv=\} %edx,%eax
[	 ]*[a-f0-9]+:[	 ]*62 f4 04 02 85 c2[	 ]+ctestbl \{dfv=\} %eax,%edx
[	 ]*[a-f0-9]+:[	 ]*62 f4 04 02 85 d0[	 ]+ctestbl \{dfv=\} %edx,%eax
[	 ]*[a-f0-9]+:[	 ]*67 62 f4 04 02 85 02[	 ]+ctestbl \{dfv=\} %eax,\(%edx\)
[	 ]*[a-f0-9]+:[	 ]*67 62 f4 04 02 85 02[	 ]+ctestbl \{dfv=\} %eax,\(%edx\)
#pass
