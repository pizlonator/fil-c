/*
 * Copyright © 2024 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "config.h"

#include "util-mem.h"
#include "util-range.h"

#include "litest.h"

#define LITEST_RUNNER_DEFAULT_TIMEOUT 30

/**
 * Result returned from tests or suites.
 */
enum litest_runner_result {
	LITEST_PASS = 75,           /**< test successful */
	LITEST_FAIL = 76,           /**< test failed. Should not be returned directly,
					 Use the litest_ macros instead */
	LITEST_SKIP = 77,           /**< test was skipped */
	LITEST_NOT_APPLICABLE = 78, /**< test does not apply */
	LITEST_TIMEOUT = 79,        /**< test aborted after timeout */
	LITEST_SYSTEM_ERROR = 80,   /**< unrelated error occurred */
};

/* For parametrized tests (litest_add_parametrized and friends)
 * a list of these is passed to every test. This struct isn't used
 * directly, use litest_test_param_fetch() instead.
 */
struct litest_test_param {
	struct list link;
	char name[128];
	struct multivalue value;
};

struct litest_test_parameters {
	int refcnt;
	struct list test_params;
};

struct litest_test_parameters *
litest_test_parameters_new(void);

struct litest_test_parameters *
litest_test_parameters_unref(struct litest_test_parameters *params);

#define litest_test_param_fetch(...) \
	_litest_test_param_fetch(__VA_ARGS__, NULL)

void
_litest_test_param_fetch(const struct litest_test_parameters *params, ...);

static inline const char *
litest_test_param_get_string(const struct litest_test_parameters *params,
			     const char *name)
{
	const char *p;
	litest_test_param_fetch(params, name, 's', &p);
	return p;
}

static inline bool
litest_test_param_get_bool(const struct litest_test_parameters *params,
			   const char *name)
{
	bool p;
	litest_test_param_fetch(params, name, 'b', &p);
	return p;
}

static inline int32_t
litest_test_param_get_i32(const struct litest_test_parameters *params, const char *name)
{
	int32_t p;
	litest_test_param_fetch(params, name, 'i', &p);
	return p;
}

static inline uint32_t
litest_test_param_get_u32(const struct litest_test_parameters *params, const char *name)
{
	uint32_t p;
	litest_test_param_fetch(params, name, 'u', &p);
	return p;
}

static inline char
litest_test_param_get_char(const struct litest_test_parameters *params,
			   const char *name)
{
	char p;
	litest_test_param_fetch(params, name, 'c', &p);
	return p;
}

static inline double
litest_test_param_get_double(const struct litest_test_parameters *params,
			     const char *name)
{
	double p;
	litest_test_param_fetch(params, name, 'd', &p);
	return p;
}

/**
 * This struct is passed into every test.
 */
struct litest_runner_test_env {
	int rangeval; /* The current value within the args.range (or 0) */
	const struct litest_test_parameters *params;
};

struct litest_runner_test_description {
	char name[512]; /* The name of the test */
	int rangeval;   /* The current value within the args.range (or 0) */

	struct litest_test_parameters *params;

	/* test function and corresponding setup/teardown, if any */
	enum litest_runner_result (*func)(const struct litest_runner_test_env *);
	void (*setup)(const struct litest_runner_test_description *);
	void (*teardown)(const struct litest_runner_test_description *);

	struct {
		struct range range; /* The range this test applies to */
		int signal;         /* expected signal for fail tests */
	} args;
};

struct litest_runner;

struct litest_runner *
litest_runner_new(void);

/**
 * Default is nprocs * 2.
 * Setting this to 0 means *no* forking. Setting this to 1 means only one test
 * is run at a time but in a child process.
 */
void
litest_runner_set_num_parallel(struct litest_runner *runner, size_t num_jobs);
void
litest_runner_set_timeout(struct litest_runner *runner, unsigned int timeout);
void
litest_runner_set_verbose(struct litest_runner *runner, bool verbose);
void
litest_runner_set_use_colors(struct litest_runner *runner, bool use_colors);
void
litest_runner_set_exit_on_fail(struct litest_runner *runner, bool do_exit);
void
litest_runner_set_output_file(struct litest_runner *runner, FILE *fp);
void
litest_runner_add_test(struct litest_runner *runner,
		       const struct litest_runner_test_description *t);
enum litest_runner_result
litest_runner_run_tests(struct litest_runner *runner);

typedef enum litest_runner_result (*litest_runner_global_setup_func_t)(void *userdata);
typedef void (*litest_runner_global_teardown_func_t)(void *userdata);

void
litest_runner_set_setup_funcs(struct litest_runner *runner,
			      litest_runner_global_setup_func_t setup,
			      litest_runner_global_teardown_func_t teardown,
			      void *userdata);

void
litest_runner_destroy(struct litest_runner *runner);

DEFINE_DESTROY_CLEANUP_FUNC(litest_runner);

/*
 * Function to call abort(). Depending on the number of forks permitted,
 * this function may simply abort() or it may longjmp back out to collect
 * errors from non-forking tests.
 */
__attribute__((noreturn)) void
litest_runner_abort(void);
