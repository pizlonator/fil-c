#objdump: -dw
#name: Intel syntax w/ suffixes
#warning_output: intel-suffix.e

.*: +file format .*

Disassembly of section \.text:
0+0 <.*>:
[ 	]*[a-f0-9]+:	0f a4 c8 01[ 	]+shld   \$0x1,%ecx,%eax
[ 	]*[a-f0-9]+:	0f a5 c8[ 	]+shld   %cl,%ecx,%eax
[ 	]*[a-f0-9]+:	d1 e1[ 	]+shl    \$1,%ecx
[ 	]*[a-f0-9]+:	d3 e1[ 	]+shl    %cl,%ecx
[ 	]*[a-f0-9]+:	62 f4 7c 18 d1 e1[ 	]+shl    \$1,%ecx,%eax
[ 	]*[a-f0-9]+:	62 f4 7c 18 d3 e1[ 	]+shl    %cl,%ecx,%eax
[ 	]*[a-f0-9]+:	d1 e1[ 	]+shl    \$1,%ecx
[ 	]*[a-f0-9]+:	d3 e1[ 	]+shl    %cl,%ecx
[ 	]*[a-f0-9]+:	62 f4 7c 18 d1 c1[ 	]+rol    \$1,%ecx,%eax
[ 	]*[a-f0-9]+:	62 f4 7c 18 d3 c1[ 	]+rol    %cl,%ecx,%eax
[ 	]*[a-f0-9]+:	d1 c1[ 	]+rol    \$1,%ecx
[ 	]*[a-f0-9]+:	d3 c1[ 	]+rol    %cl,%ecx

0+[0-9a-f]+ <.*>:
[ 	]*[a-f0-9]+:	0f ac c8 01[ 	]+shrd   \$0x1,%ecx,%eax
[ 	]*[a-f0-9]+:	0f ad c8[ 	]+shrd   %cl,%ecx,%eax
[ 	]*[a-f0-9]+:	d1 e9[ 	]+shr    \$1,%ecx
[ 	]*[a-f0-9]+:	d3 e9[ 	]+shr    %cl,%ecx
[ 	]*[a-f0-9]+:	62 f4 7c 18 d1 f9[ 	]+sar    \$1,%ecx,%eax
[ 	]*[a-f0-9]+:	62 f4 7c 18 d3 f9[ 	]+sar    %cl,%ecx,%eax
[ 	]*[a-f0-9]+:	d1 f9[ 	]+sar    \$1,%ecx
[ 	]*[a-f0-9]+:	d3 f9[ 	]+sar    %cl,%ecx
[ 	]*[a-f0-9]+:	62 f4 7c 18 d1 c9[ 	]+ror    \$1,%ecx,%eax
[ 	]*[a-f0-9]+:	62 f4 7c 18 d3 c9[ 	]+ror    %cl,%ecx,%eax
[ 	]*[a-f0-9]+:	d1 c9[ 	]+ror    \$1,%ecx
[ 	]*[a-f0-9]+:	d3 c9[ 	]+ror    %cl,%ecx
#pass
