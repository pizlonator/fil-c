# Check 64bit APX ZU instructions with evex prefix encoding

	.allow_index_reg
	.text
_start:
	imulzu $10, %bx, %ax
	imulzu $10, (%eax), %ax
	imulzu $10, %r17w, %r16w
	imulzu $10, (%r16d), %r31w
	imulzu $10, %r31w
	imulzu $9090, %bx, %ax
	imulzu $9090, (%eax), %ax
	imulzu $9090, %r17w, %r16w
	imulzu $9090, (%r16d), %r31w
	imulzu $9090, %r31w
	{nf}   imulzu $10, %bx, %ax
	{nf}   imulzu $10, (%eax), %ax
	{nf}   imul $10, %bx, %ax       # ZU=0
	{nf}   imul $10, (%eax), %ax    # ZU=0
	{evex} imul $10, %bx, %ax       # ZU=0
	{evex} imul $10, (%eax), %ax    # ZU=0
	setzuno  %al
	setzub   %bl
	setzuae  %r8b
	setzue   %r9b
	setzune  %r10b
	setzube  %r16b
	setzua   %r17b
	setzus   %r18b
	setzuns  %r19b
	setzup   %r20b
	setzunp  %r21b
	setzul   %r22b
	setzuge  %r23b
	setzule  %r24b
	setzug   %r25b
	setg     %r25b
	{evex} setg  %al
	{evex} setg  (%rax)
	setpe    %eax
	setpo    %rax

	.intel_syntax noprefix
	imulzu ax,bx, 10
	imulzu ax,WORD PTR [eax],10
	imulzu r16w,r17w,10
	imulzu r31w,WORD PTR [r16d],10
	imulzu r31w,r31w,10
	imulzu ax,bx,9090
	imulzu ax,WORD PTR [eax],9090
	imulzu r16w,r17w,9090
	imulzu r31w,WORD PTR [r16d],9090
	imulzu r31w,r31w,9090
	{nf} imulzu ax,bx, 10
	{nf} imulzu ax,WORD PTR [eax],10
	{nf} imul ax,bx, 10               # ZU=0
	{nf} imul ax,WORD PTR [eax],10    # ZU=0
	{evex} imul ax,bx, 10             # ZU=0
	{evex} imul ax,WORD PTR [eax],10  # ZU=0
	setzuno  al
	setzub   bl
	setzuae  r8b
	setzue   r9b
	setzune  r10b
	setzube  r16b
	setzua   r17b
	setzus   r18b
	setzuns  r19b
	setzup   r20b
	setzunp  r21b
	setzul   r22b
	setzuge  r23b
	setzule  r24b
	setzug   r25b
	setg     r25b
	{evex} setg  al
