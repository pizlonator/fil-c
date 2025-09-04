	.intel_syntax noprefix
	.text
left:
	shld	eax, ecx, 1
	shld	eax, ecx, cl

	shld	ecx, 1
	shld	ecx, cl

	sald	eax, ecx, 1
	sald	eax, ecx, cl

	sald	ecx, 1
	sald	ecx, cl

	rold	eax, ecx, 1
	rold	eax, ecx, cl

	rold	ecx, 1
	rold	ecx, cl

right:
	shrd	eax, ecx, 1
	shrd	eax, ecx, cl

	shrd	ecx, 1
	shrd	ecx, cl

	sard	eax, ecx, 1
	sard	eax, ecx, cl

	sard	ecx, 1
	sard	ecx, cl

	rord	eax, ecx, 1
	rord	eax, ecx, cl

	rord	ecx, 1
	rord	ecx, cl
