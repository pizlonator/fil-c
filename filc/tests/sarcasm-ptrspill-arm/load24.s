	.text
	.globl	load24
	.type	load24, %function
load24:                         ;! long(ptr)
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	ldr	x1, [x0, #0]	;! load ptr
	ldr	x2, [x0, #8]	;! load ptr
	ldr	x3, [x0, #16]	;! load ptr
	ldr	x4, [x0, #24]	;! load ptr
	ldr	x5, [x0, #32]	;! load ptr
	ldr	x6, [x0, #40]	;! load ptr
	ldr	x7, [x0, #48]	;! load ptr
	ldr	x8, [x0, #56]	;! load ptr
	ldr	x9, [x0, #64]	;! load ptr
	ldr	x10, [x0, #72]	;! load ptr
	ldr	x11, [x0, #80]	;! load ptr
	ldr	x12, [x0, #88]	;! load ptr
	ldr	x13, [x0, #96]	;! load ptr
	ldr	x14, [x0, #104]	;! load ptr
	ldr	x15, [x0, #112]	;! load ptr
	ldr	x16, [x0, #120]	;! load ptr
	ldr	x17, [x0, #128]	;! load ptr
	ldr	x18, [x0, #136]	;! load ptr
	ldr	x19, [x0, #144]	;! load ptr
	ldr	x20, [x0, #152]	;! load ptr
	ldr	x21, [x0, #160]	;! load ptr
	ldr	x22, [x0, #168]	;! load ptr
	ldr	x23, [x0, #176]	;! load ptr
	ldr	x24, [x0, #184]	;! load ptr
	bl	five            ;! long()
	mov	x25, #0
	mov	x26, #1
	ldr	x27, [x1]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #2
	ldr	x27, [x2]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #3
	ldr	x27, [x3]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #4
	ldr	x27, [x4]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #5
	ldr	x27, [x5]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #6
	ldr	x27, [x6]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #7
	ldr	x27, [x7]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #8
	ldr	x27, [x8]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #9
	ldr	x27, [x9]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #10
	ldr	x27, [x10]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #11
	ldr	x27, [x11]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #12
	ldr	x27, [x12]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #13
	ldr	x27, [x13]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #14
	ldr	x27, [x14]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #15
	ldr	x27, [x15]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #16
	ldr	x27, [x16]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #17
	ldr	x27, [x17]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #18
	ldr	x27, [x18]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #19
	ldr	x27, [x19]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #20
	ldr	x27, [x20]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #21
	ldr	x27, [x21]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #22
	ldr	x27, [x22]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #23
	ldr	x27, [x23]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x26, #24
	ldr	x27, [x24]
	mul	x27, x27, x26
	add	x25, x25, x27
	mov	x0, x25
	ldp	x29, x30, [sp], #16
	ret
	.size	load24, .-load24
	.globl	five
	.type	five, %function
five:                           ;! long()
	mov	x0, #5
	ret
	.size	five, .-five
	.section	.note.GNU-stack,"",@progbits
