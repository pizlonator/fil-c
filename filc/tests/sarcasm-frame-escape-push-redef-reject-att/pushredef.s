# UNSOUND: an outgoing stack-argument word sharing its slot with an
# outstanding push's save slot whose register was REDEFINED after the push.
# The parked value (`pushq %rbx` parking the %rsi argument pointer) is what
# the callee's stack-passed argument word 0 must observe — the slot still
# holds it — but %rbx's web now holds the redefinition (`movq $7, %rbx`), and
# the web model cannot name the pre-push value at the call. The dropped-save
# model's alias model likewise cannot express the access (a redefined save's
# slot read would see the pre-redefinition value while the register's web
# holds the redefined one). So the callsite fails loudly instead of silently
# marshalling the wrong value:
#   sarcasm: outgoing stack-argument word 0 shares its slot with the
#   outstanding save of %rbx, and %rbx was redefined after the push (the slot
#   still holds the pushed value, which cannot be marshalled; re-spell the
#   arguments, e.g. subq below the push): call sinki
	.text
	.globl	redefn
	.type	redefn, @function
redefn:                         #! long(ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr)
	pushq	%rbx
	subq	$16, %rsp
	movq	%rsi, %rbx      # rbx = arg2 (a pointer)
	pushq	%rbx            # park it
	movq	$7, %rbx        # REDEFINE rbx while parked (integer)
	movq	%rdi, 8(%rsp)   # outgoing word 1 (arg8): buf
	movq	%rdi, %rsi
	movq	%rdi, %rdx
	movq	%rdi, %rcx
	movq	%rdi, %r8
	movq	%rdi, %r9
	call	sinki ;! long(ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr)
	popq	%rbx
	addq	$16, %rsp
	popq	%rbx
	ret
	.size	redefn, .-redefn
	.section	.note.GNU-stack,"",@progbits
