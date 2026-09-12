# B2 NESTED dispatch (the chacha ChaCha20_ctr32 -> .LChaCha20_ssse3 ->
# .LChaCha20_4x shape): a cross-function jump into a mid-body tail whose
# region itself jumps into a THIRD function's mid-body tail, plus a nested
# jump to a function ENTRY (which becomes a call+epilogue returning through
# the jumper). All paths execute and return through the original jumper.
	.text
	.globl	nest_dispatch
	.type	nest_dispatch, @function
nest_dispatch:                  ;! long(long,long,long)
	# mode in %rdi, a in %rsi, b in %rdx.
	testl	%edi, %edi
	jnz	.Lv_ssse3
	# base body: a + b
	movq	%rsi, %rax
	addq	%rdx, %rax
	ret
	.size	nest_dispatch, .-nest_dispatch
	.globl	nest_ssse3
	.type	nest_ssse3, @function
nest_ssse3:                     ;! long(long,long,long)
	# owner entry: a small prologue so the join label is mid-body, then fall
	# into the shared region (direct calls with mode != 2 take the ssse3
	# body; mode == 2 continues into the 4x tail).
	cmpl	$2, %edi
	jne	.Lssse3_only
	jmp	.Lv_4x
.Lssse3_only:
	nop
.Lv_ssse3:
	# shared region start: a nested cross-function exit (to the 4x tail),
	# a nested entry jump (to nest_add_one: call+epilogue), and the ssse3
	# body. Register state (mode/a/b) is shared across both joins.
	cmpl	$2, %edi
	je	.Lv_4x
	cmpl	$3, %edi
	je	nest_add_one
	# ssse3 body: a*2 + b
	movq	%rsi, %rax
	addq	%rax, %rax
	addq	%rdx, %rax
	ret
	.size	nest_ssse3, .-nest_ssse3
	.globl	nest_4x
	.type	nest_4x, @function
nest_4x:                        ;! long(long,long,long)
	# owner entry: direct calls take the small path; the shared 4x tail is
	# the mid-body label below.
	cmpl	$2, %edi
	je	.Lv_4x
	# small path: a + b
	movq	%rsi, %rax
	addq	%rdx, %rax
	ret
.Lv_4x:
	# shared 4x tail: a*4 + b
	movq	%rsi, %rax
	shlq	$2, %rax
	addq	%rdx, %rax
	ret
	.size	nest_4x, .-nest_4x
	.globl	nest_add_one
	.type	nest_add_one, @function
nest_add_one:                   ;! long(long,long,long)
	# nested-ENTRY target: reached via call+epilogue, returns a + 1.
	movq	%rsi, %rax
	addq	$1, %rax
	ret
	.size	nest_add_one, .-nest_add_one
	.section	.note.GNU-stack,"",@progbits
