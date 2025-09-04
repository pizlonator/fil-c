addpt   x0, x0, x0
addpt   sp, x0, x0
addpt   x0, sp, x0
addpt   x0, x0, xzr
addpt   x0, x0, x0, lsl #0
addpt   x0, x0, x0, lsl #7
addpt   x8, x13, x29, lsl #5

subpt   x0, x0, x0
subpt   sp, x0, x0
subpt   x0, sp, x0
subpt   x0, x0, xzr
subpt   x0, x0, x0, lsl #0
subpt   x0, x0, x0, lsl #7
subpt   x1, x10, x22, lsl #2

maddpt  x0, x0, x0, x0
maddpt  xzr, x0, x0, x0
maddpt  x0, xzr, x0, x0
maddpt  x0, x0, xzr, x0
maddpt  x0, x0, x0, xzr
maddpt  x19, x10, x1, x28

msubpt  x0, x0, x0, x0
msubpt  xzr, x0, x0, x0
msubpt  x0, xzr, x0, x0
msubpt  x0, x0, xzr, x0
msubpt  x0, x0, x0, xzr
msubpt  x4, x13, x9, x21
