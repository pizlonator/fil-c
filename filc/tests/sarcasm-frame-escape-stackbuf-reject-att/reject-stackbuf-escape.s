# UNSOUND: a `#! stack buffer (...)` declaration covering part of the fixed
# frame PLUS a frame-address escape from ANOTHER part of the frame. The D9
# fixed-frame escape promotion materializes the WHOLE fixed frame as a GC
# region, and the buffer's bytes would then have two homes (the buffer's own
# lowered region and the promoted region), so sarcasm must reject the
# combination instead of silently giving the buffer's bytes two uncoordinated
# homes. (The escaping lea sits ABOVE the buffer's range: a lea INTO the
# buffer's own bytes is a buffer-interior value lea, which is not an escape
# and does not promote the frame.)
	.text
	.globl	badescape
	.type	badescape, @function
badescape:                      #! long(long)
	subq	$128, %rsp          # fixed frame [0,128)
	movq	%rdi, %rcx          # park the argument (the buffer index)
	leaq	96(%rsp), %rdi      # the frame escape (frame+96, above the buffer):
	movq	%rdi, %rsi          # the whole fixed frame is promoted to a GC
	call	fill32 ;! void(ptr) # region by the helper call
	movl	(%rsp,%rcx), %eax #! stack buffer (buf, %rsp, %rsp + 64)
	addq	$128, %rsp
	ret
	.size	badescape, .-badescape
	.section	.note.GNU-stack,"",@progbits
