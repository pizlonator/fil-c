.globl	foo
.globl	foo_hidden1
.globl	foo_hidden2
.globl	foo_protected

.type	foo, %gnu_indirect_function
.type	foo_hidden1, %gnu_indirect_function
.type	foo_hidden2, %gnu_indirect_function
.type	foo_protected, %gnu_indirect_function
.type	foo_internal, %gnu_indirect_function

.hidden	foo_hidden1
.hidden	foo_hidden2

.protected	foo_protected

.globl	ext_ifunc1
.globl	ext_ifunc2
.type	ext_ifunc1, %gnu_indirect_function
.type	ext_ifunc2, %gnu_indirect_function

.text
.align	2
foo:
	ret

foo_hidden1:
	ret

foo_hidden2:
	ret

foo_protected:
	ret

foo_internal:
	ret

test:
	la.got	$a0, num
	# The order is deliberately shuffled.
	bl	ext_ifunc1
	bl	foo
	bl	foo_hidden1
	bl	ext_func1
	bl	foo_protected
	bl	foo_internal
	bl	foo_hidden2
	bl	ext_func2
	bl	ext_ifunc2

.data
.align	3
num:
	.quad	114514
