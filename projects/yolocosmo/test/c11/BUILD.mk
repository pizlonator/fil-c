#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set noet ft=make ts=8 sw=8 fenc=utf-8 :vi ────────────────────┘

PKGS += TEST_C11

TEST_C11_SRCS :=						\
	$(wildcard test/c11/*.c)

TEST_C11_SRCS_TEST =						\
	$(filter %_test.c,$(TEST_C11_SRCS))

TEST_C11_OBJS =							\
	$(TEST_C11_SRCS:%.c=o/$(MODE)/%.o)

TEST_C11_COMS =							\
	$(TEST_C11_SRCS_TEST:%.c=o/$(MODE)/%)			\

TEST_C11_BINS =							\
	$(TEST_C11_COMS)					\
	$(TEST_C11_COMS:%=%.dbg)

TEST_C11_TESTS =						\
	$(TEST_C11_SRCS_TEST:%.c=o/$(MODE)/%.ok)

TEST_C11_CHECKS =						\
	$(TEST_C11_SRCS_TEST:%.c=o/$(MODE)/%.runs)

TEST_C11_DIRECTDEPS =						\
	LIBC_CALLS						\
	LIBC_INTRIN						\
	LIBC_MEM						\
	LIBC_NEXGEN32E						\
	LIBC_RUNTIME						\
	LIBC_THREAD						\
	LIBC_TINYMATH						\
	THIRD_PARTY_COMPILER_RT					\

TEST_C11_DEPS :=						\
	$(call uniq,$(foreach x,$(TEST_C11_DIRECTDEPS),$($(x))))

o/$(MODE)/test/c11/c11.pkg:					\
		$(TEST_C11_OBJS)				\
		$(foreach x,$(TEST_C11_DIRECTDEPS),$($(x)_A).pkg)

o/$(MODE)/test/c11/%.dbg:					\
		$(TEST_C11_DEPS)				\
		o/$(MODE)/test/c11/%.o				\
		o/$(MODE)/test/c11/c11.pkg			\
		$(CRT)						\
		$(APE_NO_MODIFY_SELF)
	@$(APELINK)

o/$(MODE)/test/c11/%.o: CFLAGS += -fno-builtin

.PHONY: o/$(MODE)/test/c11
o/$(MODE)/test/c11:						\
		$(TEST_C11_BINS)				\
		$(TEST_C11_CHECKS)
