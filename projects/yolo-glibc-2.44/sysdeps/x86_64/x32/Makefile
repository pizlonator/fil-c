ifeq ($(subdir),elf)
# Xfail tst-platform-1 on x32 since kernel passes i686 in AT_PLATFORM.
# See https://sourceware.org/bugzilla/show_bug.cgi?id=22363
test-xfail-tst-platform-1 = yes
endif

ifeq ($(subdir),math)
# Since x32 returns 32-bit long int and 64-bit long long int in the
# same 64-bit register, we make the 32b-bit lround an alias of the
# 64-bit llround.  Add -fno-builtin-lround to silence the compiler.
CFLAGS-s_llround.c += -fno-builtin-lround -fno-builtin-lroundf32x \
		      -fno-builtin-lroundf64
endif

ifeq ($(subdir),string)
tests += tst-size_t-memchr tst-size_t-memcmp tst-size_t-memcpy \
	 tst-size_t-memrchr tst-size_t-memset tst-size_t-strncasecmp \
	 tst-size_t-strncmp tst-size_t-strncpy tst-size_t-strnlen \
	 tst-size_t-memcmp-2 tst-size_t-strncat
endif

ifeq ($(subdir),wcsmbs)
tests += tst-size_t-wmemchr tst-size_t-wmemcmp tst-size_t-wmemset \
	 tst-size_t-wcsncmp tst-size_t-wcsnlen
endif
