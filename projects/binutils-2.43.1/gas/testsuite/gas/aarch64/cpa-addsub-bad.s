addpt	w5, w8, w0
addpt	x5, x8, x0, asr #6
addpt	x5, x8, x0, lsl #9
addpt	x5, x8, sp, lsl #5
addpt	xzr, x8, x0, lsl #3

subpt	w5, w8, w0
subpt	x5, x8, x0, asr #6
subpt	x5, x8, x0, lsl #9
subpt	x5, x8, sp, lsl #5
subpt	xzr, x8, x0, lsl #3

maddpt	w1, x2, x3, x4
maddpt	x1, w2, x3, x4
maddpt	x1, x2, w3, x4
maddpt	x1, x2, x3, w4
maddpt	sp, x2, x3, x4
maddpt	x1, sp, x3, x4
maddpt	x1, x2, sp, x4
maddpt	x1, x2, x3, sp

msubpt	w1, x2, x3, x4
msubpt	x1, w2, x3, x4
msubpt	x1, x2, w3, x4
msubpt	x1, x2, x3, w4
msubpt	sp, x2, x3, x4
msubpt	x1, sp, x3, x4
msubpt	x1, x2, sp, x4
msubpt	x1, x2, x3, sp
