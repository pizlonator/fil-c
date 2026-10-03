/* x87 environment access for Fil-C.

   Fil-C rejects inline assembly with memory operands, so fnstenv, fldenv,
   fnstcw, fldcw and the memory form of fnstsw cannot be used.  The control
   word goes through the runtime (_FPU_GETCW and _FPU_SETCW call zmath_getcw
   and zmath_setcw), and the status word is read with the register form of
   fnstsw.  The x87 status word cannot be loaded: exception flags that would
   be stored there are set in MXCSR instead, which fetestexcept and
   fegetexceptflag also read.  */

#ifndef _FILC_X87_ENV_H
#define _FILC_X87_ENV_H 1

#include <fenv.h>
#include <fpu_control.h>

static __always_inline unsigned int
filc_fnstsw (void)
{
  unsigned short int sw;
  asm volatile ("fnstsw %0" : "=a" (sw));
  return sw;
}

/* Like fnstenv, including its side effect of masking all x87 exceptions.
   The instruction and operand pointers read as zero.  */
static __always_inline void
filc_fnstenv (fenv_t *e)
{
  fpu_control_t cw;
  _FPU_GETCW (cw);
  e->__control_word = cw;
  e->__glibc_reserved1 = 0;
  e->__status_word = filc_fnstsw ();
  e->__glibc_reserved2 = 0;
  e->__tags = 0xffff;
  e->__glibc_reserved3 = 0;
  e->__eip = 0;
  e->__cs_selector = 0;
  e->__opcode = 0;
  e->__glibc_reserved4 = 0;
  e->__data_offset = 0;
  e->__data_selector = 0;
  e->__glibc_reserved5 = 0;
  cw |= 0x3f;
  _FPU_SETCW (cw);
}

/* Like fldenv for the control and status words.  The status word's
   exception flags are moved to MXCSR.  */
static __always_inline void
filc_fldenv (const fenv_t *e)
{
  unsigned int flags = e->__status_word & (FE_ALL_EXCEPT | __FE_DENORM);
  fpu_control_t cw = e->__control_word;
  _FPU_SETCW (cw);
  asm volatile ("fnclex");
  if (flags)
    __builtin_ia32_ldmxcsr (__builtin_ia32_stmxcsr () | flags);
}

/* Raise one exception flag that has no simple SSE operation.  A masked
   exception is only flagged in MXCSR; an unmasked one is raised by an SSE
   operation, which also raises the inexact exception.  */
static __always_inline void
filc_raise_flag (unsigned int flag)
{
  unsigned int mxcsr = __builtin_ia32_stmxcsr ();
  if (mxcsr & (flag << 7))
    {
      __builtin_ia32_ldmxcsr (mxcsr | flag);
      return;
    }
  float x = flag == FE_OVERFLOW ? 0x1p127f : 0x1p-126f;
  float y = flag == FE_OVERFLOW ? 0x1p127f : 0x1p-10f;
  if (flag == FE_INEXACT)
    {
      x = 1.0f;
      y = 3.0f;
      asm volatile ("divss %1, %0" : "+x" (x) : "x" (y));
    }
  else
    asm volatile ("mulss %1, %0" : "+x" (x) : "x" (y));
}

#endif /* filc-x87-env.h */
