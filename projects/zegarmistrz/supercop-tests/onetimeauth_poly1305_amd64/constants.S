# version 20080913
# D. J. Bernstein
# Public domain.

.data
.section .rodata
.p2align 5

.globl _CRYPTO_NAMESPACE(constants)
.globl CRYPTO_NAMESPACE(constants)
.globl CRYPTO_NAMESPACE(scale)
.globl CRYPTO_NAMESPACE(two32)
.globl CRYPTO_NAMESPACE(two64)
.globl CRYPTO_NAMESPACE(two96)
.globl CRYPTO_NAMESPACE(alpha32)
.globl CRYPTO_NAMESPACE(alpha64)
.globl CRYPTO_NAMESPACE(alpha96)
.globl CRYPTO_NAMESPACE(alpha130)
.globl CRYPTO_NAMESPACE(doffset0)
.globl CRYPTO_NAMESPACE(doffset1)
.globl CRYPTO_NAMESPACE(doffset2)
.globl CRYPTO_NAMESPACE(doffset3)
.globl CRYPTO_NAMESPACE(doffset3minustwo128)
.globl CRYPTO_NAMESPACE(hoffset0)
.globl CRYPTO_NAMESPACE(hoffset1)
.globl CRYPTO_NAMESPACE(hoffset2)
.globl CRYPTO_NAMESPACE(hoffset3)
.globl CRYPTO_NAMESPACE(rounding)

_CRYPTO_NAMESPACE(constants):
CRYPTO_NAMESPACE(constants):
CRYPTO_NAMESPACE(scale):
.long 0x0,0x37f40000

CRYPTO_NAMESPACE(two32):
.long 0x0,0x41f00000

CRYPTO_NAMESPACE(two64):
.long 0x0,0x43f00000

CRYPTO_NAMESPACE(two96):
.long 0x0,0x45f00000

CRYPTO_NAMESPACE(alpha32):
.long 0x0,0x45e80000

CRYPTO_NAMESPACE(alpha64):
.long 0x0,0x47e80000

CRYPTO_NAMESPACE(alpha96):
.long 0x0,0x49e80000

CRYPTO_NAMESPACE(alpha130):
.long 0x0,0x4c080000

CRYPTO_NAMESPACE(doffset0):
.long 0x0,0x43300000

CRYPTO_NAMESPACE(doffset1):
.long 0x0,0x45300000

CRYPTO_NAMESPACE(doffset2):
.long 0x0,0x47300000

CRYPTO_NAMESPACE(doffset3):
.long 0x0,0x49300000

CRYPTO_NAMESPACE(doffset3minustwo128):
.long 0x0,0x492ffffe

CRYPTO_NAMESPACE(hoffset0):
.long 0xfffffffb,0x43300001

CRYPTO_NAMESPACE(hoffset1):
.long 0xfffffffe,0x45300001

CRYPTO_NAMESPACE(hoffset2):
.long 0xfffffffe,0x47300001

CRYPTO_NAMESPACE(hoffset3):
.long 0xfffffffe,0x49300003

CRYPTO_NAMESPACE(rounding):
.byte 0x7f
.byte 0x13
.section	.note.GNU-stack,"",@progbits
