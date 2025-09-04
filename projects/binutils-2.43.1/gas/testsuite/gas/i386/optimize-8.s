# Check bogus instructions are still rejected when optimizing

	.text
_start:
	testw	$4, %ecx
	testl	$4, %cx
