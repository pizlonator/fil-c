/*
 * Assert definitions for tests.
 *
 * Copyright (c) 2020 Dmitry V. Levin <ldv@altlinux.org>
 */

#ifndef TEST_ASSERT_H
# define TEST_ASSERT_H

# include <config.h>

# include <limits.h>
# include <stdio.h>
# include <stdlib.h>

# define ASSERT_(expected_, expected_str_, op_, seen_, seen_str_)		\
	do {									\
		__typeof__(expected_) e_ = (expected_);				\
		__typeof__(seen_) s_ = (seen_);					\
		if (e_ op_ s_) break;						\
		fprintf(stderr,							\
			"%s:%d: Assertion failed: %s (%#lx) %s %s (%#lx)\n",	\
			__FILE__, __LINE__,					\
			(expected_str_), (unsigned long) e_, #op_,		\
			(seen_str_), (unsigned long) s_);			\
		abort();							\
	} while (0)								\
/* End of ASSERT_ definition.  */

# define ASSERT_EQ(expected_, seen_)						\
	ASSERT_((expected_), #expected_, ==, (seen_), #seen_)			\
/* End of ASSERT_EQ definition.  */

# define ASSERT_NE(expected_, seen_)						\
	ASSERT_((expected_), #expected_, !=, (seen_), #seen_)			\
/* End of ASSERT_NE definition.  */

# define ASSERT_LT(expected_, seen_)						\
	ASSERT_((expected_), #expected_, <, (seen_), #seen_)			\
/* End of ASSERT_LT definition.  */

# define ASSERT_LE(expected_, seen_)						\
	ASSERT_((expected_), #expected_, <=, (seen_), #seen_)			\
/* End of ASSERT_LT definition.  */

# define ASSERT_GT(expected_, seen_)						\
	ASSERT_((expected_), #expected_, >, (seen_), #seen_)			\
/* End of ASSERT_LT definition.  */

# define ASSERT_GE(expected_, seen_)						\
	ASSERT_((expected_), #expected_, >=, (seen_), #seen_)			\
/* End of ASSERT_LT definition.  */

# ifndef PATH_MAX
#  define PATH_MAX 4096
# endif

#endif /* TEST_ASSERT_H */
