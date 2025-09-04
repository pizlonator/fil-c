# Check illegal 64bit APX_F instructions
	.text
	addq $0xe0, %fs:0, %rdx
	xorq $0xe0, foo(%eax,%edx), %rdx
	{nf} adc $1, (%rax)
	{nf} not %r15
	{nf} rcl $0x7b,%r15
	{nf} rcr $0x7b,%r15
	{nf} sbb $0x7b,%r15
	{nf} pop2 %rax, %rbx
	{nf} push2 %rbx, %rax
	{nf} adcx %r15,%r15
	{nf} mulx %r15,%r15,%r11
	{nf} cfcmovl %dx,291(%r8,%rax,4)
	{nf} cfcmovl 291(%r8,%rax,4),%dx
	{nf} cfcmovb %dx,%ax,%r31w
	{nf} {vex} bextr %ecx, %edx, %r10d
	{vex} {nf} bextr %ecx, %edx, %r10d
	{nf} add %dl,%ah
	{evex} adc %dl,%ah
