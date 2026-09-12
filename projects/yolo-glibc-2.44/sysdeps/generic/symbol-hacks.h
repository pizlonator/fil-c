/* Some compiler optimizations may transform loops into memset/memmove
   calls and without proper declaration it may generate PLT calls.  */
#if !defined __ASSEMBLER__ && IS_IN (libc) && defined SHARED \
    && !defined LIBC_NONSHARED
asm ("memmove = __GI_memmove");
asm ("memset = __GI_memset");
asm ("memcpy = __GI_memcpy");

/* clang might generate the internal fortfify calls when it is enabled,
   through the buitintin.  */
asm ("__vfprintf_chk = __GI___vfprintf_chk");
asm ("__vsprintf_chk = __GI___vsprintf_chk");
asm ("__vsyslog_chk = __GI___vsyslog_chk");
asm ("__memcpy_chk = __GI___memcpy_chk");
asm ("__memmove_chk = __GI___memmove_chk");
asm ("__memset_chk = __GI___memset_chk");
asm ("__mempcpy_chk = __GI___mempcpy_chk");
asm ("__stpcpy_chk = __GI___stpcpy_chk");
asm ("__strcpy_chk = __GI___strcpy_chk");
asm ("strcpy = __GI_strcpy");
asm ("strncpy = __GI_strncpy");
asm ("strcat = __GI_strcat");
asm ("strlen = __GI_strlen");

/* Some targets do not use __stack_chk_fail_local.  In libc.so,
   redirect __stack_chk_fail to a hidden reference
   __stack_chk_fail_local, to avoid the PLT reference.
   __stack_chk_fail itself is a global symbol, exported from libc.so,
   and cannot be made hidden.  */

# if IS_IN (libc) && defined SHARED \
  && defined STACK_PROTECTOR_LEVEL && STACK_PROTECTOR_LEVEL > 0
asm (".hidden __stack_chk_fail_local\n"
     "__stack_chk_fail = __stack_chk_fail_local");
# endif
#endif
