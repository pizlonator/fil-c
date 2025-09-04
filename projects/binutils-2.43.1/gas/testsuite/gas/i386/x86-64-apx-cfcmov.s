# Check 64bit EVEX-promoted CMOVcc instructions

	.text
_start:
	cfcmovb	%dx,%ax
	cfcmovb	%dx,%ax,%r31w
	cfcmovb	%dx,291(%r8,%rax,4)
	cfcmovb	%ecx,%edx
	cfcmovb	%ecx,%edx,%r10d
	cfcmovb	%ecx,291(%r8,%rax,4)
	cfcmovb	%r31,%r15
	cfcmovb	%r31,%r15,%r11
	cfcmovb	%r31,291(%r8,%rax,4)
	cfcmovb	291(%r8,%rax,4),%dx
	cfcmovb	291(%r8,%rax,4),%dx,%ax
	cfcmovb	291(%r8,%rax,4),%ecx
	cfcmovb	291(%r8,%rax,4),%ecx,%edx
	cfcmovb	291(%r8,%rax,4),%r31
	cfcmovb	291(%r8,%rax,4),%r31,%r15
	cmovb	%dx,%ax,%r31w
	cmovb	%ecx,%edx,%r10d
	cmovb	%r31,%r15,%r11
	cmovb	291(%r8,%rax,4),%dx,%ax
	cmovb	291(%r8,%rax,4),%ecx,%edx
	cmovb	291(%r8,%rax,4),%r31,%r15

	.irp m, o, no, b, nae, nb, ae, e, z, ne, nz, be, na, nbe, a, s, ns, p, pe, np, po, l, nge, nl, ge, le, ng, nle, g
	cfcmov\m  %dx,%ax
	cmov\m	  %dx,%ax,%r31w
	.endr

	.intel_syntax noprefix
	cfcmovb	ax,dx
	cfcmovb	r31w,ax,dx
	cfcmovb	WORD PTR [r8+rax*4+291],dx
	cfcmovb	edx,ecx
	cfcmovb	r10d,edx,ecx
	cfcmovb	DWORD PTR [r8+rax*4+291],ecx
	cfcmovb	r15,r31
	cfcmovb	r11,r15,r31
	cfcmovb	QWORD PTR [r8+rax*4+291],r31
	cfcmovb	dx,WORD PTR [r8+rax*4+291]
	cfcmovb	ax,dx,WORD PTR [r8+rax*4+291]
	cfcmovb	ecx,DWORD PTR [r8+rax*4+291]
	cfcmovb	edx,ecx,DWORD PTR [r8+rax*4+291]
	cfcmovb	r31,QWORD PTR [r8+rax*4+291]
	cfcmovb	r15,r31,QWORD PTR [r8+rax*4+291]
	cmovb   r31w,ax,dx
	cmovb   r10d,edx,ecx
	cmovb   r11,r15,r31
	cmovb   ax,dx,WORD PTR [r8+rax*4+291]
	cmovb   edx,ecx,DWORD PTR [r8+rax*4+291]
	cmovb   r15,r31,QWORD PTR [r8+rax*4+291]

	.irp m, o, no, b, nae, nb, ae, e, z, ne, nz, be, na, nbe, a, s, ns, p, pe, np, po, l, nge, nl, ge, le, ng, nle, g
	cfcmov\m ax,dx
	cmov\m   r31w,ax,dx
	.endr
