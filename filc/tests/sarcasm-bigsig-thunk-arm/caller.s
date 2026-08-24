/* Regression test for the callsite resolver thunk (pizlonatedFI<sig>_<name>):
 *
 *  - Signatures bigger than the 12-bit `cmp` immediate range must be compared
 *    the way pizlonated clang emits them (movz / sub+cmp split / movz+movk),
 *    not as a raw `cmp x8, #<sig>` that GNU as rejects.  This file exercises
 *    all four encodings: sig 1598 (plain cmp), sig 11839 (movz), sig 194714
 *    (sub/cmp split), sig 23561750 (movz+movk).
 *  - The fast tail call must load the canonical entrypoint into the first
 *    register PAST the dense argument words (x4 for 2 words, x5 for 3, x6 for
 *    4, x8 for 6); loading it into x5 unconditionally clobbered the 4th
 *    argument word for >=4-word signatures.
 *
 * The callees live in libbigsig.so with their pizlonatedFI<sig> aliases
 * localized (ver.ld), so the exe's own weak thunks are what actually run.
 */
	.text
	.global	caller2
	.type	caller2, %function
caller2:                        ;! long(long)
	mov	x1, #8
	bl	ext2            ;! long(long, long)
	ret
	.size	caller2, .-caller2
	.global	caller4
	.type	caller4, %function
caller4:                        ;! long(long)
	mov	x1, x0
	mov	x2, x0
	mov	x3, #7
	bl	ext4            ;! long(long, long, long, long)
	ret
	.size	caller4, .-caller4
	.global	caller6
	.type	caller6, %function
caller6:                        ;! long(long)
	mov	x1, x0
	mov	x2, x0
	mov	x3, x0
	mov	x4, x0
	mov	x5, #7
	bl	ext6            ;! long(long, long, long, long, long, long)
	ret
	.size	caller6, .-caller6
	.global	callerp
	.type	callerp, %function
callerp:                        ;! long(ptr)
	mov	x1, x0
	mov	x0, #40
	bl	ext2p           ;! long(long, ptr)
	ret
	.size	callerp, .-callerp
	.global	callerpp
	.type	callerpp, %function
callerpp:                       ;! long(ptr)
	mov	x1, x0
	bl	extpp           ;! long(ptr, ptr)
	ret
	.size	callerpp, .-callerpp
	.global	callerppp
	.type	callerppp, %function
callerppp:                      ;! long(ptr)
	mov	x1, x0
	mov	x2, x0
	bl	extppp          ;! long(ptr, ptr, ptr)
	ret
	.size	callerppp, .-callerppp
	.section	.note.GNU-stack,"",@progbits
