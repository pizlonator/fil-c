# protected function should be non-preemptible and relocated with
# R_LARCH_RELATIVE in shared library, for both GOT and pointer data

.globl x
.protected x
.type x, @function
x:
  ret

.globl _start
_start:
  la.got $a0, x
  ret

.data
p:
  .quad x
