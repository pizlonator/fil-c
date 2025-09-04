# Testcase for DWARF regnum ginsn API
	.text
	.globl  foo
	.type   foo, @function
foo:
	push   %rbp
	mov    %rsp, %rbp

	mov    -0x2(%rax), %al
	mov    -0x2(%rax), %bl
	mov    -0x2(%rax), %dl
	mov    -0x2(%rax), %ah
	mov    -0x2(%rax), %ch
	mov    -0x2(%rax), %dh
	mov    -0x2(%rax), %bh

	mov    -0x2(%rax), %axl
	mov    -0x2(%rax), %spl

	mov    -0x2(%rax), %ax
	mov    -0x2(%rax), %r8w
	mov    -0x2(%rax), %sp

	add    $8, %spl

	mov    -0x2(%rax), %eax
	mov    -0x2(%rax), %r8d

	add    $16384, %esp

	add    $8, %rbp

	lea    -0x2(%rip), %rax
	mov    %rax, 0x2(%eip)
	mov    -0x2(%eip), %rax

	mov    %rbp, %rsp
	pop    %rbp
	ret
.LFE0:
	.size   foo, .-foo
