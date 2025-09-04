#as:
#objdump: -dw
#name: x86_64 APX_F insns with evex pseudo prefix
#source: x86-64-apx_f-evex.s

.*: +file format .*

Disassembly of section \.text:

0+ <_start>:
\s*[a-f0-9]+:\s*62 54 fc 08 fc bc 80 23 01 00 00\s+\{evex\} aadd\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 7c 08 fc bc 80 23 01 00 00\s+\{evex\} aadd\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fd 08 fc bc 80 23 01 00 00\s+\{evex\} aand\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 7d 08 fc bc 80 23 01 00 00\s+\{evex\} aand\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 83 d7 7b\s+\{evex\} adc\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 83 d7 7b\s+\{evex\} adc\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 83 d7 7b\s+\{evex\} adc\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 80 d0 7b\s+\{evex\} adc\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 80 94 80 23 01 00 00 7b\s+\{evex\} adcb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 83 94 80 23 01 00 00 7b\s+\{evex\} adcw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 83 94 80 23 01 00 00 7b\s+\{evex\} adcl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 83 94 80 23 01 00 00 7b\s+\{evex\} adcq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 11 ff\s+\{evex\} adc\s+%r15,%r15
\s*[a-f0-9]+:\s*62 54 fc 08 11 bc 80 23 01 00 00\s+\{evex\} adc\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 11 fa\s+\{evex\} adc\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 54 7c 08 11 bc 80 23 01 00 00\s+\{evex\} adc\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7d 08 11 f8\s+\{evex\} adc\s+%r15w,%ax
\s*[a-f0-9]+:\s*62 54 7d 08 11 bc 80 23 01 00 00\s+\{evex\} adc\s+%r15w,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 10 c2\s+\{evex\} adc\s+%r8b,%dl
\s*[a-f0-9]+:\s*62 54 7c 08 10 84 80 23 01 00 00\s+\{evex\} adc\s+%r8b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 13 bc 80 23 01 00 00\s+\{evex\} adc\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7c 08 13 bc 80 23 01 00 00\s+\{evex\} adc\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 54 7d 08 13 bc 80 23 01 00 00\s+\{evex\} adc\s+0x123\(%r8,%rax,4\),%r15w
\s*[a-f0-9]+:\s*62 54 7c 08 12 84 80 23 01 00 00\s+\{evex\} adc\s+0x123\(%r8,%rax,4\),%r8b
\s*[a-f0-9]+:\s*62 54 fd 08 66 ff\s+\{evex\} adcx\s+%r15,%r15
\s*[a-f0-9]+:\s*62 d4 7d 08 66 d7\s+\{evex\} adcx\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 54 fd 08 66 bc 80 23 01 00 00\s+\{evex\} adcx\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7d 08 66 bc 80 23 01 00 00\s+\{evex\} adcx\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 d4 fc 08 83 c7 7b\s+\{evex\} add\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 83 c7 7b\s+\{evex\} add\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 83 c7 7b\s+\{evex\} add\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 80 c0 7b\s+\{evex\} add\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 80 84 80 23 01 00 00 7b\s+\{evex\} addb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 83 84 80 23 01 00 00 7b\s+\{evex\} addw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 83 84 80 23 01 00 00 7b\s+\{evex\} addl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 83 84 80 23 01 00 00 7b\s+\{evex\} addq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 01 ff\s+\{evex\} add\s+%r15,%r15
\s*[a-f0-9]+:\s*62 54 fc 08 01 bc 80 23 01 00 00\s+\{evex\} add\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 01 fa\s+\{evex\} add\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 54 7c 08 01 bc 80 23 01 00 00\s+\{evex\} add\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7d 08 01 f8\s+\{evex\} add\s+%r15w,%ax
\s*[a-f0-9]+:\s*62 54 7d 08 01 bc 80 23 01 00 00\s+\{evex\} add\s+%r15w,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 00 c2\s+\{evex\} add\s+%r8b,%dl
\s*[a-f0-9]+:\s*62 54 7c 08 00 84 80 23 01 00 00\s+\{evex\} add\s+%r8b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 03 bc 80 23 01 00 00\s+\{evex\} add\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7c 08 03 bc 80 23 01 00 00\s+\{evex\} add\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 54 7d 08 03 bc 80 23 01 00 00\s+\{evex\} add\s+0x123\(%r8,%rax,4\),%r15w
\s*[a-f0-9]+:\s*62 54 7c 08 02 84 80 23 01 00 00\s+\{evex\} add\s+0x123\(%r8,%rax,4\),%r8b
\s*[a-f0-9]+:\s*62 54 fe 08 66 ff\s+\{evex\} adox\s+%r15,%r15
\s*[a-f0-9]+:\s*62 d4 7e 08 66 d7\s+\{evex\} adox\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 54 fe 08 66 bc 80 23 01 00 00\s+\{evex\} adox\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7e 08 66 bc 80 23 01 00 00\s+\{evex\} adox\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 d4 fc 08 83 e7 7b\s+\{evex\} and\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 83 e7 7b\s+\{evex\} and\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 83 e7 7b\s+\{evex\} and\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 80 e0 7b\s+\{evex\} and\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 80 a4 80 23 01 00 00 7b\s+\{evex\} andb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 83 a4 80 23 01 00 00 7b\s+\{evex\} andw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 83 a4 80 23 01 00 00 7b\s+\{evex\} andl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 83 a4 80 23 01 00 00 7b\s+\{evex\} andq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 21 ff\s+\{evex\} and\s+%r15,%r15
\s*[a-f0-9]+:\s*62 54 fc 08 21 bc 80 23 01 00 00\s+\{evex\} and\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 21 fa\s+\{evex\} and\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 54 7c 08 21 bc 80 23 01 00 00\s+\{evex\} and\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7d 08 21 f8\s+\{evex\} and\s+%r15w,%ax
\s*[a-f0-9]+:\s*62 54 7d 08 21 bc 80 23 01 00 00\s+\{evex\} and\s+%r15w,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 20 c2\s+\{evex\} and\s+%r8b,%dl
\s*[a-f0-9]+:\s*62 54 7c 08 20 84 80 23 01 00 00\s+\{evex\} and\s+%r8b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 23 bc 80 23 01 00 00\s+\{evex\} and\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7c 08 23 bc 80 23 01 00 00\s+\{evex\} and\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 54 7d 08 23 bc 80 23 01 00 00\s+\{evex\} and\s+0x123\(%r8,%rax,4\),%r15w
\s*[a-f0-9]+:\s*62 54 7c 08 22 84 80 23 01 00 00\s+\{evex\} and\s+0x123\(%r8,%rax,4\),%r8b
\s*[a-f0-9]+:\s*62 52 84 08 f2 df\s+\{evex\} andn\s+%r15,%r15,%r11
\s*[a-f0-9]+:\s*62 52 6c 08 f2 d7\s+\{evex\} andn\s+%r15d,%edx,%r10d
\s*[a-f0-9]+:\s*62 52 84 08 f2 bc 80 23 01 00 00\s+\{evex\} andn\s+0x123\(%r8,%rax,4\),%r15,%r15
\s*[a-f0-9]+:\s*62 d2 04 08 f2 94 80 23 01 00 00\s+\{evex\} andn\s+0x123\(%r8,%rax,4\),%r15d,%edx
\s*[a-f0-9]+:\s*62 54 ff 08 fc bc 80 23 01 00 00\s+\{evex\} aor\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 7f 08 fc bc 80 23 01 00 00\s+\{evex\} aor\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fe 08 fc bc 80 23 01 00 00\s+\{evex\} axor\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 7e 08 fc bc 80 23 01 00 00\s+\{evex\} axor\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 84 08 f7 df\s+\{evex\} bextr\s+%r15,%r15,%r11
\s*[a-f0-9]+:\s*62 52 84 08 f7 bc 80 23 01 00 00\s+\{evex\} bextr\s+%r15,0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 72 04 08 f7 d2\s+\{evex\} bextr\s+%r15d,%edx,%r10d
\s*[a-f0-9]+:\s*62 d2 04 08 f7 94 80 23 01 00 00\s+\{evex\} bextr\s+%r15d,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 d2 84 08 f3 df\s+\{evex\} blsi\s+%r15,%r15
\s*[a-f0-9]+:\s*62 d2 6c 08 f3 df\s+\{evex\} blsi\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 d2 84 08 f3 9c 80 23 01 00 00\s+\{evex\} blsi\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 d2 04 08 f3 9c 80 23 01 00 00\s+\{evex\} blsi\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 d2 84 08 f3 d7\s+\{evex\} blsmsk\s+%r15,%r15
\s*[a-f0-9]+:\s*62 d2 6c 08 f3 d7\s+\{evex\} blsmsk\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 d2 84 08 f3 94 80 23 01 00 00\s+\{evex\} blsmsk\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 d2 04 08 f3 94 80 23 01 00 00\s+\{evex\} blsmsk\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 d2 84 08 f3 cf\s+\{evex\} blsr\s+%r15,%r15
\s*[a-f0-9]+:\s*62 d2 6c 08 f3 cf\s+\{evex\} blsr\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 d2 84 08 f3 8c 80 23 01 00 00\s+\{evex\} blsr\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 d2 04 08 f3 8c 80 23 01 00 00\s+\{evex\} blsr\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 52 84 08 f5 df\s+\{evex\} bzhi\s+%r15,%r15,%r11
\s*[a-f0-9]+:\s*62 52 84 08 f5 bc 80 23 01 00 00\s+\{evex\} bzhi\s+%r15,0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 72 04 08 f5 d2\s+\{evex\} bzhi\s+%r15d,%edx,%r10d
\s*[a-f0-9]+:\s*62 d2 04 08 f5 94 80 23 01 00 00\s+\{evex\} bzhi\s+%r15d,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 52 85 08 e6 bc 80 23 01 00 00\s+\{evex\} cmpbexadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 e6 94 80 23 01 00 00\s+\{evex\} cmpbexadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 e2 bc 80 23 01 00 00\s+\{evex\} cmpbxadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 e2 94 80 23 01 00 00\s+\{evex\} cmpbxadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 ee bc 80 23 01 00 00\s+\{evex\} cmplexadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 ee 94 80 23 01 00 00\s+\{evex\} cmplexadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 ec bc 80 23 01 00 00\s+\{evex\} cmplxadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 ec 94 80 23 01 00 00\s+\{evex\} cmplxadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 e7 bc 80 23 01 00 00\s+\{evex\} cmpaxadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 e7 94 80 23 01 00 00\s+\{evex\} cmpaxadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 e3 bc 80 23 01 00 00\s+\{evex\} cmpaexadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 e3 94 80 23 01 00 00\s+\{evex\} cmpaexadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 ef bc 80 23 01 00 00\s+\{evex\} cmpgxadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 ef 94 80 23 01 00 00\s+\{evex\} cmpgxadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 ed bc 80 23 01 00 00\s+\{evex\} cmpgexadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 ed 94 80 23 01 00 00\s+\{evex\} cmpgexadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 e1 bc 80 23 01 00 00\s+\{evex\} cmpnoxadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 e1 94 80 23 01 00 00\s+\{evex\} cmpnoxadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 eb bc 80 23 01 00 00\s+\{evex\} cmpnpxadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 eb 94 80 23 01 00 00\s+\{evex\} cmpnpxadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 e9 bc 80 23 01 00 00\s+\{evex\} cmpnsxadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 e9 94 80 23 01 00 00\s+\{evex\} cmpnsxadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 e5 bc 80 23 01 00 00\s+\{evex\} cmpnexadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 e5 94 80 23 01 00 00\s+\{evex\} cmpnexadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 e0 bc 80 23 01 00 00\s+\{evex\} cmpoxadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 e0 94 80 23 01 00 00\s+\{evex\} cmpoxadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 ea bc 80 23 01 00 00\s+\{evex\} cmppxadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 ea 94 80 23 01 00 00\s+\{evex\} cmppxadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 e8 bc 80 23 01 00 00\s+\{evex\} cmpsxadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 e8 94 80 23 01 00 00\s+\{evex\} cmpsxadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 e4 bc 80 23 01 00 00\s+\{evex\} cmpexadd\s+%r15,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d2 05 08 e4 94 80 23 01 00 00\s+\{evex\} cmpexadd\s+%r15d,%edx,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 ff cf\s+\{evex\} dec\s+%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 ff cf\s+\{evex\} dec\s+%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 ff cf\s+\{evex\} dec\s+%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 fe c8\s+\{evex\} dec\s+%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 fe 8c 80 23 01 00 00\s+\{evex\} decb\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 ff 8c 80 23 01 00 00\s+\{evex\} decw\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 ff 8c 80 23 01 00 00\s+\{evex\} decl\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 ff 8c 80 23 01 00 00\s+\{evex\} decq\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 f7 f7\s+\{evex\} div\s+%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 f7 f7\s+\{evex\} div\s+%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 f7 f7\s+\{evex\} div\s+%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 f6 f0\s+\{evex\} div\s+%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 f6 b4 80 23 01 00 00\s+\{evex\} divb\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 f7 b4 80 23 01 00 00\s+\{evex\} divw\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 f7 b4 80 23 01 00 00\s+\{evex\} divl\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 f7 b4 80 23 01 00 00\s+\{evex\} divq\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 7f 08 f8 bc 80 23 01 00 00\s+\{evex\} enqcmd\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*67 62 54 7f 08 f8 bc 80 23 01 00 00\s+\{evex\} enqcmd\s+0x123\(%r8d,%eax,4\),%r15d
\s*[a-f0-9]+:\s*62 54 7e 08 f8 bc 80 23 01 00 00\s+\{evex\} enqcmds\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*67 62 54 7e 08 f8 bc 80 23 01 00 00\s+\{evex\} enqcmds\s+0x123\(%r8d,%eax,4\),%r15d
\s*[a-f0-9]+:\s*62 d4 fc 08 f7 ff\s+\{evex\} idiv\s+%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 f7 ff\s+\{evex\} idiv\s+%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 f7 ff\s+\{evex\} idiv\s+%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 f6 f8\s+\{evex\} idiv\s+%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 f6 bc 80 23 01 00 00\s+\{evex\} idivb\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 f7 bc 80 23 01 00 00\s+\{evex\} idivw\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 f7 bc 80 23 01 00 00\s+\{evex\} idivl\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 f7 bc 80 23 01 00 00\s+\{evex\} idivq\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 f7 ef\s+\{evex\} imul\s+%r15
\s*[a-f0-9]+:\s*62 54 fc 08 af ff\s+\{evex\} imul\s+%r15,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 f7 ef\s+\{evex\} imul\s+%r15d
\s*[a-f0-9]+:\s*62 d4 7c 08 af d7\s+\{evex\} imul\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 d4 7d 08 f7 ef\s+\{evex\} imul\s+%r15w
\s*[a-f0-9]+:\s*62 d4 7d 08 af c7\s+\{evex\} imul\s+%r15w,%ax
\s*[a-f0-9]+:\s*62 d4 7c 08 f6 e8\s+\{evex\} imul\s+%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 f6 ac 80 23 01 00 00\s+\{evex\} imulb\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 f7 ac 80 23 01 00 00\s+\{evex\} imulw\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 f7 ac 80 23 01 00 00\s+\{evex\} imull\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 af bc 80 23 01 00 00\s+\{evex\} imul\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7c 08 af bc 80 23 01 00 00\s+\{evex\} imul\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 54 7d 08 af bc 80 23 01 00 00\s+\{evex\} imul\s+0x123\(%r8,%rax,4\),%r15w
\s*[a-f0-9]+:\s*62 d4 fc 08 f7 ac 80 23 01 00 00\s+\{evex\} imulq\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 f4 7d 08 6b c2 7b\s+\{evex\} imul \$0x7b,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 08 6b d1 7b\s+\{evex\} imul \$0x7b,%ecx,%edx
\s*[a-f0-9]+:\s*62 54 fc 08 6b f9 7b\s+\{evex\} imul \$0x7b,%r9,%r15
\s*[a-f0-9]+:\s*62 54 fc 08 6b c9 7b\s+\{evex\} imul \$0x7b,%r9,%r9
\s*[a-f0-9]+:\s*62 d4 7d 08 6b 94 80 23 01 00 00 7b\s+\{evex\} imul \$0x7b,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 08 6b 8c 80 23 01 00 00 7b\s+\{evex\} imul \$0x7b,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 54 fc 08 6b 8c 80 23 01 00 00 7b\s+\{evex\} imul \$0x7b,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 f4 7d 08 6b c2 90\s+\{evex\} imul \$0xff90,%dx,%ax
\s*[a-f0-9]+:\s*62 f4 7c 08 69 d1 90 ff 00 00\s+\{evex\} imul \$0xff90,%ecx,%edx
\s*[a-f0-9]+:\s*62 54 fc 08 69 f9 90 ff 00 00\s+\{evex\} imul \$0xff90,%r9,%r15
\s*[a-f0-9]+:\s*62 54 fc 08 69 c9 90 ff 00 00\s+\{evex\} imul \$0xff90,%r9,%r9
\s*[a-f0-9]+:\s*62 d4 7d 08 6b 94 80 23 01 00 00 90\s+\{evex\} imul \$0xff90,0x123\(%r8,%rax,4\),%dx
\s*[a-f0-9]+:\s*62 d4 7c 08 69 8c 80 23 01 00 00 90 ff 00 00\s+\{evex\} imul \$0xff90,0x123\(%r8,%rax,4\),%ecx
\s*[a-f0-9]+:\s*62 54 fc 08 69 8c 80 23 01 00 00 90 ff 00 00\s+\{evex\} imul \$0xff90,0x123\(%r8,%rax,4\),%r9
\s*[a-f0-9]+:\s*62 d4 fc 08 ff c7\s+\{evex\} inc\s+%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 ff c7\s+\{evex\} inc\s+%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 ff c7\s+\{evex\} inc\s+%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 fe c0\s+\{evex\} inc\s+%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 fe 84 80 23 01 00 00\s+\{evex\} incb\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 ff 84 80 23 01 00 00\s+\{evex\} incw\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 ff 84 80 23 01 00 00\s+\{evex\} incl\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 ff 84 80 23 01 00 00\s+\{evex\} incq\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 7e 08 f0 bc 80 23 01 00 00\s+\{evex\} invept\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7e 08 f2 bc 80 23 01 00 00\s+\{evex\} invpcid\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7e 08 f1 bc 80 23 01 00 00\s+\{evex\} invvpid\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 f1 7d 08 90 eb\s+\{evex\} kmovb\s+%k3,%k5
\s*[a-f0-9]+:\s*62 71 7d 08 93 fd\s+\{evex\} kmovb\s+%k5,%r15d
\s*[a-f0-9]+:\s*62 d1 7d 08 91 ac 80 23 01 00 00\s+\{evex\} kmovb\s+%k5,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d1 7d 08 92 ef\s+\{evex\} kmovb\s+%r15d,%k5
\s*[a-f0-9]+:\s*62 d1 7d 08 90 ac 80 23 01 00 00\s+\{evex\} kmovb\s+0x123\(%r8,%rax,4\),%k5
\s*[a-f0-9]+:\s*62 f1 fd 08 90 eb\s+\{evex\} kmovd\s+%k3,%k5
\s*[a-f0-9]+:\s*62 71 7f 08 93 fd\s+\{evex\} kmovd\s+%k5,%r15d
\s*[a-f0-9]+:\s*62 d1 fd 08 91 ac 80 23 01 00 00\s+\{evex\} kmovd\s+%k5,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d1 7f 08 92 ef\s+\{evex\} kmovd\s+%r15d,%k5
\s*[a-f0-9]+:\s*62 d1 fd 08 90 ac 80 23 01 00 00\s+\{evex\} kmovd\s+0x123\(%r8,%rax,4\),%k5
\s*[a-f0-9]+:\s*62 f1 fc 08 90 eb\s+\{evex\} kmovq\s+%k3,%k5
\s*[a-f0-9]+:\s*62 71 ff 08 93 fd\s+\{evex\} kmovq\s+%k5,%r15
\s*[a-f0-9]+:\s*62 d1 fc 08 91 ac 80 23 01 00 00\s+\{evex\} kmovq\s+%k5,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d1 ff 08 92 ef\s+\{evex\} kmovq\s+%r15,%k5
\s*[a-f0-9]+:\s*62 d1 fc 08 90 ac 80 23 01 00 00\s+\{evex\} kmovq\s+0x123\(%r8,%rax,4\),%k5
\s*[a-f0-9]+:\s*62 f1 7c 08 90 eb\s+\{evex\} kmovw\s+%k3,%k5
\s*[a-f0-9]+:\s*62 71 7c 08 93 fd\s+\{evex\} kmovw\s+%k5,%r15d
\s*[a-f0-9]+:\s*62 d1 7c 08 91 ac 80 23 01 00 00\s+\{evex\} kmovw\s+%k5,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d1 7c 08 92 ef\s+\{evex\} kmovw\s+%r15d,%k5
\s*[a-f0-9]+:\s*62 d1 7c 08 90 ac 80 23 01 00 00\s+\{evex\} kmovw\s+0x123\(%r8,%rax,4\),%k5
\s*[a-f0-9]+:\s*62 54 fc 08 f5 ff\s+\{evex\} lzcnt\s+%r15,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 f5 d7\s+\{evex\} lzcnt\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 d4 7d 08 f5 c7\s+\{evex\} lzcnt\s+%r15w,%ax
\s*[a-f0-9]+:\s*62 54 fc 08 f5 bc 80 23 01 00 00\s+\{evex\} lzcnt\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7c 08 f5 bc 80 23 01 00 00\s+\{evex\} lzcnt\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 54 7d 08 f5 bc 80 23 01 00 00\s+\{evex\} lzcnt\s+0x123\(%r8,%rax,4\),%r15w
\s*[a-f0-9]+:\s*62 54 fc 08 61 bc 80 23 01 00 00\s+\{evex\} movbe\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 7c 08 61 bc 80 23 01 00 00\s+\{evex\} movbe\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 7d 08 61 bc 80 23 01 00 00\s+\{evex\} movbe\s+%r15w,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 60 bc 80 23 01 00 00\s+\{evex\} movbe\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7c 08 60 bc 80 23 01 00 00\s+\{evex\} movbe\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 54 7d 08 60 bc 80 23 01 00 00\s+\{evex\} movbe\s+0x123\(%r8,%rax,4\),%r15w
\s*[a-f0-9]+:\s*62 54 7d 08 f8 bc 80 23 01 00 00\s+\{evex\} movdir64b\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*67 62 54 7d 08 f8 bc 80 23 01 00 00\s+\{evex\} movdir64b\s+0x123\(%r8d,%eax,4\),%r15d
\s*[a-f0-9]+:\s*62 54 fc 08 f9 bc 80 23 01 00 00\s+\{evex\} movdiri\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 7c 08 f9 bc 80 23 01 00 00\s+\{evex\} movdiri\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 f7 e7\s+\{evex\} mul\s+%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 f7 e7\s+\{evex\} mul\s+%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 f7 e7\s+\{evex\} mul\s+%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 f6 e0\s+\{evex\} mul\s+%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 f6 a4 80 23 01 00 00\s+\{evex\} mulb\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 f7 a4 80 23 01 00 00\s+\{evex\} mulw\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 f7 a4 80 23 01 00 00\s+\{evex\} mull\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 f7 a4 80 23 01 00 00\s+\{evex\} mulq\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 87 08 f6 df\s+\{evex\} mulx\s+%r15,%r15,%r11
\s*[a-f0-9]+:\s*62 52 6f 08 f6 d7\s+\{evex\} mulx\s+%r15d,%edx,%r10d
\s*[a-f0-9]+:\s*62 52 87 08 f6 bc 80 23 01 00 00\s+\{evex\} mulx\s+0x123\(%r8,%rax,4\),%r15,%r15
\s*[a-f0-9]+:\s*62 d2 07 08 f6 94 80 23 01 00 00\s+\{evex\} mulx\s+0x123\(%r8,%rax,4\),%r15d,%edx
\s*[a-f0-9]+:\s*62 d4 fc 08 f7 df\s+\{evex\} neg\s+%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 f7 df\s+\{evex\} neg\s+%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 f7 df\s+\{evex\} neg\s+%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 f6 d8\s+\{evex\} neg\s+%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 f6 9c 80 23 01 00 00\s+\{evex\} negb\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 f7 9c 80 23 01 00 00\s+\{evex\} negw\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 f7 9c 80 23 01 00 00\s+\{evex\} negl\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 f7 9c 80 23 01 00 00\s+\{evex\} negq\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 f7 d7\s+\{evex\} not\s+%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 f7 d7\s+\{evex\} not\s+%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 f7 d7\s+\{evex\} not\s+%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 f6 d0\s+\{evex\} not\s+%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 f6 94 80 23 01 00 00\s+\{evex\} notb\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 f7 94 80 23 01 00 00\s+\{evex\} notw\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 f7 94 80 23 01 00 00\s+\{evex\} notl\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 f7 94 80 23 01 00 00\s+\{evex\} notq\s+0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 83 cf 7b\s+\{evex\} or\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 83 cf 7b\s+\{evex\} or\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 83 cf 7b\s+\{evex\} or\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 80 c8 7b\s+\{evex\} or\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 80 8c 80 23 01 00 00 7b\s+\{evex\} orb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 83 8c 80 23 01 00 00 7b\s+\{evex\} orw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 83 8c 80 23 01 00 00 7b\s+\{evex\} orl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 83 8c 80 23 01 00 00 7b\s+\{evex\} orq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 09 ff\s+\{evex\} or\s+%r15,%r15
\s*[a-f0-9]+:\s*62 54 fc 08 09 bc 80 23 01 00 00\s+\{evex\} or\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 09 fa\s+\{evex\} or\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 54 7c 08 09 bc 80 23 01 00 00\s+\{evex\} or\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7d 08 09 f8\s+\{evex\} or\s+%r15w,%ax
\s*[a-f0-9]+:\s*62 54 7d 08 09 bc 80 23 01 00 00\s+\{evex\} or\s+%r15w,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 08 c2\s+\{evex\} or\s+%r8b,%dl
\s*[a-f0-9]+:\s*62 54 7c 08 08 84 80 23 01 00 00\s+\{evex\} or\s+%r8b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 0b bc 80 23 01 00 00\s+\{evex\} or\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7c 08 0b bc 80 23 01 00 00\s+\{evex\} or\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 54 7d 08 0b bc 80 23 01 00 00\s+\{evex\} or\s+0x123\(%r8,%rax,4\),%r15w
\s*[a-f0-9]+:\s*62 54 7c 08 0a 84 80 23 01 00 00\s+\{evex\} or\s+0x123\(%r8,%rax,4\),%r8b
\s*[a-f0-9]+:\s*62 52 87 08 f5 df\s+\{evex\} pdep\s+%r15,%r15,%r11
\s*[a-f0-9]+:\s*62 52 6f 08 f5 d7\s+\{evex\} pdep\s+%r15d,%edx,%r10d
\s*[a-f0-9]+:\s*62 52 87 08 f5 bc 80 23 01 00 00\s+\{evex\} pdep\s+0x123\(%r8,%rax,4\),%r15,%r15
\s*[a-f0-9]+:\s*62 d2 07 08 f5 94 80 23 01 00 00\s+\{evex\} pdep\s+0x123\(%r8,%rax,4\),%r15d,%edx
\s*[a-f0-9]+:\s*62 52 86 08 f5 df\s+\{evex\} pext\s+%r15,%r15,%r11
\s*[a-f0-9]+:\s*62 52 6e 08 f5 d7\s+\{evex\} pext\s+%r15d,%edx,%r10d
\s*[a-f0-9]+:\s*62 52 86 08 f5 bc 80 23 01 00 00\s+\{evex\} pext\s+0x123\(%r8,%rax,4\),%r15,%r15
\s*[a-f0-9]+:\s*62 d2 06 08 f5 94 80 23 01 00 00\s+\{evex\} pext\s+0x123\(%r8,%rax,4\),%r15d,%edx
\s*[a-f0-9]+:\s*62 54 fc 08 88 ff\s+\{evex\} popcnt\s+%r15,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 88 d7\s+\{evex\} popcnt\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 d4 7d 08 88 c7\s+\{evex\} popcnt\s+%r15w,%ax
\s*[a-f0-9]+:\s*62 54 fc 08 88 bc 80 23 01 00 00\s+\{evex\} popcnt\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7c 08 88 bc 80 23 01 00 00\s+\{evex\} popcnt\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 54 7d 08 88 bc 80 23 01 00 00\s+\{evex\} popcnt\s+0x123\(%r8,%rax,4\),%r15w
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 d7 7b\s+\{evex\} rcl\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 d7 7b\s+\{evex\} rcl\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 d7 7b\s+\{evex\} rcl\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 d0 7b\s+\{evex\} rcl\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 94 80 23 01 00 00 7b\s+\{evex\} rclb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 94 80 23 01 00 00 7b\s+\{evex\} rclw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 94 80 23 01 00 00 7b\s+\{evex\} rcll\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 94 80 23 01 00 00 7b\s+\{evex\} rclq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 d7\s+\{evex\} rcl\s+\$1,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 d7\s+\{evex\} rcl\s+\$1,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 d7\s+\{evex\} rcl\s+\$1,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 d0\s+\{evex\} rcl\s+\$1,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 94 80 23 01 00 00\s+\{evex\} rclb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 94 80 23 01 00 00\s+\{evex\} rclw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 94 80 23 01 00 00\s+\{evex\} rcll\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 94 80 23 01 00 00\s+\{evex\} rclq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 d7\s+\{evex\} rcl\s+%cl,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 d7\s+\{evex\} rcl\s+%cl,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 d7\s+\{evex\} rcl\s+%cl,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 d0\s+\{evex\} rcl\s+%cl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 94 80 23 01 00 00\s+\{evex\} rclb\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 94 80 23 01 00 00\s+\{evex\} rclw\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 94 80 23 01 00 00\s+\{evex\} rcll\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 94 80 23 01 00 00\s+\{evex\} rclq\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 df 7b\s+\{evex\} rcr\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 df 7b\s+\{evex\} rcr\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 df 7b\s+\{evex\} rcr\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 d8 7b\s+\{evex\} rcr\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 9c 80 23 01 00 00 7b\s+\{evex\} rcrb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 9c 80 23 01 00 00 7b\s+\{evex\} rcrw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 9c 80 23 01 00 00 7b\s+\{evex\} rcrl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 9c 80 23 01 00 00 7b\s+\{evex\} rcrq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 df\s+\{evex\} rcr\s+\$1,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 df\s+\{evex\} rcr\s+\$1,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 df\s+\{evex\} rcr\s+\$1,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 d8\s+\{evex\} rcr\s+\$1,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 9c 80 23 01 00 00\s+\{evex\} rcrb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 9c 80 23 01 00 00\s+\{evex\} rcrw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 9c 80 23 01 00 00\s+\{evex\} rcrl\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 9c 80 23 01 00 00\s+\{evex\} rcrq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 df\s+\{evex\} rcr\s+%cl,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 df\s+\{evex\} rcr\s+%cl,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 df\s+\{evex\} rcr\s+%cl,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 d8\s+\{evex\} rcr\s+%cl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 9c 80 23 01 00 00\s+\{evex\} rcrb\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 9c 80 23 01 00 00\s+\{evex\} rcrw\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 9c 80 23 01 00 00\s+\{evex\} rcrl\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 9c 80 23 01 00 00\s+\{evex\} rcrq\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 c7 7b\s+\{evex\} rol\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 c7 7b\s+\{evex\} rol\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 c7 7b\s+\{evex\} rol\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 c0 7b\s+\{evex\} rol\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 84 80 23 01 00 00 7b\s+\{evex\} rolb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 84 80 23 01 00 00 7b\s+\{evex\} rolw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 84 80 23 01 00 00 7b\s+\{evex\} roll\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 84 80 23 01 00 00 7b\s+\{evex\} rolq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 c7\s+\{evex\} rol\s+\$1,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 c7\s+\{evex\} rol\s+\$1,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 c7\s+\{evex\} rol\s+\$1,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 c0\s+\{evex\} rol\s+\$1,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 84 80 23 01 00 00\s+\{evex\} rolb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 84 80 23 01 00 00\s+\{evex\} rolw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 84 80 23 01 00 00\s+\{evex\} roll\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 84 80 23 01 00 00\s+\{evex\} rolq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 c7\s+\{evex\} rol\s+%cl,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 c7\s+\{evex\} rol\s+%cl,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 c7\s+\{evex\} rol\s+%cl,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 c0\s+\{evex\} rol\s+%cl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 84 80 23 01 00 00\s+\{evex\} rolb\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 84 80 23 01 00 00\s+\{evex\} rolw\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 84 80 23 01 00 00\s+\{evex\} roll\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 84 80 23 01 00 00\s+\{evex\} rolq\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 cf 7b\s+\{evex\} ror\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 cf 7b\s+\{evex\} ror\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 cf 7b\s+\{evex\} ror\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 c8 7b\s+\{evex\} ror\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 8c 80 23 01 00 00 7b\s+\{evex\} rorb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 8c 80 23 01 00 00 7b\s+\{evex\} rorw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 8c 80 23 01 00 00 7b\s+\{evex\} rorl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 8c 80 23 01 00 00 7b\s+\{evex\} rorq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 cf\s+\{evex\} ror\s+\$1,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 cf\s+\{evex\} ror\s+\$1,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 cf\s+\{evex\} ror\s+\$1,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 c8\s+\{evex\} ror\s+\$1,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 8c 80 23 01 00 00\s+\{evex\} rorb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 8c 80 23 01 00 00\s+\{evex\} rorw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 8c 80 23 01 00 00\s+\{evex\} rorl\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 8c 80 23 01 00 00\s+\{evex\} rorq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 cf\s+\{evex\} ror\s+%cl,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 cf\s+\{evex\} ror\s+%cl,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 cf\s+\{evex\} ror\s+%cl,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 c8\s+\{evex\} ror\s+%cl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 8c 80 23 01 00 00\s+\{evex\} rorb\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 8c 80 23 01 00 00\s+\{evex\} rorw\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 8c 80 23 01 00 00\s+\{evex\} rorl\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 8c 80 23 01 00 00\s+\{evex\} rorq\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 53 ff 08 f0 ff 7b\s+\{evex\} rorx\s+\$0x7b,%r15,%r15
\s*[a-f0-9]+:\s*62 d3 7f 08 f0 d7 7b\s+\{evex\} rorx\s+\$0x7b,%r15d,%edx
\s*[a-f0-9]+:\s*62 53 ff 08 f0 bc 80 23 01 00 00 7b\s+\{evex\} rorx\s+\$0x7b,0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 53 7f 08 f0 bc 80 23 01 00 00 7b\s+\{evex\} rorx\s+\$0x7b,0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 e7 7b\s+\{evex\} shl\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 e7 7b\s+\{evex\} shl\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 e7 7b\s+\{evex\} shl\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 e0 7b\s+\{evex\} shl\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 a4 80 23 01 00 00 7b\s+\{evex\} shlb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 a4 80 23 01 00 00 7b\s+\{evex\} shlw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 a4 80 23 01 00 00 7b\s+\{evex\} shll\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 a4 80 23 01 00 00 7b\s+\{evex\} shlq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 e7\s+\{evex\} shl\s+\$1,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 e7\s+\{evex\} shl\s+\$1,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 e7\s+\{evex\} shl\s+\$1,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 e0\s+\{evex\} shl\s+\$1,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 a4 80 23 01 00 00\s+\{evex\} shlb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 a4 80 23 01 00 00\s+\{evex\} shlw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 a4 80 23 01 00 00\s+\{evex\} shll\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 a4 80 23 01 00 00\s+\{evex\} shlq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 e7\s+\{evex\} shl\s+%cl,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 e7\s+\{evex\} shl\s+%cl,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 e7\s+\{evex\} shl\s+%cl,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 e0\s+\{evex\} shl\s+%cl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 a4 80 23 01 00 00\s+\{evex\} shlb\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 a4 80 23 01 00 00\s+\{evex\} shlw\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 a4 80 23 01 00 00\s+\{evex\} shll\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 a4 80 23 01 00 00\s+\{evex\} shlq\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 ff 7b\s+\{evex\} sar\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 ff 7b\s+\{evex\} sar\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 ff 7b\s+\{evex\} sar\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 f8 7b\s+\{evex\} sar\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 bc 80 23 01 00 00 7b\s+\{evex\} sarb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 bc 80 23 01 00 00 7b\s+\{evex\} sarw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 bc 80 23 01 00 00 7b\s+\{evex\} sarl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 bc 80 23 01 00 00 7b\s+\{evex\} sarq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 ff\s+\{evex\} sar\s+\$1,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 ff\s+\{evex\} sar\s+\$1,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 ff\s+\{evex\} sar\s+\$1,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 f8\s+\{evex\} sar\s+\$1,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 bc 80 23 01 00 00\s+\{evex\} sarb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 bc 80 23 01 00 00\s+\{evex\} sarw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 bc 80 23 01 00 00\s+\{evex\} sarl\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 bc 80 23 01 00 00\s+\{evex\} sarq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 ff\s+\{evex\} sar\s+%cl,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 ff\s+\{evex\} sar\s+%cl,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 ff\s+\{evex\} sar\s+%cl,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 f8\s+\{evex\} sar\s+%cl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 bc 80 23 01 00 00\s+\{evex\} sarb\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 bc 80 23 01 00 00\s+\{evex\} sarw\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 bc 80 23 01 00 00\s+\{evex\} sarl\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 bc 80 23 01 00 00\s+\{evex\} sarq\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 86 08 f7 df\s+\{evex\} sarx\s+%r15,%r15,%r11
\s*[a-f0-9]+:\s*62 52 86 08 f7 bc 80 23 01 00 00\s+\{evex\} sarx\s+%r15,0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 72 06 08 f7 d2\s+\{evex\} sarx\s+%r15d,%edx,%r10d
\s*[a-f0-9]+:\s*62 d2 06 08 f7 94 80 23 01 00 00\s+\{evex\} sarx\s+%r15d,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 d4 fc 08 83 df 7b\s+\{evex\} sbb\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 83 df 7b\s+\{evex\} sbb\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 83 df 7b\s+\{evex\} sbb\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 80 d8 7b\s+\{evex\} sbb\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 80 9c 80 23 01 00 00 7b\s+\{evex\} sbbb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 83 9c 80 23 01 00 00 7b\s+\{evex\} sbbw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 83 9c 80 23 01 00 00 7b\s+\{evex\} sbbl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 83 9c 80 23 01 00 00 7b\s+\{evex\} sbbq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 19 ff\s+\{evex\} sbb\s+%r15,%r15
\s*[a-f0-9]+:\s*62 54 fc 08 19 bc 80 23 01 00 00\s+\{evex\} sbb\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 19 fa\s+\{evex\} sbb\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 54 7c 08 19 bc 80 23 01 00 00\s+\{evex\} sbb\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7d 08 19 f8\s+\{evex\} sbb\s+%r15w,%ax
\s*[a-f0-9]+:\s*62 54 7d 08 19 bc 80 23 01 00 00\s+\{evex\} sbb\s+%r15w,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 18 c2\s+\{evex\} sbb\s+%r8b,%dl
\s*[a-f0-9]+:\s*62 54 7c 08 18 84 80 23 01 00 00\s+\{evex\} sbb\s+%r8b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 1b bc 80 23 01 00 00\s+\{evex\} sbb\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7c 08 1b bc 80 23 01 00 00\s+\{evex\} sbb\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 54 7d 08 1b bc 80 23 01 00 00\s+\{evex\} sbb\s+0x123\(%r8,%rax,4\),%r15w
\s*[a-f0-9]+:\s*62 54 7c 08 1a 84 80 23 01 00 00\s+\{evex\} sbb\s+0x123\(%r8,%rax,4\),%r8b
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 e7 7b\s+\{evex\} shl\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 e7 7b\s+\{evex\} shl\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 e7 7b\s+\{evex\} shl\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 e0 7b\s+\{evex\} shl\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 a4 80 23 01 00 00 7b\s+\{evex\} shlb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 a4 80 23 01 00 00 7b\s+\{evex\} shlw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 a4 80 23 01 00 00 7b\s+\{evex\} shll\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 a4 80 23 01 00 00 7b\s+\{evex\} shlq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 e7\s+\{evex\} shl\s+\$1,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 e7\s+\{evex\} shl\s+\$1,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 e7\s+\{evex\} shl\s+\$1,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 e0\s+\{evex\} shl\s+\$1,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 a4 80 23 01 00 00\s+\{evex\} shlb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 a4 80 23 01 00 00\s+\{evex\} shlw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 a4 80 23 01 00 00\s+\{evex\} shll\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 a4 80 23 01 00 00\s+\{evex\} shlq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 e7\s+\{evex\} shl\s+%cl,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 e7\s+\{evex\} shl\s+%cl,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 e7\s+\{evex\} shl\s+%cl,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 e0\s+\{evex\} shl\s+%cl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 a4 80 23 01 00 00\s+\{evex\} shlb\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 a4 80 23 01 00 00\s+\{evex\} shlw\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 a4 80 23 01 00 00\s+\{evex\} shll\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 a4 80 23 01 00 00\s+\{evex\} shlq\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 24 ff 7b\s+\{evex\} shld\s+\$0x7b,%r15,%r15
\s*[a-f0-9]+:\s*62 54 fc 08 24 bc 80 23 01 00 00 7b\s+\{evex\} shld\s+\$0x7b,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 24 fa 7b\s+\{evex\} shld\s+\$0x7b,%r15d,%edx
\s*[a-f0-9]+:\s*62 54 7c 08 24 bc 80 23 01 00 00 7b\s+\{evex\} shld\s+\$0x7b,%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7d 08 24 f8 7b\s+\{evex\} shld\s+\$0x7b,%r15w,%ax
\s*[a-f0-9]+:\s*62 54 7d 08 24 bc 80 23 01 00 00 7b\s+\{evex\} shld\s+\$0x7b,%r15w,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 a5 ff\s+\{evex\} shld\s+%cl,%r15,%r15
\s*[a-f0-9]+:\s*62 54 fc 08 a5 bc 80 23 01 00 00\s+\{evex\} shld\s+%cl,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 a5 fa\s+\{evex\} shld\s+%cl,%r15d,%edx
\s*[a-f0-9]+:\s*62 54 7c 08 a5 bc 80 23 01 00 00\s+\{evex\} shld\s+%cl,%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7d 08 a5 f8\s+\{evex\} shld\s+%cl,%r15w,%ax
\s*[a-f0-9]+:\s*62 54 7d 08 a5 bc 80 23 01 00 00\s+\{evex\} shld\s+%cl,%r15w,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 85 08 f7 df\s+\{evex\} shlx\s+%r15,%r15,%r11
\s*[a-f0-9]+:\s*62 52 85 08 f7 bc 80 23 01 00 00\s+\{evex\} shlx\s+%r15,0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 72 05 08 f7 d2\s+\{evex\} shlx\s+%r15d,%edx,%r10d
\s*[a-f0-9]+:\s*62 d2 05 08 f7 94 80 23 01 00 00\s+\{evex\} shlx\s+%r15d,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 ef 7b\s+\{evex\} shr\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 ef 7b\s+\{evex\} shr\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 ef 7b\s+\{evex\} shr\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 e8 7b\s+\{evex\} shr\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 c0 ac 80 23 01 00 00 7b\s+\{evex\} shrb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 c1 ac 80 23 01 00 00 7b\s+\{evex\} shrw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 c1 ac 80 23 01 00 00 7b\s+\{evex\} shrl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 c1 ac 80 23 01 00 00 7b\s+\{evex\} shrq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 ef\s+\{evex\} shr\s+\$1,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 ef\s+\{evex\} shr\s+\$1,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 ef\s+\{evex\} shr\s+\$1,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 e8\s+\{evex\} shr\s+\$1,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d0 ac 80 23 01 00 00\s+\{evex\} shrb\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d1 ac 80 23 01 00 00\s+\{evex\} shrw\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d1 ac 80 23 01 00 00\s+\{evex\} shrl\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d1 ac 80 23 01 00 00\s+\{evex\} shrq\s+\$1,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 ef\s+\{evex\} shr\s+%cl,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 ef\s+\{evex\} shr\s+%cl,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 ef\s+\{evex\} shr\s+%cl,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 e8\s+\{evex\} shr\s+%cl,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 d2 ac 80 23 01 00 00\s+\{evex\} shrb\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 d3 ac 80 23 01 00 00\s+\{evex\} shrw\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 d3 ac 80 23 01 00 00\s+\{evex\} shrl\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 d3 ac 80 23 01 00 00\s+\{evex\} shrq\s+%cl,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 2c ff 7b\s+\{evex\} shrd\s+\$0x7b,%r15,%r15
\s*[a-f0-9]+:\s*62 54 fc 08 2c bc 80 23 01 00 00 7b\s+\{evex\} shrd\s+\$0x7b,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 2c fa 7b\s+\{evex\} shrd\s+\$0x7b,%r15d,%edx
\s*[a-f0-9]+:\s*62 54 7c 08 2c bc 80 23 01 00 00 7b\s+\{evex\} shrd\s+\$0x7b,%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7d 08 2c f8 7b\s+\{evex\} shrd\s+\$0x7b,%r15w,%ax
\s*[a-f0-9]+:\s*62 54 7d 08 2c bc 80 23 01 00 00 7b\s+\{evex\} shrd\s+\$0x7b,%r15w,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 ad ff\s+\{evex\} shrd\s+%cl,%r15,%r15
\s*[a-f0-9]+:\s*62 54 fc 08 ad bc 80 23 01 00 00\s+\{evex\} shrd\s+%cl,%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 ad fa\s+\{evex\} shrd\s+%cl,%r15d,%edx
\s*[a-f0-9]+:\s*62 54 7c 08 ad bc 80 23 01 00 00\s+\{evex\} shrd\s+%cl,%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7d 08 ad f8\s+\{evex\} shrd\s+%cl,%r15w,%ax
\s*[a-f0-9]+:\s*62 54 7d 08 ad bc 80 23 01 00 00\s+\{evex\} shrd\s+%cl,%r15w,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 52 87 08 f7 df\s+\{evex\} shrx\s+%r15,%r15,%r11
\s*[a-f0-9]+:\s*62 52 87 08 f7 bc 80 23 01 00 00\s+\{evex\} shrx\s+%r15,0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 72 07 08 f7 d2\s+\{evex\} shrx\s+%r15d,%edx,%r10d
\s*[a-f0-9]+:\s*62 d2 07 08 f7 94 80 23 01 00 00\s+\{evex\} shrx\s+%r15d,0x123\(%r8,%rax,4\),%edx
\s*[a-f0-9]+:\s*62 d4 fc 08 83 ef 7b\s+\{evex\} sub\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 83 ef 7b\s+\{evex\} sub\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 83 ef 7b\s+\{evex\} sub\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 80 e8 7b\s+\{evex\} sub\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 80 ac 80 23 01 00 00 7b\s+\{evex\} subb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 83 ac 80 23 01 00 00 7b\s+\{evex\} subw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 83 ac 80 23 01 00 00 7b\s+\{evex\} subl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 83 ac 80 23 01 00 00 7b\s+\{evex\} subq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 29 ff\s+\{evex\} sub\s+%r15,%r15
\s*[a-f0-9]+:\s*62 54 fc 08 29 bc 80 23 01 00 00\s+\{evex\} sub\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 29 fa\s+\{evex\} sub\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 54 7c 08 29 bc 80 23 01 00 00\s+\{evex\} sub\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7d 08 29 f8\s+\{evex\} sub\s+%r15w,%ax
\s*[a-f0-9]+:\s*62 54 7d 08 29 bc 80 23 01 00 00\s+\{evex\} sub\s+%r15w,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 28 c2\s+\{evex\} sub\s+%r8b,%dl
\s*[a-f0-9]+:\s*62 54 7c 08 28 84 80 23 01 00 00\s+\{evex\} sub\s+%r8b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 2b bc 80 23 01 00 00\s+\{evex\} sub\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7c 08 2b bc 80 23 01 00 00\s+\{evex\} sub\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 54 7d 08 2b bc 80 23 01 00 00\s+\{evex\} sub\s+0x123\(%r8,%rax,4\),%r15w
\s*[a-f0-9]+:\s*62 54 7c 08 2a 84 80 23 01 00 00\s+\{evex\} sub\s+0x123\(%r8,%rax,4\),%r8b
\s*[a-f0-9]+:\s*62 54 fc 08 f4 ff\s+\{evex\} tzcnt\s+%r15,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 f4 d7\s+\{evex\} tzcnt\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 d4 7d 08 f4 c7\s+\{evex\} tzcnt\s+%r15w,%ax
\s*[a-f0-9]+:\s*62 54 fc 08 f4 bc 80 23 01 00 00\s+\{evex\} tzcnt\s+0x123\(%r8,%rax,4\),%r15
\s*[a-f0-9]+:\s*62 54 7c 08 f4 bc 80 23 01 00 00\s+\{evex\} tzcnt\s+0x123\(%r8,%rax,4\),%r15d
\s*[a-f0-9]+:\s*62 54 7d 08 f4 bc 80 23 01 00 00\s+\{evex\} tzcnt\s+0x123\(%r8,%rax,4\),%r15w
\s*[a-f0-9]+:\s*62 54 7c 08 66 bc 80 23 01 00 00\s+\{evex\} wrssd\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 66 bc 80 23 01 00 00\s+\{evex\} wrssq\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 7d 08 65 bc 80 23 01 00 00\s+\{evex\} wrussd\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fd 08 65 bc 80 23 01 00 00\s+\{evex\} wrussq\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 83 f7 7b\s+\{evex\} xor\s+\$0x7b,%r15
\s*[a-f0-9]+:\s*62 d4 7c 08 83 f7 7b\s+\{evex\} xor\s+\$0x7b,%r15d
\s*[a-f0-9]+:\s*62 d4 7d 08 83 f7 7b\s+\{evex\} xor\s+\$0x7b,%r15w
\s*[a-f0-9]+:\s*62 d4 7c 08 80 f0 7b\s+\{evex\} xor\s+\$0x7b,%r8b
\s*[a-f0-9]+:\s*62 d4 7c 08 80 b4 80 23 01 00 00 7b\s+\{evex\} xorb\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7d 08 83 b4 80 23 01 00 00 7b\s+\{evex\} xorw\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 7c 08 83 b4 80 23 01 00 00 7b\s+\{evex\} xorl\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 d4 fc 08 83 b4 80 23 01 00 00 7b\s+\{evex\} xorq\s+\$0x7b,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 54 fc 08 31 ff\s+\{evex\} xor\s+%r15,%r15
\s*[a-f0-9]+:\s*62 54 fc 08 31 bc 80 23 01 00 00\s+\{evex\} xor\s+%r15,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 31 fa\s+\{evex\} xor\s+%r15d,%edx
\s*[a-f0-9]+:\s*62 54 7c 08 31 bc 80 23 01 00 00\s+\{evex\} xor\s+%r15d,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7d 08 31 f8\s+\{evex\} xor\s+%r15w,%ax
\s*[a-f0-9]+:\s*62 54 7d 08 31 bc 80 23 01 00 00\s+\{evex\} xor\s+%r15w,0x123\(%r8,%rax,4\)
\s*[a-f0-9]+:\s*62 74 7c 08 30 c2\s+\{evex\} xor\s+%r8b,%dl
\s*[a-f0-9]+:\s*62 54 7c 08 30 84 80 23 01 00 00\s+\{evex\} xor\s+%r8b,0x123\(%r8,%rax,4\)
#pass
