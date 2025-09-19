/*
 * Copyright © 2013 Red Hat, Inc.
 * Copyright © 2013 Marcin Slusarz <marcin.slusarz@gmail.com>
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

#include "config.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <getopt.h>
#include <libudev.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "linux/input.h"
#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-bus.h>
#endif
#ifdef __FreeBSD__
#include <termios.h>
#endif

#include <libevdev/libevdev.h>
#include <linux/kd.h>
#include <valgrind/valgrind.h>

#include "util-backtrace.h"
#include "util-files.h"
#include "util-libinput.h"
#include "util-mem.h"

#include "builddir.h"
#include "libinput-util.h"
#include "litest-int.h"
#include "litest-runner.h"
#include "litest.h"
#include "quirks.h"

#define evbit(t, c) ((t) << 16U | (c & 0xffff))

#define UDEV_RULES_D "/run/udev/rules.d"
#define UDEV_FUZZ_OVERRIDE_RULE_FILE                                           \
  UDEV_RULES_D                                                                 \
  "/91-litest-fuzz-override-REMOVEME-XXXXXX.rules"
#define UDEV_TEST_DEVICE_RULE_FILE                                             \
  UDEV_RULES_D                                                                 \
  "/91-litest-test-device-REMOVEME-XXXXXXX.rules"
#define UDEV_DEVICE_GROUPS_FILE                                                \
  UDEV_RULES_D                                                                 \
  "/80-libinput-device-groups-litest-XXXXXX.rules"

bool verbose = false;
bool in_debugger = false;
bool run_deviceless = false;
static bool use_system_rules_quirks = false;
static bool exit_first = false;
static FILE *outfile = NULL;
static const char *filter_test = NULL;
static const char *filter_device = NULL;
static const char *filter_group = NULL;
static int filter_rangeval = INT_MIN;
bool use_colors = false;

struct param_filter {
	char name[64];
	char glob[64];
};
struct param_filter filter_params[8]; /* name=NULL terminated */

static struct quirks_context *quirks_context;

struct created_file {
	struct list link;
	char *path;
};

static void
created_file_destroy(struct created_file *f)
{
	list_remove(&f->link);
	free(f->path);
	free(f);
}

static void
created_file_unlink(struct created_file *f)
{
	unlink(f->path);
	rmdir(f->path);
}

struct suite *current_suite = NULL;

static void
litest_init_udev_rules(struct list *created_files_list);
static void
litest_remove_udev_rules(struct list *created_files_list);
static void
litest_print_event(struct libinput_event *event, const char *message);

enum quirks_setup_mode {
	QUIRKS_SETUP_USE_SRCDIR,
	QUIRKS_SETUP_ONLY_DEVICE,
	QUIRKS_SETUP_FULL,
};
static void
litest_setup_quirks(struct list *created_files_list, enum quirks_setup_mode mode);

/* defined for the litest selftest */
#ifndef LITEST_DISABLE_BACKTRACE_LOGGING
#define litest_log(...) fprintf(stderr, __VA_ARGS__)
#define litest_vlog(format_, args_) vfprintf(stderr, format_, args_)
#else
#define litest_log(...)                                                        \
  { /* __VA_ARGS__ */                                                          \
  }
#define litest_vlog(...)                                                       \
  { /* __VA_ARGS__ */                                                          \
  }
#endif

LIBINPUT_ATTRIBUTE_PRINTF(4, 5)
void
_litest_checkpoint(const char *func,
		   int line,
		   const char *color,
		   const char *format,
		   ...)
{
	char buf[1024];
	va_list args;

	va_start(args, format);
	if (verbose) {
		vsnprintf(buf, sizeof(buf), format, args);
		fprintf(stderr,
			"%s%s():%d - %s%s%s\n",
			use_colors ? ANSI_BRIGHT_BLUE : "",
			func,
			line,
			use_colors ? color : "",
			buf,
			use_colors ? ANSI_NORMAL : "");
	}
	va_end(args);
}

void
litest_backtrace(const char *func)
{
#ifndef LITEST_DISABLE_BACKTRACE_LOGGING
	if (RUNNING_ON_VALGRIND) {
		fprintf(stderr, "Using valgrind, omitting backtrace\n");
		return;
	}
	char buf[256];

	snprintf(buf, sizeof(buf), "in %s", func);

	backtrace_print(stderr,
			use_colors,
			"in litest_backtrace",
			"in litest_runner_test_run",
			func ? buf : NULL);
#endif
}

LIBINPUT_ATTRIBUTE_PRINTF(5, 6)
__attribute__((noreturn)) void
litest_fail_condition(const char *file,
		      int line,
		      const char *func,
		      const char *condition,
		      const char *message,
		      ...)
{
	litest_log("FAILED: %s\n", condition);

	if (message) {
		char buf[1024];
		va_list args;
		va_start(args, message);
		vsnprintf(buf, sizeof(buf), message, args);
		va_end(args);
		litest_log("%s\n", buf);
	}

	litest_log("in %s() (%s:%d)\n", func, file ? file : "???", line);
	litest_backtrace(func);
	litest_runner_abort();
}

__attribute__((noreturn)) void
litest_fail_comparison_int(const char *file,
			   int line,
			   const char *func,
			   const char *operator,
			   int a,
			   int b,
			   const char *astr,
			   const char *bstr)
{
	litest_log("FAILED COMPARISON: %s %s %s\n", astr, operator, bstr);
	litest_log("Resolved to: %d %s %d\n", a, operator, b);
	litest_log("in %s() (%s:%d)\n", func, file, line);
	litest_backtrace(func);
	litest_runner_abort();
}

__attribute__((noreturn)) void
litest_fail_comparison_double(const char *file,
			      int line,
			      const char *func,
			      const char *operator,
			      double a,
			      double b,
			      const char *astr,
			      const char *bstr)
{
	litest_log("FAILED COMPARISON: %s %s %s\n", astr, operator, bstr);
	litest_log("Resolved to: %.3f %s %.3f\n", a, operator, b);
	litest_log("in %s() (%s:%d)\n", func, file, line);
	litest_backtrace(func);
	litest_runner_abort();
}

__attribute__((noreturn)) void
litest_fail_comparison_ptr(const char *file,
			   int line,
			   const char *func,
			   const char *comparison)
{
	litest_log("FAILED COMPARISON: %s\n", comparison);
	litest_log("in %s() (%s:%d)\n", func, file, line);
	litest_backtrace(func);
	litest_runner_abort();
}

__attribute__((noreturn)) void
litest_fail_comparison_str(const char *file,
			   int line,
			   const char *func,
			   const char *comparison,
			   const char *operator,
			   const char *astr,
			   const char *bstr)
{
	litest_log("FAILED COMPARISON: %s %s %s\n", astr, operator, bstr);
	litest_log("Resolved to: %s %s %s\n", astr, operator, bstr);
	litest_log("in %s() (%s:%d)\n", func, file, line);
	litest_backtrace(func);
	litest_runner_abort();
}

struct litest_parameter_value {
	size_t refcnt;
	struct list link; /* litest_parameter->values */

	struct multivalue value;
};

struct litest_parameter {
	size_t refcnt;
	struct list link; /* litest_parameters.params */
	char name[128];
	char type; /* One of u, i, d, c, s, b */

	struct list values; /* litest_parameter_value */
};

struct litest_parameters {
	size_t refcnt;
	struct list params; /* struct litest_parameter */
};

static struct litest_parameter_value *
litest_parameter_value_new(void)
{
	struct litest_parameter_value *pv = zalloc(sizeof *pv);

	list_init(&pv->link);
	pv->refcnt = 1;

	return pv;
}

static inline void
litest_parameter_add_string(struct litest_parameter *p, const char *s)
{
	assert(p->type == 's');

	struct litest_parameter_value *pv = litest_parameter_value_new();
	pv->value = multivalue_new_string(s);
	list_append(&p->values, &pv->link);
}

static inline void
litest_parameter_add_char(struct litest_parameter *p, char c)
{
	assert(p->type == 'c');

	struct litest_parameter_value *pv = litest_parameter_value_new();
	pv->value = multivalue_new_char(c);
	list_append(&p->values, &pv->link);
}

static inline void
litest_parameter_add_bool(struct litest_parameter *p, bool b)
{
	assert(p->type == 'b');

	struct litest_parameter_value *pv = litest_parameter_value_new();
	pv->value = multivalue_new_bool(b);
	list_append(&p->values, &pv->link);
}

static inline void
litest_parameter_add_u32(struct litest_parameter *p, uint32_t u)
{
	assert(p->type == 'u');

	struct litest_parameter_value *pv = litest_parameter_value_new();
	pv->value = multivalue_new_u32(u);
	list_append(&p->values, &pv->link);
}

static inline void
litest_parameter_add_i32(struct litest_parameter *p, int32_t i)
{
	assert(p->type == 'i');

	struct litest_parameter_value *pv = litest_parameter_value_new();
	pv->value = multivalue_new_i32(i);
	list_append(&p->values, &pv->link);
}

static void
litest_parameter_add_double(struct litest_parameter *p, double d)
{
	assert(p->type == 'd');

	struct litest_parameter_value *pv = litest_parameter_value_new();
	pv->value = multivalue_new_double(d);
	list_append(&p->values, &pv->link);
}

static inline void
litest_parameter_add_named_i32(struct litest_parameter *p,
			       const struct litest_named_i32 i)
{
	assert(p->type == 'I');

	struct litest_parameter_value *pv = litest_parameter_value_new();
	pv->value = multivalue_new_named_i32(i.value, i.name);
	list_append(&p->values, &pv->link);
}

#if 0
static struct litest_parameter_value *
litest_parameter_value_ref(struct litest_parameter_value *pv) {
	assert(pv);
	assert(pv->refcnt > 0);
	pv->refcnt++;
	return pv;
}
#endif

static struct litest_parameter_value *
litest_parameter_value_unref(struct litest_parameter_value *pv)
{
	if (pv) {
		assert(pv->refcnt > 0);
		if (--pv->refcnt == 0) {
			list_remove(&pv->link);
			free(pv);
		}
	}
	return NULL;
}

static struct litest_parameter *
litest_parameter_new(const char *name, char type)
{
	struct litest_parameter *p = zalloc(sizeof *p);

	switch (type) {
	case 'b':
	case 'c':
	case 'd':
	case 'i':
	case 'I':
	case 's':
	case 'u':
		break;
	default:
		assert(!"Type not yet implemented");
	}

	list_init(&p->link);
	list_init(&p->values);
	snprintf(p->name, sizeof(p->name), "%s", name);
	p->type = type;
	p->refcnt = 1;

	return p;
}

static struct litest_parameter *
litest_parameter_ref(struct litest_parameter *p)
{
	assert(p);
	assert(p->refcnt > 0);
	p->refcnt++;
	return p;
}

static struct litest_parameter *
litest_parameter_unref(struct litest_parameter *p)
{
	if (p) {
		assert(p->refcnt > 0);
		if (--p->refcnt == 0) {
			struct litest_parameter_value *pv;
			list_for_each_safe(pv, &p->values, link) {
				litest_parameter_value_unref(pv);
			}
			list_remove(&p->link);
			free(p);
		}
	}
	return NULL;
}

DEFINE_UNREF_CLEANUP_FUNC(litest_parameter);

static void
litest_parameters_add(struct litest_parameters *ps, struct litest_parameter *param)
{
	struct litest_parameter *p;
	list_for_each(p, &ps->params, link) {
		assert(!streq(p->name, param->name));
	}

	litest_parameter_ref(param);
	list_append(&ps->params, &param->link);
}

struct litest_parameters *
_litest_parameters_new(const char *name, ...)
{
	struct litest_parameters *ps = zalloc(sizeof *ps);

	list_init(&ps->params);
	ps->refcnt = 1;

	va_list args;
	va_start(args, name);

	while (name) {
		char type = va_arg(args, int);

		_unref_(litest_parameter) *param = litest_parameter_new(name, type);
		if (type == 'b') {
			litest_parameter_add_bool(param, true);
			litest_parameter_add_bool(param, false);
		} else {
			unsigned int nargs = va_arg(args, unsigned int);
			for (unsigned int _ = 0; _ < nargs; _++) {
				switch (type) {
				case 'c': {
					char b = va_arg(args, int);
					litest_parameter_add_char(param, b);
					break;
				}
				case 'u': {
					uint32_t b = va_arg(args, uint32_t);
					litest_parameter_add_u32(param, b);
					break;
				}
				case 'i': {
					int32_t b = va_arg(args, int32_t);
					litest_parameter_add_i32(param, b);
					break;
				}
				case 'd': {
					double b = va_arg(args, double);
					litest_parameter_add_double(param, b);
					break;
				}
				case 's': {
					const char *s = va_arg(args, const char *);
					litest_parameter_add_string(param, s);
					break;
				}
				case 'I': {
					struct litest_named_i32 p =
						va_arg(args, struct litest_named_i32);
					litest_parameter_add_named_i32(param, p);
					break;
				}
				default:
					litest_abort_msg(
						"Unhandled parameter type '%c'",
						type);
					break;
				}
			}
		}

		litest_parameters_add(ps, param);
		name = va_arg(args, const char *);
	}

	va_end(args);

	return ps;
}

struct litest_parameters *
litest_parameters_ref(struct litest_parameters *p)
{
	assert(p);
	assert(p->refcnt > 0);
	p->refcnt++;
	return p;
}

struct litest_parameters *
litest_parameters_unref(struct litest_parameters *params)
{
	if (params) {
		assert(params->refcnt > 0);
		if (--params->refcnt == 0) {
			struct litest_parameter *p;
			list_for_each_safe(p, &params->params, link) {
				litest_parameter_unref(p);
			}
			free(params);
		}
	}
	return NULL;
}

static inline int
_permutate(struct litest_parameters_permutation *permutation,
	   struct list *next_param,
	   void *list_head,
	   litest_parameters_permutation_func_t func,
	   void *userdata)
{
	if (next_param->next == list_head) {
		func(permutation, userdata);
		return 0;
	}
	struct litest_parameter_value *pv;
	struct litest_parameter *param = list_first_entry(next_param, param, link);
	list_for_each(pv, &param->values, link) {
		struct litest_parameters_permutation_value v = {
			.value = pv->value,
		};

		memcpy(v.name, param->name, min(sizeof(v.name), sizeof(param->name)));

		list_append(&permutation->values, &v.link);
		int rc = _permutate(permutation,
				    &param->link,
				    list_head,
				    func,
				    userdata);
		if (rc)
			return rc;
		list_remove(&v.link);
	}

	return 0;
}

/**
 * Calls the given function func with each permutation of
 * the given test parameters.
 */
int
litest_parameters_permutations(struct litest_parameters *params,
			       litest_parameters_permutation_func_t func,
			       void *userdata)
{

	struct litest_parameters_permutation permutation;
	list_init(&permutation.values);

	return _permutate(&permutation,
			  &params->params,
			  &params->params,
			  func,
			  userdata);
}

static struct litest_device *current_device;

struct litest_device *
litest_current_device(void)
{
	return current_device;
}

int
_litest_dispatch(struct libinput *li, const char *func, int line)
{
	static int dispatch_counter = 0;

	++dispatch_counter;

	_litest_checkpoint(func,
			   line,
			   ANSI_MAGENTA,
			   "┌────────────────────  dispatch %3d ────────────────────┐",
			   dispatch_counter);
	int rc = libinput_dispatch(li);
	enum libinput_event_type type = libinput_next_event_type(li);

	const char *evtype =
		type == LIBINPUT_EVENT_NONE ? "NONE" : litest_event_type_str(type);
	_litest_checkpoint(
		func,
		line,
		ANSI_MAGENTA,
		"└──────────────────── /dispatch %3d ────────────────────┘ pending %s",
		dispatch_counter,
		evtype);
	return rc;
}

static void
grab_device(struct litest_device *device, bool mode)
{
	struct libinput *li = libinput_device_get_context(device->libinput_device);
	struct litest_context *ctx = libinput_get_user_data(li);
	_unref_(udev_device) * udev_device;
	const char *devnode;
	struct path *p;

	udev_device = libinput_device_get_udev_device(device->libinput_device);
	litest_assert_ptr_notnull(udev_device);

	devnode = udev_device_get_devnode(udev_device);

	/* Note: in some tests we create multiple devices for the same path.
	 * This will only grab the first device in the list but we're using
	 * list_insert() so the first device is the latest that was
	 * initialized, so we should be good.
	 */
	list_for_each(p, &ctx->paths, link) {
		if (streq(p->path, devnode)) {
			int rc = ioctl(p->fd, EVIOCGRAB, (void *)mode ? 1 : 0);
			litest_assert_errno_success(rc);
			return;
		}
	}
	litest_abort_msg("Failed to find device %s to %sgrab",
			 devnode,
			 mode ? "" : "un");
}

void
litest_grab_device(struct litest_device *device)
{
	grab_device(device, true);
}

void
litest_ungrab_device(struct litest_device *device)
{
	grab_device(device, false);
}

void
litest_set_current_device(struct litest_device *device)
{
	current_device = device;
}

void
litest_generic_device_teardown(void)
{
	litest_device_destroy(current_device);
	current_device = NULL;
}

static struct list devices = LIST_INIT(devices); /* struct litest_test_device */

void
litest_add_test_device(struct list *device)
{
	list_append(&devices, device);
}

static inline void
litest_system(const char *command)
{
	int ret;

	ret = system(command);

	if (ret == -1) {
		litest_abort_msg("Failed to execute: %s", command);
	} else if (WIFEXITED(ret)) {
		if (WEXITSTATUS(ret))
			litest_abort_msg("'%s' failed with %d",
					 command,
					 WEXITSTATUS(ret));
	} else if (WIFSIGNALED(ret)) {
		litest_abort_msg("'%s' terminated with signal %d",
				 command,
				 WTERMSIG(ret));
	}
}

static void
litest_reload_udev_rules(void)
{
	litest_system("udevadm control --reload-rules");
}

static bool
filter_for_rangeval(const struct range *range, int rangeval)
{
	return !range || filter_rangeval == INT_MIN || filter_rangeval == rangeval;
}

static void
litest_add_tcase_for_device(struct suite *suite,
			    const char *funcname,
			    const void *func,
			    const struct litest_test_device *dev,
			    const struct range *range)
{
	const struct range no_range = range_init_empty();

	if (run_deviceless)
		return;

	if (!range)
		range = &no_range;

	int rangeval = range->lower;
	do {
		if (filter_for_rangeval(range, rangeval)) {
			struct test *t;

			t = zalloc(sizeof(*t));
			t->name = safe_strdup(funcname);
			t->devname = safe_strdup(dev->shortname);
			t->func = func;
			t->setup = dev->setup;
			t->teardown = dev->teardown ? dev->teardown
						    : litest_generic_device_teardown;
			if (range)
				t->range = *range;
			t->rangeval = rangeval;

			list_append(&suite->tests, &t->node);
		}
	} while (++rangeval < range->upper);
}

struct permutation_userdata {
	struct suite *suite;
	const char *funcname;
	const void *func;
	const struct litest_test_device *dev;
	char devname[64]; /* set if dev == NULL */

	const struct param_filter *param_filters; /* name=NULL terminated */
};

static int
permutation_func(struct litest_parameters_permutation *permutation, void *userdata)
{
	struct permutation_userdata *data = userdata;

	struct litest_test_parameters *params = litest_test_parameters_new();
	struct litest_parameters_permutation_value *pmv;
	bool filtered = false;
	list_for_each(pmv, &permutation->values, link) {
		const struct param_filter *f = data->param_filters;
		while (!filtered && strlen(f->name)) {
			if (streq(pmv->name, f->name)) {
				_autofree_ char *s = multivalue_as_str(&pmv->value);
				if (fnmatch(f->glob, s, 0) != 0)
					filtered = true;
			}
			f++;
		}

		if (filtered)
			break;

		struct litest_test_param *tp = zalloc(sizeof *tp);
		snprintf(tp->name, sizeof(tp->name), "%s", pmv->name);
		tp->value = multivalue_copy(&pmv->value);
		list_append(&params->test_params, &tp->link);
	}

	if (filtered) {
		litest_test_parameters_unref(params);
		return 0;
	}

	struct test *t;

	t = zalloc(sizeof(*t));
	t->name = safe_strdup(data->funcname);
	t->func = data->func;
	if (data->dev) {
		t->devname = safe_strdup(data->dev->shortname);
		t->setup = data->dev->setup;
		t->teardown = data->dev->teardown ? data->dev->teardown
						  : litest_generic_device_teardown;
	} else {
		t->devname = safe_strdup(data->devname);
		t->setup = NULL;
		t->teardown = NULL;
	}
	t->rangeval = 0;
	t->params = params;

	list_append(&data->suite->tests, &t->node);

	return 0;
}

static void
litest_add_tcase_for_device_with_params(struct suite *suite,
					const char *funcname,
					const void *func,
					const struct litest_test_device *dev,
					struct litest_parameters *params)
{
	if (run_deviceless)
		return;

	struct permutation_userdata data = {
		.suite = suite,
		.funcname = funcname,
		.func = func,
		.dev = dev,
		.param_filters = filter_params,
	};

	litest_parameters_permutations(params, permutation_func, &data);
}

static void
litest_add_tcase_no_device(struct suite *suite,
			   const void *func,
			   const char *funcname,
			   const struct range *range)
{
	const char *test_name = funcname;
	const struct range no_range = range_init_empty();

	if (filter_device && fnmatch(filter_device, test_name, 0) != 0)
		return;

	if (run_deviceless)
		return;

	if (!range)
		range = &no_range;

	int rangeval = range->lower;
	do {
		if (filter_for_rangeval(range, rangeval)) {
			struct test *t;

			t = zalloc(sizeof(*t));
			t->name = safe_strdup(test_name);
			t->devname = safe_strdup("no device");
			t->func = func;
			if (range)
				t->range = *range;
			t->rangeval = rangeval;
			t->setup = NULL;
			t->teardown = NULL;

			list_append(&suite->tests, &t->node);
		}
	} while (++rangeval < range->upper);
}

static void
litest_add_tcase_no_device_with_params(struct suite *suite,
				       const void *func,
				       const char *funcname,
				       struct litest_parameters *params)
{
	const char *test_name = funcname;

	if (filter_device && fnmatch(filter_device, test_name, 0) != 0)
		return;

	if (run_deviceless)
		return;

	struct permutation_userdata data = {
		.suite = suite,
		.funcname = funcname,
		.func = func,
		.param_filters = filter_params,
	};
	snprintf(data.devname, sizeof(data.devname), "no device");

	litest_parameters_permutations(params, permutation_func, &data);
}

static void
litest_add_tcase_deviceless(struct suite *suite,
			    const void *func,
			    const char *funcname,
			    const struct range *range)
{
	const char *test_name = funcname;
	const struct range no_range = range_init_empty();

	if (filter_device && fnmatch(filter_device, test_name, 0) != 0)
		return;

	if (!range)
		range = &no_range;

	int rangeval = range->lower;
	do {
		if (filter_for_rangeval(range, rangeval)) {
			struct test *t;

			t = zalloc(sizeof(*t));
			t->deviceless = true;
			t->name = safe_strdup(test_name);
			t->devname = safe_strdup("deviceless");
			t->func = func;
			if (range)
				t->range = *range;
			t->rangeval = rangeval;
			t->setup = NULL;
			t->teardown = NULL;

			list_append(&suite->tests, &t->node);
		}
	} while (++rangeval < range->upper);
}

static void
litest_add_tcase_deviceless_with_params(struct suite *suite,
					const void *func,
					const char *funcname,
					struct litest_parameters *params)
{
	const char *test_name = funcname;

	if (filter_device && fnmatch(filter_device, test_name, 0) != 0)
		return;

	struct permutation_userdata data = {
		.suite = suite,
		.funcname = funcname,
		.func = func,
		.param_filters = filter_params,
	};
	snprintf(data.devname, sizeof(data.devname), "deviceless");

	litest_parameters_permutations(params, permutation_func, &data);
}

static void
litest_add_tcase(const char *filename,
		 const char *funcname,
		 const void *func,
		 int64_t required,
		 int64_t excluded,
		 const struct range *range,
		 struct litest_parameters *params)
{
	bool added = false;

	litest_assert(required >= LITEST_DEVICELESS);
	litest_assert(excluded >= LITEST_DEVICELESS);

	if (filter_test && fnmatch(filter_test, funcname, 0) != 0)
		return;

	struct suite *suite = current_suite;

	if (filter_group && fnmatch(filter_group, suite->name, 0) != 0)
		return;

	if (required == LITEST_DEVICELESS && excluded == LITEST_DEVICELESS) {
		if (params)
			litest_add_tcase_deviceless_with_params(suite,
								func,
								funcname,
								params);
		else
			litest_add_tcase_deviceless(suite, func, funcname, range);
		added = true;
	} else if (required == LITEST_DISABLE_DEVICE &&
		   excluded == LITEST_DISABLE_DEVICE) {
		if (params)
			litest_add_tcase_no_device_with_params(suite,
							       func,
							       funcname,
							       params);
		else
			litest_add_tcase_no_device(suite, func, funcname, range);
		added = true;
	} else if (required != LITEST_ANY || excluded != LITEST_ANY) {
		struct litest_test_device *dev;

		list_for_each(dev, &devices, node) {
			if (dev->features & LITEST_IGNORED)
				continue;

			if (filter_device &&
			    fnmatch(filter_device, dev->shortname, 0) != 0)
				continue;
			if ((dev->features & required) != required ||
			    (dev->features & excluded) != 0)
				continue;

			if (params) {
				litest_add_tcase_for_device_with_params(suite,
									funcname,
									func,
									dev,
									params);
			} else {
				litest_add_tcase_for_device(suite,
							    funcname,
							    func,
							    dev,
							    range);
			}
			added = true;
		}
	} else {
		struct litest_test_device *dev;

		list_for_each(dev, &devices, node) {
			if (dev->features & LITEST_IGNORED)
				continue;

			if (filter_device &&
			    fnmatch(filter_device, dev->shortname, 0) != 0)
				continue;

			if (params) {
				litest_add_tcase_for_device_with_params(suite,
									funcname,
									func,
									dev,
									params);
			} else {
				litest_add_tcase_for_device(suite,
							    funcname,
							    func,
							    dev,
							    range);
			}
			added = true;
		}
	}

	if (!added && filter_test == NULL && filter_device == NULL &&
	    filter_group == NULL) {
		fprintf(stderr,
			"Test '%s' does not match any devices. Aborting.\n",
			funcname);
		abort();
	}
}

void
_litest_add_no_device(const char *name, const char *funcname, const void *func)
{
	_litest_add(name, funcname, func, LITEST_DISABLE_DEVICE, LITEST_DISABLE_DEVICE);
}

void
_litest_add_parametrized_no_device(const char *name,
				   const char *funcname,
				   const void *func,
				   struct litest_parameters *params)
{
	_litest_add_parametrized(name,
				 funcname,
				 func,
				 LITEST_DISABLE_DEVICE,
				 LITEST_DISABLE_DEVICE,
				 params);
}

void
_litest_add_ranged_no_device(const char *name,
			     const char *funcname,
			     const void *func,
			     const struct range *range)
{
	_litest_add_ranged(name,
			   funcname,
			   func,
			   LITEST_DISABLE_DEVICE,
			   LITEST_DISABLE_DEVICE,
			   range);
}

void
_litest_add_deviceless(const char *name, const char *funcname, const void *func)
{
	_litest_add_ranged(name,
			   funcname,
			   func,
			   LITEST_DEVICELESS,
			   LITEST_DEVICELESS,
			   NULL);
}

void
_litest_add_parametrized_deviceless(const char *name,
				    const char *funcname,
				    const void *func,
				    struct litest_parameters *params)
{
	_litest_add_parametrized(name,
				 funcname,
				 func,
				 LITEST_DISABLE_DEVICE,
				 LITEST_DISABLE_DEVICE,
				 params);
}

void
_litest_add(const char *name,
	    const char *funcname,
	    const void *func,
	    int64_t required,
	    int64_t excluded)
{
	_litest_add_ranged(name, funcname, func, required, excluded, NULL);
}

void
_litest_add_ranged(const char *name,
		   const char *funcname,
		   const void *func,
		   int64_t required,
		   int64_t excluded,
		   const struct range *range)
{
	litest_add_tcase(name, funcname, func, required, excluded, range, NULL);
}

void
_litest_add_parametrized(const char *name,
			 const char *funcname,
			 const void *func,
			 int64_t required,
			 int64_t excluded,
			 struct litest_parameters *params)
{
	litest_add_tcase(name, funcname, func, required, excluded, NULL, params);
}

void
_litest_add_for_device(const char *name,
		       const char *funcname,
		       const void *func,
		       enum litest_device_type type)
{
	_litest_add_ranged_for_device(name, funcname, func, type, NULL);
}

void
_litest_add_ranged_for_device(const char *filename,
			      const char *funcname,
			      const void *func,
			      enum litest_device_type type,
			      const struct range *range)
{
	struct litest_test_device *dev;
	bool device_filtered = false;

	litest_assert(type < LITEST_NO_DEVICE);

	if (filter_test && fnmatch(filter_test, funcname, 0) != 0)
		return;

	struct suite *s = current_suite;

	if (filter_group && fnmatch(filter_group, s->name, 0) != 0)
		return;

	list_for_each(dev, &devices, node) {
		if (filter_device && fnmatch(filter_device, dev->shortname, 0) != 0) {
			device_filtered = true;
			continue;
		}

		if (dev->type == type) {
			litest_add_tcase_for_device(s, funcname, func, dev, range);
			return;
		}
	}

	/* only abort if no filter was set, that's a bug */
	if (!device_filtered)
		litest_abort_msg("Invalid test device type");
}

void
_litest_add_parametrized_for_device(const char *filename,
				    const char *funcname,
				    const void *func,
				    enum litest_device_type type,
				    struct litest_parameters *params)
{
	struct litest_test_device *dev;
	bool device_filtered = false;

	litest_assert(type < LITEST_NO_DEVICE);

	if (filter_test && fnmatch(filter_test, funcname, 0) != 0)
		return;

	struct suite *s = current_suite;

	if (filter_group && fnmatch(filter_group, s->name, 0) != 0)
		return;

	list_for_each(dev, &devices, node) {
		if (filter_device && fnmatch(filter_device, dev->shortname, 0) != 0) {
			device_filtered = true;
			continue;
		}

		if (dev->type == type) {
			litest_add_tcase_for_device_with_params(s,
								funcname,
								func,
								dev,
								params);
			return;
		}
	}

	/* only abort if no filter was set, that's a bug */
	if (!device_filtered)
		litest_abort_msg("Invalid test device type");
}

static int
is_actual_error(const char *error, size_t index, void *data)
{
	/* We don't want to abort on these errors */
	if (((RUNNING_ON_VALGRIND || in_debugger) &&
	     strstr(error, "scheduled expiry is in the past")) ||
	    strstr(error, "event processing lagging behind")) {
		return 0;
	}

	return 1;
}

LIBINPUT_ATTRIBUTE_PRINTF(3, 0)
static void
litest_log_handler(struct libinput *libinput,
		   enum libinput_log_priority pri,
		   const char *format,
		   va_list args)
{
	const char *priority = NULL;
	const char *color = "";
	const char *color_reset = ANSI_NORMAL;

	switch (pri) {
	case LIBINPUT_LOG_PRIORITY_INFO:
		priority = "info ";
		color = ANSI_BOLD;
		break;
	case LIBINPUT_LOG_PRIORITY_ERROR:
		priority = "error";
		color = ANSI_BRIGHT_RED;
		break;
	case LIBINPUT_LOG_PRIORITY_DEBUG:
		priority = "debug";
		color = ANSI_NORMAL;
		break;
	default:
		abort();
	}

	_autofree_ char *msg = strdup_vprintf(format, args);

	if (!use_colors)
		color_reset = "";
	else if (strstr(format, "tap:"))
		color = ANSI_BLUE;
	else if (strstr(format, "thumb state:"))
		color = ANSI_YELLOW;
	else if (strstr(format, "button state:"))
		color = ANSI_MAGENTA;
	else if (strstr(format, "touch-size:") || strstr(format, "pressure:"))
		color = ANSI_GREEN;
	else if (strstr(format, "palm:") || strstr(format, "thumb:"))
		color = ANSI_CYAN;
	else if (strstr(format, "edge-scroll:"))
		color = ANSI_BRIGHT_GREEN;
	else if (strstr(format, "gesture:"))
		color = ANSI_BRIGHT_YELLOW;
	else if (strstr(msg, "Plugin:"))
		color = ANSI_BRIGHT_CYAN;

	fprintf(stderr, "%slitest %s %s%s", color, priority, msg, color_reset);

	if (strstr(msg, "client bug: ") || strstr(msg, "libinput bug: ") ||
	    strstr(msg, "plugin bug: ")) {
		/* valgrind is too slow and some of our offsets are too
		 * short, don't abort if during a valgrind run we get a
		 * negative offset */
		if ((RUNNING_ON_VALGRIND || in_debugger) &&
		    strstr(format, "scheduled expiry is in the past")) {
			/* noop */
		} else if (strstr(format, "event processing lagging behind")) {
			/* noop */
		} else {
			litest_abort_msg("libinput bug triggered, aborting.");
		}
	}

	if (strstr(msg, "Touch jump detected and discarded")) {
		litest_abort_msg("libinput touch jump triggered, aborting.");
	}
}

static void
litest_init_device_udev_rules(struct litest_test_device *dev, FILE *f)
{
	const struct key_value_str *kv;
	static int count;
	bool need_keyboard_builtin = false;

	if (dev->udev_properties[0].key == NULL)
		return;

	count++;

	fprintf(f, "# %s\n", dev->shortname);
	fprintf(f, "ACTION==\"remove\", GOTO=\"rule%d_end\"\n", count);
	fprintf(f, "KERNEL!=\"event*\", GOTO=\"rule%d_end\"\n", count);

	fprintf(f, "ATTRS{name}==\"litest %s*\"", dev->name);

	kv = dev->udev_properties;
	while (kv->key) {
		fprintf(f, ", \\\n\tENV{%s}=\"%s\"", kv->key, kv->value);
		if (strstartswith(kv->key, "EVDEV_ABS_"))
			need_keyboard_builtin = true;
		kv++;
	}
	fprintf(f, "\n");

	/* Special case: the udev keyboard builtin is only run for hwdb
	 * matches but we don't set any up in litest. So instead scan the
	 * device's udev properties for any EVDEV_ABS properties and where
	 * they exist, force a (re-)run of the keyboard builtin to set up
	 * the evdev device correctly.
	 * This needs to be done as separate rule apparently, otherwise the
	 * ENV variables aren't set yet by the time the builtin runs.
	 */
	if (need_keyboard_builtin) {
		fprintf(f,
			""
			"ATTRS{name}==\"litest %s*\","
			" IMPORT{builtin}=\"keyboard\"\n",
			dev->name);
	}

	fprintf(f, "LABEL=\"rule%d_end\"\n\n", count);
	;
}

static void
litest_init_all_device_udev_rules(struct list *created_files)
{
	struct created_file *file = zalloc(sizeof(*file));
	struct litest_test_device *dev;
	FILE *f;
	int fd;

	char *path = strdup_printf("%s/99-litest-XXXXXX.rules", UDEV_RULES_D);
	litest_assert_ptr_notnull(path);

	fd = mkstemps(path, 6);
	litest_assert_errno_success(fd);
	f = fdopen(fd, "w");
	litest_assert_notnull(f);

	list_for_each(dev, &devices, node)
		litest_init_device_udev_rules(dev, f);

	fclose(f);

	file->path = path;
	list_insert(created_files, &file->link);
}

static int
open_restricted(const char *path, int flags, void *userdata)
{
	const char prefix[] = "/dev/input/event";
	struct litest_context *ctx = userdata;
	struct path *p;
	int fd;

	litest_assert_ptr_notnull(ctx);

	fd = open(path, flags);
	if (fd < 0)
		return -errno;

	if (strstartswith(path, prefix)) {
		p = zalloc(sizeof *p);
		p->path = safe_strdup(path);
		p->fd = fd;
		/* We specifically insert here so that the most-recently
		 * opened path is the first one in the list. This helps when
		 * we have multiple test devices with the same device path,
		 * the fd of the most recent device is the first one to get
		 * grabbed
		 */
		list_insert(&ctx->paths, &p->link);
	}

	return fd;
}

static void
close_restricted(int fd, void *userdata)
{
	struct litest_context *ctx = userdata;
	struct path *p;

	list_for_each_safe(p, &ctx->paths, link) {
		if (p->fd != fd)
			continue;
		list_remove(&p->link);
		free(p->path);
		free(p);
	}

	close(fd);
}

static struct libinput_interface interface = {
	.open_restricted = open_restricted,
	.close_restricted = close_restricted,
};

LIBINPUT_ATTRIBUTE_PRINTF(3, 0)
static inline void
quirk_log_handler(struct libinput *unused,
		  enum libinput_log_priority priority,
		  const char *format,
		  va_list args)
{
	if (priority < LIBINPUT_LOG_PRIORITY_ERROR)
		return;

	vfprintf(stderr, format, args);
}

static enum litest_runner_result
init_quirks(void *userdata)
{
	const char *data_path = getenv("LIBINPUT_QUIRKS_DIR");
	if (!data_path)
		data_path = LIBINPUT_QUIRKS_DIR;

	quirks_context = quirks_init_subsystem(data_path,
					       NULL,
					       quirk_log_handler,
					       NULL,
					       QLOG_LIBINPUT_LOGGING);

	return LITEST_PASS;
}

static void
teardown_quirks(void *userdata)
{
	quirks_context_unref(quirks_context);
}

static int
litest_run_suite(struct list *suites, int njobs)
{
	size_t ntests = 0;
	enum litest_runner_result result = LITEST_SKIP;
	struct suite *s;
	_destroy_(litest_runner) *runner = litest_runner_new();

	litest_runner_set_num_parallel(runner, njobs > 0 ? njobs : 0);
	if (outfile)
		litest_runner_set_output_file(runner, outfile);
	litest_runner_set_verbose(runner, verbose);
	litest_runner_set_use_colors(runner, use_colors);
	litest_runner_set_timeout(runner, 30);
	litest_runner_set_exit_on_fail(runner, exit_first);
	litest_runner_set_setup_funcs(runner, init_quirks, teardown_quirks, NULL);

	list_for_each(s, suites, node) {
		struct test *t;
		list_for_each(t, &s->tests, node) {
			struct litest_runner_test_description tdesc = { 0 };

			if (range_is_valid(&t->range)) {
				snprintf(tdesc.name,
					 sizeof(tdesc.name),
					 "%s:%s:%s:%d",
					 s->name,
					 t->name,
					 t->devname,
					 t->rangeval);
			} else if (t->params) {
				char buf[256] = { 0 };

				struct litest_test_param *tp;
				bool is_first = true;
				list_for_each(tp, &t->params->test_params, link) {
					_autofree_ char *val =
						multivalue_as_str(&tp->value);
					snprintf(buf + strlen(buf),
						 sizeof(buf) - strlen(buf),
						 "%s%s:%s",
						 is_first ? "" : ",",
						 tp->name,
						 val);
					is_first = false;
				}
				snprintf(tdesc.name,
					 sizeof(tdesc.name),
					 "%s:%s:%s:%s",
					 s->name,
					 t->name,
					 t->devname,
					 buf);
			} else {
				snprintf(tdesc.name,
					 sizeof(tdesc.name),
					 "%s:%s:%s",
					 s->name,
					 t->name,
					 t->devname);
			}
			tdesc.func = t->func;
			tdesc.setup = t->setup;
			tdesc.teardown = t->teardown;
			tdesc.args.range = t->range;
			tdesc.rangeval = t->rangeval;
			tdesc.params = t->params;
			litest_runner_add_test(runner, &tdesc);
			ntests++;
		}
	}

	if (ntests > 0)
		result = litest_runner_run_tests(runner);

	return result;
}

static inline int
inhibit(void)
{
	int lock_fd = -1;
#ifdef HAVE_LIBSYSTEMD
	_cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
	_unref_(sd_bus_message) *m = NULL;
	_unref_(sd_bus) *bus = NULL;
	int rc;

	if (run_deviceless)
		return -1;

	rc = sd_bus_open_system(&bus);
	if (rc != 0) {
		fprintf(stderr, "Warning: inhibit failed: %s\n", strerror(-rc));
		goto out;
	}

	rc = sd_bus_call_method(
		bus,
		"org.freedesktop.login1",
		"/org/freedesktop/login1",
		"org.freedesktop.login1.Manager",
		"Inhibit",
		&error,
		&m,
		"ssss",
		"sleep:shutdown:handle-lid-switch:handle-power-key:handle-suspend-key:"
		"handle-hibernate-key",
		"libinput test-suite runner",
		"testing in progress",
		"block");
	if (rc < 0) {
		fprintf(stderr, "Warning: inhibit failed: %s\n", error.message);
		goto out;
	}

	rc = sd_bus_message_read(m, "h", &lock_fd);
	if (rc < 0) {
		fprintf(stderr, "Warning: inhibit failed: %s\n", strerror(-rc));
		goto out;
	}

	lock_fd = dup(lock_fd);
out:
	sd_bus_close(bus);
#endif
	return lock_fd;
}

static int
disable_tty(void)
{
	int tty_mode = -1;

	if (isatty(STDIN_FILENO) && ioctl(STDIN_FILENO, KDGKBMODE, &tty_mode) == 0) {
#ifdef __linux__
		ioctl(STDIN_FILENO, KDSKBMODE, K_OFF);
#elif __FreeBSD__
		ioctl(STDIN_FILENO, KDSKBMODE, K_RAW);

		/* Put the tty into raw mode */
		struct termios tios;
		if (tcgetattr(STDIN_FILENO, &tios))
			fprintf(stderr,
				"Failed to get terminal attribute: %d - %s\n",
				errno,
				strerror(errno));
		cfmakeraw(&tios);
		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tios))
			fprintf(stderr,
				"Failed to set terminal attribute: %d - %s\n",
				errno,
				strerror(errno));
#endif
	}

	return tty_mode;
}

static void
restore_tty(int tty_mode)
{
	if (tty_mode != -1) {
		ioctl(STDIN_FILENO, KDSKBMODE, tty_mode);
#ifdef __FreeBSD__
		/* Put the tty into "sane" mode */
		struct termios tios;
		if (tcgetattr(STDIN_FILENO, &tios))
			fprintf(stderr,
				"Failed to get terminal attribute: %d - %s\n",
				errno,
				strerror(errno));
		cfmakesane(&tios);
		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tios))
			fprintf(stderr,
				"Failed to set terminal attribute: %d - %s\n",
				errno,
				strerror(errno));
#endif
	}
}

enum litest_runner_result
litest_run(struct list *suites, int njobs)
{
	const struct rlimit corelimit = { 0, 0 };
	int inhibit_lock_fd;
	int tty_mode = -1;

	setenv("LIBINPUT_RUNNING_TEST_SUITE", "1", 1);

	if (setrlimit(RLIMIT_CORE, &corelimit) != 0)
		perror("WARNING: Core dumps not disabled");

	struct list created_files_list = LIST_INIT(created_files_list);

	if (run_deviceless) {
		litest_setup_quirks(&created_files_list, QUIRKS_SETUP_USE_SRCDIR);
	} else {
		enum quirks_setup_mode mode;
		litest_init_udev_rules(&created_files_list);

		mode = use_system_rules_quirks ? QUIRKS_SETUP_ONLY_DEVICE
					       : QUIRKS_SETUP_FULL;
		litest_setup_quirks(&created_files_list, mode);
	}

	/* If we're running 'normally' on the VT, disable the keyboard to
	 * avoid messing up our host. But if we're inside gdb or running
	 * without forking, leave it as-is.
	 */
	if (!run_deviceless && njobs > 1 && !in_debugger)
		tty_mode = disable_tty();

	inhibit_lock_fd = inhibit();

	enum litest_runner_result result = litest_run_suite(suites, njobs);

	close(inhibit_lock_fd);

	restore_tty(tty_mode);

	litest_remove_udev_rules(&created_files_list);

	return result;
}

static struct input_absinfo *
merge_absinfo(const struct input_absinfo *orig, const struct input_absinfo *override)
{
	struct input_absinfo *abs;
	unsigned int nelem, i;
	size_t sz = ABS_MAX + 1;

	if (!orig)
		return NULL;

	abs = zalloc(sz * sizeof(*abs));
	litest_assert_ptr_notnull(abs);

	nelem = 0;
	while (orig[nelem].value != -1) {
		abs[nelem] = orig[nelem];
		nelem++;
		litest_assert_int_lt(nelem, sz);
	}

	/* just append, if the same axis is present twice, libevdev will
	   only use the last value anyway */
	i = 0;
	while (override && override[i].value != -1) {
		abs[nelem++] = override[i++];
		litest_assert_int_lt(nelem, sz);
	}

	litest_assert_int_lt(nelem, sz);
	abs[nelem].value = -1;

	return abs;
}

static int *
merge_events(const int *orig, const int *override)
{
	int *events;
	unsigned int nelem, i;
	size_t sz = KEY_MAX * 3;

	if (!orig)
		return NULL;

	events = zalloc(sz * sizeof(int));
	litest_assert_ptr_notnull(events);

	nelem = 0;
	while (orig[nelem] != -1) {
		events[nelem] = orig[nelem];
		nelem++;
		litest_assert_int_lt(nelem, sz);
	}

	/* just append, if the same axis is present twice, libevdev will
	 * ignore the double definition anyway */
	i = 0;
	while (override && override[i] != -1) {
		events[nelem++] = override[i++];
		litest_assert_int_le(nelem, sz);
	}

	litest_assert_int_lt(nelem, sz);
	events[nelem] = -1;

	return events;
}

static inline struct created_file *
litest_copy_file(const char *dest, const char *src, const char *header, bool is_file)
{
	int in, out, length;
	struct created_file *file;

	file = zalloc(sizeof(*file));
	file->path = safe_strdup(dest);

	if (strstr(dest, "XXXXXX")) {
		int suffixlen;

		suffixlen = file->path + strlen(file->path) - rindex(file->path, '.');
		out = mkstemps(file->path, suffixlen);
	} else {
		out = open(file->path, O_CREAT | O_WRONLY, 0644);
	}
	if (out == -1)
		litest_abort_msg("Failed to write to file %s (%s)",
				 file->path,
				 strerror(errno));
	litest_assert_errno_success(chmod(file->path, 0644));

	if (header) {
		length = strlen(header);
		litest_assert_int_eq(write(out, header, length), length);
	}

	if (is_file) {
		in = open(src, O_RDONLY);
		if (in == -1)
			litest_abort_msg("Failed to open file %s (%s)",
					 src,
					 strerror(errno));
		/* lazy, just check for error and empty file copy */
		litest_assert_int_gt(litest_send_file(out, in), 0);
		close(in);
	} else {
		size_t written = write(out, src, strlen(src));
		litest_assert_int_eq(written, strlen(src));
	}
	close(out);

	return file;
}

static inline void
litest_install_model_quirks(struct list *created_files_list)
{
	const char *warning =
		"#################################################################\n"
		"# WARNING: REMOVE THIS FILE\n"
		"# This is a run-time file for the libinput test suite and\n"
		"# should be removed on exit. If the test-suite is not currently \n"
		"# running, remove this file\n"
		"#################################################################\n\n";
	struct created_file *file;
	const char *test_device_udev_rule =
		"KERNELS==\"*input*\", "
		"ATTRS{name}==\"litest *\", "
		"ENV{LIBINPUT_TEST_DEVICE}=\"1\"";

	file = litest_copy_file(UDEV_TEST_DEVICE_RULE_FILE,
				test_device_udev_rule,
				warning,
				false);
	list_insert(created_files_list, &file->link);

	/* Only install the litest device rule when we're running as system
	 * test suite, we expect the others to be in place already */
	if (use_system_rules_quirks)
		return;

	file = litest_copy_file(UDEV_DEVICE_GROUPS_FILE,
				LIBINPUT_DEVICE_GROUPS_RULES_FILE,
				warning,
				true);
	list_insert(created_files_list, &file->link);

	file = litest_copy_file(UDEV_FUZZ_OVERRIDE_RULE_FILE,
				LIBINPUT_FUZZ_OVERRIDE_UDEV_RULES_FILE,
				warning,
				true);
	list_insert(created_files_list, &file->link);
}

static char *
litest_init_device_quirk_file(const char *data_dir, struct litest_test_device *dev)
{
	int fd;
	FILE *f;
	char path[PATH_MAX];
	static int count;

	if (!dev->quirk_file)
		return NULL;

	snprintf(path,
		 sizeof(path),
		 "%s/99-%03d-%s.quirks",
		 data_dir,
		 ++count,
		 dev->shortname);
	fd = open(path, O_CREAT | O_WRONLY, 0644);
	litest_assert_errno_success(fd);
	f = fdopen(fd, "w");
	litest_assert_notnull(f);
	litest_assert_int_ge(fputs(dev->quirk_file, f), 0);
	fclose(f);

	return safe_strdup(path);
}

static int
is_quirks_file(const struct dirent *dir)
{
	return strendswith(dir->d_name, ".quirks");
}

/**
 * Install the quirks from the quirks/ source directory.
 */
static void
litest_install_source_quirks(struct list *created_files_list, const char *dirname)
{
	_autofree_ struct dirent **namelist;
	int ndev;

	ndev = scandir(LIBINPUT_QUIRKS_SRCDIR, &namelist, is_quirks_file, versionsort);
	litest_assert_int_ge(ndev, 0);

	for (int idx = 0; idx < ndev; idx++) {
		struct created_file *file;
		char *filename;
		char dest[PATH_MAX];
		char src[PATH_MAX];

		_autofree_ struct dirent *entry = namelist[idx];
		filename = entry->d_name;
		snprintf(src, sizeof(src), "%s/%s", LIBINPUT_QUIRKS_SRCDIR, filename);
		snprintf(dest, sizeof(dest), "%s/%s", dirname, filename);
		file = litest_copy_file(dest, src, NULL, true);
		list_append(created_files_list, &file->link);
	}
}

/**
 * Install the quirks from the various litest test devices
 */
static void
litest_install_device_quirks(struct list *created_files_list, const char *dirname)
{
	struct litest_test_device *dev;

	list_for_each(dev, &devices, node) {
		char *path;

		path = litest_init_device_quirk_file(dirname, dev);
		if (path) {
			struct created_file *file = zalloc(sizeof(*file));
			file->path = path;
			list_insert(created_files_list, &file->link);
		}
	}
}

static void
litest_setup_quirks(struct list *created_files_list, enum quirks_setup_mode mode)
{
	struct created_file *file = NULL;
	const char *dirname;
	char tmpdir[] = "/run/litest-XXXXXX";

	switch (mode) {
	case QUIRKS_SETUP_USE_SRCDIR:
		dirname = LIBINPUT_QUIRKS_SRCDIR;
		break;
	case QUIRKS_SETUP_ONLY_DEVICE:
		dirname = LIBINPUT_QUIRKS_DIR;
		litest_install_device_quirks(created_files_list, dirname);
		break;
	case QUIRKS_SETUP_FULL:
		litest_assert_notnull(mkdtemp(tmpdir));
		litest_assert_errno_success(chmod(tmpdir, 0755));
		file = zalloc(sizeof *file);
		file->path = safe_strdup(tmpdir);
		dirname = tmpdir;

		litest_install_source_quirks(created_files_list, dirname);
		litest_install_device_quirks(created_files_list, dirname);
		list_append(created_files_list, &file->link);
		break;
	}

	setenv("LIBINPUT_QUIRKS_DIR", dirname, 1);
}

static inline void
litest_init_udev_rules(struct list *created_files)
{
	litest_assert_neg_errno_success(mkdir_p(UDEV_RULES_D));

	litest_install_model_quirks(created_files);
	litest_init_all_device_udev_rules(created_files);
	litest_reload_udev_rules();
}

static inline void
litest_remove_udev_rules(struct list *created_files_list)
{
	struct created_file *f;
	bool reload_udev;

	reload_udev = !list_empty(created_files_list);

	list_for_each_safe(f, created_files_list, link) {
		created_file_unlink(f);
		created_file_destroy(f);
	}

	if (reload_udev)
		litest_reload_udev_rules();
}

/**
 * Creates a uinput device but does not add it to a libinput context
 */
struct litest_device *
litest_create(enum litest_device_type which,
	      const char *name_override,
	      struct input_id *id_override,
	      const struct input_absinfo *abs_override,
	      const int *events_override)
{
	struct litest_device *d = NULL;
	struct litest_test_device *dev;
	const char *name;
	const struct input_id *id;
	_autofree_ struct input_absinfo *abs;
	_autofree_ int *events;
	int *e;
	const char *path;
	int fd, rc;
	bool found = false;
	bool create_device = true;

	list_for_each(dev, &devices, node) {
		if (dev->type == which) {
			found = true;
			break;
		}
	}

	if (!found)
		litest_abort_msg("Invalid device type %d", which);

	d = zalloc(sizeof(*d));
	d->which = which;

	/* device has custom create method */
	if (dev->create) {
		create_device = dev->create(d);
		if (abs_override || events_override) {
			litest_abort_msg("Custom create cannot be overridden");
		}
	}

	abs = merge_absinfo(dev->absinfo, abs_override);
	events = merge_events(dev->events, events_override);
	name = name_override ? name_override : dev->name;
	id = id_override ? id_override : dev->id;

	if (create_device) {
		d->uinput = litest_create_uinput_device_from_description(name,
									 id,
									 abs,
									 events);
		d->interface = dev->interface;

		for (e = events; *e != -1; e += 2) {
			unsigned int type = *e, code = *(e + 1);

			if (type == INPUT_PROP_MAX && code == INPUT_PROP_SEMI_MT) {
				d->semi_mt.is_semi_mt = true;
				break;
			}
		}
	}

	path = libevdev_uinput_get_devnode(d->uinput);
	litest_assert_ptr_notnull(path);
	fd = open(path, O_RDWR | O_NONBLOCK);
	litest_assert_errno_success(fd);

	rc = libevdev_new_from_fd(fd, &d->evdev);
	litest_assert_neg_errno_success(rc);

	return d;
}

struct libinput *
litest_create_context(void)
{
	struct libinput *libinput;
	struct litest_context *ctx;

	ctx = zalloc(sizeof *ctx);
	list_init(&ctx->paths);

	libinput = libinput_path_create_context(&interface, ctx);
	litest_assert_notnull(libinput);

	libinput_log_set_handler(libinput, litest_log_handler);
	if (verbose)
		libinput_log_set_priority(libinput, LIBINPUT_LOG_PRIORITY_DEBUG);

	return libinput;
}

void
litest_destroy_context(struct libinput *li)
{
	if (li) {
		_autofree_ struct litest_context *ctx = libinput_get_user_data(li);
		litest_assert_ptr_notnull(ctx);
		libinput_unref(li);

		struct path *p;
		list_for_each_safe(p, &ctx->paths, link) {
			litest_abort_msg("Device paths should be removed by now");
		}
	}
}

void
litest_context_set_user_data(struct libinput *li, void *data)
{
	struct litest_user_data *litest_data = libinput_get_user_data(li);
	litest_assert_ptr_notnull(litest_data);
	litest_data->private = data;
}

void *
litest_context_get_user_data(struct libinput *li)
{
	struct litest_user_data *litest_data = libinput_get_user_data(li);
	litest_assert_ptr_notnull(litest_data);
	return litest_data->private;
}

void
litest_disable_log_handler(struct libinput *libinput)
{
	libinput_log_set_handler(libinput, NULL);
}

void
litest_restore_log_handler(struct libinput *libinput)
{
	libinput_log_set_handler(libinput, litest_log_handler);
	if (verbose)
		libinput_log_set_priority(libinput, LIBINPUT_LOG_PRIORITY_DEBUG);
}

LIBINPUT_ATTRIBUTE_PRINTF(3, 0)
static void
litest_bug_log_handler(struct libinput *libinput,
		       enum libinput_log_priority pri,
		       const char *format,
		       va_list args)
{
	if (pri != LIBINPUT_LOG_PRIORITY_ERROR || strstr(format, "client bug: ") ||
	    strstr(format, "libinput bug: ") || strstr(format, "kernel bug: "))
		return;

	/* messages from plugins don't have the string in the format, it's one of the
	 * args... */
	_autofree_ char *msg = strdup_vprintf(format, args);
	if (strstr(msg, "plugin bug:"))
		return;

	litest_abort_msg("Expected bug statement in log msg ('%s'), aborting.", msg);
}

void
litest_set_log_handler_bug(struct libinput *libinput)
{
	libinput_log_set_handler(libinput, litest_bug_log_handler);
}

struct litest_device *
litest_add_device_with_overrides(struct libinput *libinput,
				 enum litest_device_type which,
				 const char *name_override,
				 struct input_id *id_override,
				 const struct input_absinfo *abs_override,
				 const int *events_override)
{
	struct litest_device *d;
	const char *path;

	d = litest_create(which,
			  name_override,
			  id_override,
			  abs_override,
			  events_override);

	path = libevdev_uinput_get_devnode(d->uinput);
	litest_assert_ptr_notnull(path);

	d->libinput = libinput;
	d->libinput_device = libinput_path_add_device(d->libinput, path);
	litest_assert_ptr_notnull(d->libinput_device);
	_unref_(udev_device) *ud = libinput_device_get_udev_device(d->libinput_device);
	d->quirks = quirks_fetch_for_device(quirks_context, ud);

	libinput_device_ref(d->libinput_device);

	if (d->interface) {
		unsigned int code;

		code = ABS_X;
		if (!libevdev_has_event_code(d->evdev, EV_ABS, code))
			code = ABS_MT_POSITION_X;
		if (libevdev_has_event_code(d->evdev, EV_ABS, code)) {
			d->interface->min[ABS_X] =
				libevdev_get_abs_minimum(d->evdev, code);
			d->interface->max[ABS_X] =
				libevdev_get_abs_maximum(d->evdev, code);
		}

		code = ABS_Y;
		if (!libevdev_has_event_code(d->evdev, EV_ABS, code))
			code = ABS_MT_POSITION_Y;
		if (libevdev_has_event_code(d->evdev, EV_ABS, code)) {
			d->interface->min[ABS_Y] =
				libevdev_get_abs_minimum(d->evdev, code);
			d->interface->max[ABS_Y] =
				libevdev_get_abs_maximum(d->evdev, code);
		}
		d->interface->tool_type = BTN_TOOL_PEN;
	}
	return d;
}

struct litest_device *
litest_add_device(struct libinput *libinput, enum litest_device_type which)
{
	return litest_add_device_with_overrides(libinput,
						which,
						NULL,
						NULL,
						NULL,
						NULL);
}

struct litest_device *
litest_create_device_with_overrides(enum litest_device_type which,
				    const char *name_override,
				    struct input_id *id_override,
				    const struct input_absinfo *abs_override,
				    const int *events_override)
{
	struct litest_device *dev =
		litest_add_device_with_overrides(litest_create_context(),
						 which,
						 name_override,
						 id_override,
						 abs_override,
						 events_override);
	dev->owns_context = true;
	return dev;
}

struct litest_device *
litest_create_device(enum litest_device_type which)
{
	return litest_create_device_with_overrides(which, NULL, NULL, NULL, NULL);
}

static struct udev_monitor *
udev_setup_monitor(void)
{
	_unref_(udev) *udev = udev_new();
	_unref_(udev_monitor) *udev_monitor = NULL;
	int rc;

	litest_assert_notnull(udev);
	udev_monitor = udev_monitor_new_from_netlink(udev, "udev");
	litest_assert_notnull(udev_monitor);
	udev_monitor_filter_add_match_subsystem_devtype(udev_monitor, "input", NULL);

	/* remove O_NONBLOCK */
	rc = fcntl(udev_monitor_get_fd(udev_monitor), F_SETFL, 0);
	litest_assert_errno_success(rc);
	litest_assert_int_eq(udev_monitor_enable_receiving(udev_monitor), 0);

	return steal(&udev_monitor);
}

static struct udev_device *
udev_wait_for_device_event(struct udev_monitor *udev_monitor,
			   const char *udev_event,
			   const char *syspath)
{
	/* blocking, we don't want to continue until udev is ready */
	while (1) {
		_unref_(udev_device) *udev_device = NULL;
		const char *udev_syspath = NULL;
		const char *udev_action;

		udev_device = udev_monitor_receive_device(udev_monitor);
		if (!udev_device) {
			if (errno == EAGAIN)
				continue;

			litest_abort_msg(
				"Failed to receive udev device from monitor: %s (%d)",
				strerror(errno),
				errno);
		}
		udev_action = udev_device_get_action(udev_device);
		if (!udev_action || !streq(udev_action, udev_event)) {
			continue;
		}

		udev_syspath = udev_device_get_syspath(udev_device);
		if (strstartswith(udev_syspath, syspath))
			return steal(&udev_device);
	}
}

void
litest_device_destroy(struct litest_device *d)
{

	_unref_(udev_monitor) *udev_monitor = NULL;
	_unref_(udev_device) *udev_device = NULL;
	char path[PATH_MAX];

	if (!d)
		return;

	udev_monitor = udev_setup_monitor();
	snprintf(path,
		 sizeof(path),
		 "%s/event",
		 libevdev_uinput_get_syspath(d->uinput));

	litest_assert_int_eq(d->skip_ev_syn, 0);

	quirks_unref(d->quirks);

	if (d->libinput_device) {
		libinput_path_remove_device(d->libinput_device);
		libinput_device_unref(d->libinput_device);
	}
	if (d->owns_context) {
		libinput_dispatch(d->libinput);
		litest_destroy_context(d->libinput);
	}
	close(libevdev_get_fd(d->evdev));
	libevdev_free(d->evdev);
	libevdev_uinput_destroy(d->uinput);
	free(d->private);
	memset(d, 0, sizeof(*d));
	free(d);

	udev_device =
		udev_wait_for_device_event(udev_monitor, // NOLINT: deadcode.DeadStores
					   "remove",
					   path);
}

void
litest_event_unchecked(struct litest_device *d,
		       unsigned int type,
		       unsigned int code,
		       int value)
{
	int ret = libevdev_uinput_write_event(d->uinput, type, code, value);
	litest_assert_neg_errno_success(ret);
}

void
litest_event(struct litest_device *d, unsigned int type, unsigned int code, int value)
{
	if (!libevdev_has_event_code(d->evdev, type, code))
		return;

	if (type == EV_SYN && code == SYN_REPORT) {
		if (d->skip_ev_syn)
			return;

		for (size_t i = 0; i < d->frame.nevents; i++) {
			struct input_event *e = &d->frame.events[i];
			int ret = libevdev_uinput_write_event(d->uinput,
							      e->type,
							      e->code,
							      e->value);
			litest_assert_neg_errno_success(ret);
		}

		int ret = libevdev_uinput_write_event(d->uinput,
						      EV_SYN,
						      SYN_REPORT,
						      value);
		litest_assert_neg_errno_success(ret);

		d->frame.nevents = 0;
	} else {
		size_t i;

		if (type == EV_SYN || (type == EV_ABS && code >= ABS_MT_SLOT)) {
			i = d->frame.nevents;
		} else {
			for (i = 0; i < d->frame.nevents; i++) {
				if (d->frame.events[i].type == type &&
				    d->frame.events[i].code == code)
					break;
			}
		}
		litest_assert_int_lt(i, ARRAY_LENGTH(d->frame.events));
		d->frame.events[i] = (struct input_event){
			.type = type,
			.code = code,
			.value = value,
		};
		if (i >= d->frame.nevents)
			d->frame.nevents++;
	}
}

static bool
axis_replacement_value(struct litest_device *d,
		       struct axis_replacement *axes,
		       int32_t evcode,
		       int32_t *value)
{
	struct axis_replacement *axis = axes;

	if (!axes)
		return false;

	while (axis->evcode != -1) {
		if (axis->evcode == evcode) {
			switch (evcode) {
			case ABS_MT_SLOT:
			case ABS_MT_TRACKING_ID:
			case ABS_MT_TOOL_TYPE:
				*value = axis->value;
				break;
			default:
				*value = litest_scale(d, evcode, axis->value);
				break;
			}
			return true;
		}
		axis++;
	}

	return false;
}

int
litest_auto_assign_value(struct litest_device *d,
			 const struct input_event *ev,
			 int slot,
			 double x,
			 double y,
			 struct axis_replacement *axes,
			 bool touching)
{
	static int tracking_id;
	int value = ev->value;

	if (value != LITEST_AUTO_ASSIGN || ev->type != EV_ABS)
		return value;

	switch (ev->code) {
	case ABS_X:
	case ABS_MT_POSITION_X:
		value = litest_scale(d, ABS_X, x);
		break;
	case ABS_Y:
	case ABS_MT_POSITION_Y:
		value = litest_scale(d, ABS_Y, y);
		break;
	case ABS_MT_TRACKING_ID:
		value = ++tracking_id;
		break;
	case ABS_MT_SLOT:
		value = slot;
		break;
	case ABS_MT_DISTANCE:
		value = touching ? 0 : 1;
		break;
	case ABS_MT_TOOL_TYPE:
		if (!axis_replacement_value(d, axes, ev->code, &value))
			value = MT_TOOL_FINGER;
		break;
	default:
		if (!axis_replacement_value(d, axes, ev->code, &value) &&
		    d->interface->get_axis_default) {
			int error = d->interface->get_axis_default(d, ev->code, &value);
			if (error) {
				litest_abort_msg(
					"Failed to get default axis value for %s (%d)",
					libevdev_event_code_get_name(EV_ABS, ev->code),
					ev->code);
			}
		}
		break;
	}

	return value;
}

static void
send_btntool(struct litest_device *d, bool hover)
{
	litest_event(d, EV_KEY, BTN_TOUCH, d->ntouches_down != 0 && !hover);
	litest_event(d, EV_KEY, BTN_TOOL_FINGER, d->ntouches_down == 1);
	litest_event(d, EV_KEY, BTN_TOOL_DOUBLETAP, d->ntouches_down == 2);
	litest_event(d, EV_KEY, BTN_TOOL_TRIPLETAP, d->ntouches_down == 3);
	litest_event(d, EV_KEY, BTN_TOOL_QUADTAP, d->ntouches_down == 4);
	litest_event(d, EV_KEY, BTN_TOOL_QUINTTAP, d->ntouches_down == 5);
}

static void
slot_start(struct litest_device *d,
	   unsigned int slot,
	   double x,
	   double y,
	   struct axis_replacement *axes,
	   bool touching,
	   bool filter_abs_xy)
{
	struct input_event *ev;

	litest_assert_int_ge(d->ntouches_down, 0);
	d->ntouches_down++;

	send_btntool(d, !touching);

	/* If the test device overrides touch_down and says it didn't
	 * handle the event, let's continue normally */
	if (d->interface->touch_down && d->interface->touch_down(d, slot, x, y))
		return;

	for (ev = d->interface->touch_down_events;
	     ev && (int16_t)ev->type != -1 && (int16_t)ev->code != -1;
	     ev++) {
		int value = litest_auto_assign_value(d, ev, slot, x, y, axes, touching);
		if (value == LITEST_AUTO_ASSIGN)
			continue;

		if (filter_abs_xy && ev->type == EV_ABS &&
		    (ev->code == ABS_X || ev->code == ABS_Y))
			continue;

		litest_event(d, ev->type, ev->code, value);
	}
}

static void
slot_move(struct litest_device *d,
	  unsigned int slot,
	  double x,
	  double y,
	  struct axis_replacement *axes,
	  bool touching,
	  bool filter_abs_xy)
{
	struct input_event *ev;

	if (d->interface->touch_move && d->interface->touch_move(d, slot, x, y))
		return;

	for (ev = d->interface->touch_move_events;
	     ev && (int16_t)ev->type != -1 && (int16_t)ev->code != -1;
	     ev++) {
		int value = litest_auto_assign_value(d, ev, slot, x, y, axes, touching);
		if (value == LITEST_AUTO_ASSIGN)
			continue;

		if (filter_abs_xy && ev->type == EV_ABS &&
		    (ev->code == ABS_X || ev->code == ABS_Y))
			continue;

		litest_event(d, ev->type, ev->code, value);
	}
}

static void
touch_up(struct litest_device *d, unsigned int slot)
{
	struct input_event *ev;
	struct input_event up[] = {
		{ .type = EV_ABS, .code = ABS_MT_SLOT, .value = LITEST_AUTO_ASSIGN },
		{ .type = EV_ABS, .code = ABS_MT_TRACKING_ID, .value = -1 },
		{ .type = EV_ABS, .code = ABS_MT_PRESSURE, .value = 0 },
		{ .type = EV_ABS, .code = ABS_MT_TOUCH_MAJOR, .value = 0 },
		{ .type = EV_ABS, .code = ABS_MT_TOUCH_MINOR, .value = 0 },
		{ .type = EV_SYN, .code = SYN_REPORT, .value = 0 },
		{ .type = -1, .code = -1 }
	};

	litest_assert_int_gt(d->ntouches_down, 0);
	d->ntouches_down--;

	send_btntool(d, false);

	if (d->interface->touch_up && d->interface->touch_up(d, slot)) {
		return;
	} else if (d->interface->touch_up_events) {
		ev = d->interface->touch_up_events;
	} else
		ev = up;

	for (/* */; ev && (int16_t)ev->type != -1 && (int16_t)ev->code != -1; ev++) {
		int value = litest_auto_assign_value(d, ev, slot, 0, 0, NULL, false);
		litest_event(d, ev->type, ev->code, value);
	}
}

static void
litest_slot_start(struct litest_device *d,
		  unsigned int slot,
		  double x,
		  double y,
		  struct axis_replacement *axes,
		  bool touching)
{
	double t, l, r = 0, b = 0; /* top, left, right, bottom */
	bool filter_abs_xy = false;

	if (!d->semi_mt.is_semi_mt) {
		slot_start(d, slot, x, y, axes, touching, filter_abs_xy);
		return;
	}

	if (d->ntouches_down >= 2 || slot > 1)
		return;

	slot = d->ntouches_down;

	if (d->ntouches_down == 0) {
		l = x;
		t = y;
	} else {
		int other = (slot + 1) % 2;
		l = min(x, d->semi_mt.touches[other].x);
		t = min(y, d->semi_mt.touches[other].y);
		r = max(x, d->semi_mt.touches[other].x);
		b = max(y, d->semi_mt.touches[other].y);
	}

	litest_with_event_frame(d) {
		if (d->ntouches_down == 0)
			slot_start(d, 0, l, t, axes, touching, filter_abs_xy);
		else
			slot_move(d, 0, l, t, axes, touching, filter_abs_xy);

		if (slot == 1) {
			filter_abs_xy = true;
			slot_start(d, 1, r, b, axes, touching, filter_abs_xy);
		}
	}

	d->semi_mt.touches[slot].x = x;
	d->semi_mt.touches[slot].y = y;
}

void
litest_touch_sequence(struct litest_device *d,
		      unsigned int slot,
		      double x_from,
		      double y_from,
		      double x_to,
		      double y_to,
		      int steps)
{
	litest_touch_down(d, slot, x_from, y_from);
	litest_touch_move_to(d, slot, x_from, y_from, x_to, y_to, steps);
	litest_touch_up(d, slot);
}

void
litest_touch_down(struct litest_device *d, unsigned int slot, double x, double y)
{
	litest_slot_start(d, slot, x, y, NULL, true);
}

void
litest_touch_down_extended(struct litest_device *d,
			   unsigned int slot,
			   double x,
			   double y,
			   struct axis_replacement *axes)
{
	litest_slot_start(d, slot, x, y, axes, true);
}

static void
litest_slot_move(struct litest_device *d,
		 unsigned int slot,
		 double x,
		 double y,
		 struct axis_replacement *axes,
		 bool touching)
{
	double t, l, r = 0, b = 0; /* top, left, right, bottom */
	bool filter_abs_xy = false;

	if (!d->semi_mt.is_semi_mt) {
		slot_move(d, slot, x, y, axes, touching, filter_abs_xy);
		return;
	}

	if (d->ntouches_down > 2 || slot > 1)
		return;

	if (d->ntouches_down == 1) {
		l = x;
		t = y;
	} else {
		int other = (slot + 1) % 2;
		l = min(x, d->semi_mt.touches[other].x);
		t = min(y, d->semi_mt.touches[other].y);
		r = max(x, d->semi_mt.touches[other].x);
		b = max(y, d->semi_mt.touches[other].y);
	}

	litest_with_event_frame(d) {
		slot_move(d, 0, l, t, axes, touching, filter_abs_xy);

		if (d->ntouches_down == 2) {
			filter_abs_xy = true;
			slot_move(d, 1, r, b, axes, touching, filter_abs_xy);
		}
	}

	d->semi_mt.touches[slot].x = x;
	d->semi_mt.touches[slot].y = y;
}

void
litest_touch_up(struct litest_device *d, unsigned int slot)
{
	if (!d->semi_mt.is_semi_mt) {
		touch_up(d, slot);
		return;
	}

	if (d->ntouches_down > 2 || slot > 1)
		return;

	litest_with_event_frame(d) {
		touch_up(d, d->ntouches_down - 1);

		/* if we have one finger left, send x/y coords for that finger left.
		this is likely to happen with a real touchpad */
		if (d->ntouches_down == 1) {
			bool touching = true;
			bool filter_abs_xy = false;

			int other = (slot + 1) % 2;
			slot_move(d,
				  0,
				  d->semi_mt.touches[other].x,
				  d->semi_mt.touches[other].y,
				  NULL,
				  touching,
				  filter_abs_xy);
		}
	}
}

void
litest_touch_move(struct litest_device *d, unsigned int slot, double x, double y)
{
	litest_slot_move(d, slot, x, y, NULL, true);
}

void
litest_touch_move_extended(struct litest_device *d,
			   unsigned int slot,
			   double x,
			   double y,
			   struct axis_replacement *axes)
{
	litest_slot_move(d, slot, x, y, axes, true);
}

void
litest_touch_move_to(struct litest_device *d,
		     unsigned int slot,
		     double x_from,
		     double y_from,
		     double x_to,
		     double y_to,
		     int steps)
{
	litest_touch_move_to_extended(d, slot, x_from, y_from, x_to, y_to, NULL, steps);
}

void
litest_touch_move_to_extended(struct litest_device *d,
			      unsigned int slot,
			      double x_from,
			      double y_from,
			      double x_to,
			      double y_to,
			      struct axis_replacement *axes,
			      int steps)
{
	int sleep_ms = 10;

	for (int i = 1; i < steps; i++) {
		litest_touch_move_extended(d,
					   slot,
					   x_from + (x_to - x_from) / steps * i,
					   y_from + (y_to - y_from) / steps * i,
					   axes);
		libinput_dispatch(d->libinput);
		msleep(sleep_ms);
		libinput_dispatch(d->libinput);
	}
	litest_touch_move_extended(d, slot, x_to, y_to, axes);
}

static int
auto_assign_tablet_value(struct litest_device *d,
			 const struct input_event *ev,
			 int x,
			 int y,
			 struct axis_replacement *axes)
{
	static int tracking_id;
	int value = ev->value;

	if (value != LITEST_AUTO_ASSIGN || ev->type != EV_ABS)
		return value;

	switch (ev->code) {
	case ABS_MT_TRACKING_ID:
		value = ++tracking_id;
		break;
	case ABS_X:
	case ABS_MT_POSITION_X:
		value = litest_scale(d, ABS_X, x);
		break;
	case ABS_Y:
	case ABS_MT_POSITION_Y:
		value = litest_scale(d, ABS_Y, y);
		break;
	default:
		if (!axis_replacement_value(d, axes, ev->code, &value) &&
		    d->interface->get_axis_default) {
			int error = d->interface->get_axis_default(d, ev->code, &value);
			if (error) {
				litest_abort_msg(
					"Failed to get default axis value for %s (%d)",
					libevdev_event_code_get_name(EV_ABS, ev->code),
					ev->code);
			}
		}
		break;
	}

	return value;
}

static int
tablet_ignore_event(const struct input_event *ev, int value)
{
	return value == -1 && (ev->code == ABS_PRESSURE || ev->code == ABS_DISTANCE);
}

void
litest_tablet_set_tool_type(struct litest_device *d, unsigned int code)
{
	switch (code) {
	case BTN_TOOL_PEN:
	case BTN_TOOL_RUBBER:
	case BTN_TOOL_BRUSH:
	case BTN_TOOL_PENCIL:
	case BTN_TOOL_AIRBRUSH:
	case BTN_TOOL_MOUSE:
	case BTN_TOOL_LENS:
		break;
	default:
		abort();
	}

	d->interface->tool_type = code;
}

static void
litest_tool_event(struct litest_device *d, int value)
{
	unsigned int tool = d->interface->tool_type;

	litest_event(d, EV_KEY, tool, value);
}

void
litest_tablet_proximity_in(struct litest_device *d,
			   double x,
			   double y,
			   struct axis_replacement *axes)
{
	struct input_event *ev;

	/* If the test device overrides proximity_in and says it didn't
	 * handle the event, let's continue normally */
	if (d->interface->tablet_proximity_in &&
	    d->interface->tablet_proximity_in(d, d->interface->tool_type, &x, &y, axes))
		return;

	ev = d->interface->tablet_proximity_in_events;
	while (ev && (int16_t)ev->type != -1 && (int16_t)ev->code != -1) {
		int value;

		switch (evbit(ev->type, ev->code)) {
		case evbit(EV_KEY, LITEST_BTN_TOOL_AUTO):
			litest_tool_event(d, ev->value);
			break;
		default:
			value = auto_assign_tablet_value(d, ev, x, y, axes);
			if (!tablet_ignore_event(ev, value))
				litest_event(d, ev->type, ev->code, value);
		}
		ev++;
	}
}

void
litest_tablet_proximity_out(struct litest_device *d)
{
	struct input_event *ev;

	/* If the test device overrides proximity_out and says it didn't
	 * handle the event, let's continue normally */
	if (d->interface->tablet_proximity_out &&
	    d->interface->tablet_proximity_out(d, d->interface->tool_type))
		return;

	ev = d->interface->tablet_proximity_out_events;
	while (ev && (int16_t)ev->type != -1 && (int16_t)ev->code != -1) {
		int value;

		switch (evbit(ev->type, ev->code)) {
		case evbit(EV_KEY, LITEST_BTN_TOOL_AUTO):
			litest_tool_event(d, ev->value);
			break;
		default:
			value = auto_assign_tablet_value(d, ev, -1, -1, NULL);
			if (!tablet_ignore_event(ev, value))
				litest_event(d, ev->type, ev->code, value);
			break;
		}
		ev++;
	}
}

void
litest_tablet_motion(struct litest_device *d,
		     double x,
		     double y,
		     struct axis_replacement *axes)
{
	struct input_event *ev;

	/* If the test device overrides proximity_out and says it didn't
	 * handle the event, let's continue normally */
	if (d->interface->tablet_motion && d->interface->tablet_motion(d, &x, &y, axes))
		return;

	ev = d->interface->tablet_motion_events;
	while (ev && (int16_t)ev->type != -1 && (int16_t)ev->code != -1) {
		int value = auto_assign_tablet_value(d, ev, x, y, axes);
		if (!tablet_ignore_event(ev, value))
			litest_event(d, ev->type, ev->code, value);
		ev++;
	}
}

void
litest_tablet_tip_down(struct litest_device *d,
		       double x,
		       double y,
		       struct axis_replacement *axes)
{
	/* If the test device overrides tip_down and says it didn't
	 * handle the event, let's continue normally */
	if (d->interface->tablet_tip_down &&
	    d->interface->tablet_tip_down(d, &x, &y, axes))
		return;

	litest_event(d, EV_KEY, BTN_TOUCH, 1);
	litest_tablet_motion(d, x, y, axes);
}

void
litest_tablet_tip_up(struct litest_device *d,
		     double x,
		     double y,
		     struct axis_replacement *axes)
{
	/* If the test device overrides tip_down and says it didn't
	 * handle the event, let's continue normally */
	if (d->interface->tablet_tip_up && d->interface->tablet_tip_up(d, &x, &y, axes))
		return;

	litest_event(d, EV_KEY, BTN_TOUCH, 0);
	litest_tablet_motion(d, x, y, axes);
}

void
litest_touch_move_two_touches(struct litest_device *d,
			      double x0,
			      double y0,
			      double x1,
			      double y1,
			      double dx,
			      double dy,
			      int steps)
{
	int sleep_ms = 10;

	for (int i = 1; i < steps; i++) {
		litest_with_event_frame(d) {
			litest_touch_move(d,
					  0,
					  x0 + dx / steps * i,
					  y0 + dy / steps * i);
			litest_touch_move(d,
					  1,
					  x1 + dx / steps * i,
					  y1 + dy / steps * i);
		}
		libinput_dispatch(d->libinput);
		msleep(sleep_ms);
		libinput_dispatch(d->libinput);
	}
	litest_with_event_frame(d) {
		litest_touch_move(d, 0, x0 + dx, y0 + dy);
		litest_touch_move(d, 1, x1 + dx, y1 + dy);
	}
}

void
litest_touch_move_three_touches(struct litest_device *d,
				double x0,
				double y0,
				double x1,
				double y1,
				double x2,
				double y2,
				double dx,
				double dy,
				int steps)
{
	int sleep_ms = 10;

	for (int i = 1; i <= steps; i++) {
		double step_x = dx / steps * i;
		double step_y = dy / steps * i;

		litest_with_event_frame(d) {
			litest_touch_move(d, 0, x0 + step_x, y0 + step_y);
			litest_touch_move(d, 1, x1 + step_x, y1 + step_y);
			litest_touch_move(d, 2, x2 + step_x, y2 + step_y);
		}

		libinput_dispatch(d->libinput);
		msleep(sleep_ms);
	}
	libinput_dispatch(d->libinput);
}

void
litest_hover_start(struct litest_device *d, unsigned int slot, double x, double y)
{
	struct axis_replacement axes[] = {
		{ ABS_MT_PRESSURE, 0 },
		{ ABS_PRESSURE, 0 },
		{ -1, -1 },
	};

	litest_slot_start(d, slot, x, y, axes, 0);
}

void
litest_hover_end(struct litest_device *d, unsigned int slot)
{
	struct input_event *ev;
	struct input_event up[] = {
		{ .type = EV_ABS, .code = ABS_MT_SLOT, .value = LITEST_AUTO_ASSIGN },
		{ .type = EV_ABS, .code = ABS_MT_DISTANCE, .value = 1 },
		{ .type = EV_ABS, .code = ABS_MT_TRACKING_ID, .value = -1 },
		{ .type = EV_SYN, .code = SYN_REPORT, .value = 0 },
		{ .type = -1, .code = -1 }
	};

	litest_assert_int_gt(d->ntouches_down, 0);
	d->ntouches_down--;

	send_btntool(d, true);

	if (d->interface->touch_up) {
		d->interface->touch_up(d, slot);
		return;
	} else if (d->interface->touch_up_events) {
		ev = d->interface->touch_up_events;
	} else
		ev = up;

	while (ev && (int16_t)ev->type != -1 && (int16_t)ev->code != -1) {
		int value = litest_auto_assign_value(d, ev, slot, 0, 0, NULL, false);
		litest_event(d, ev->type, ev->code, value);
		ev++;
	}
}

void
litest_hover_move(struct litest_device *d, unsigned int slot, double x, double y)
{
	struct axis_replacement axes[] = {
		{ ABS_MT_PRESSURE, 0 },
		{ ABS_PRESSURE, 0 },
		{ -1, -1 },
	};

	litest_slot_move(d, slot, x, y, axes, false);
}

void
litest_hover_move_to(struct litest_device *d,
		     unsigned int slot,
		     double x_from,
		     double y_from,
		     double x_to,
		     double y_to,
		     int steps)
{
	int sleep_ms = 10;

	for (int i = 0; i < steps - 1; i++) {
		litest_hover_move(d,
				  slot,
				  x_from + (x_to - x_from) / steps * i,
				  y_from + (y_to - y_from) / steps * i);
		libinput_dispatch(d->libinput);
		msleep(sleep_ms);
		libinput_dispatch(d->libinput);
	}
	litest_hover_move(d, slot, x_to, y_to);
}

void
litest_hover_move_two_touches(struct litest_device *d,
			      double x0,
			      double y0,
			      double x1,
			      double y1,
			      double dx,
			      double dy,
			      int steps)
{
	int sleep_ms = 10;

	for (int i = 0; i < steps - 1; i++) {
		litest_with_event_frame(d) {
			litest_hover_move(d,
					  0,
					  x0 + dx / steps * i,
					  y0 + dy / steps * i);
			litest_hover_move(d,
					  1,
					  x1 + dx / steps * i,
					  y1 + dy / steps * i);
		}
		libinput_dispatch(d->libinput);
		msleep(sleep_ms);
		libinput_dispatch(d->libinput);
	}
	litest_with_event_frame(d) {
		litest_hover_move(d, 0, x0 + dx, y0 + dy);
		litest_hover_move(d, 1, x1 + dx, y1 + dy);
	}
}

void
litest_button_click(struct litest_device *d, unsigned int button, bool is_press)
{
	struct input_event click[] = {
		{ .type = EV_KEY, .code = button, .value = is_press ? 1 : 0 },
		{ .type = EV_SYN, .code = SYN_REPORT, .value = 0 },
	};

	ARRAY_FOR_EACH(click, ev)
		litest_event(d, ev->type, ev->code, ev->value);
}

void
litest_button_click_debounced(struct litest_device *d,
			      struct libinput *li,
			      unsigned int button,
			      bool is_press)
{
	litest_button_click(d, button, is_press);
	litest_timeout_debounce(li);
}

void
litest_button_scroll(struct litest_device *dev,
		     unsigned int button,
		     double dx,
		     double dy)
{
	struct libinput *li = dev->libinput;

	litest_button_click_debounced(dev, li, button, 1);

	litest_timeout_buttonscroll(li);

	litest_event(dev, EV_REL, REL_X, dx);
	litest_event(dev, EV_REL, REL_Y, dy);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	litest_button_click_debounced(dev, li, button, 0);

	libinput_dispatch(li);
}

void
litest_button_scroll_locked(struct litest_device *dev,
			    unsigned int button,
			    double dx,
			    double dy)
{
	struct libinput *li = dev->libinput;

	litest_button_click_debounced(dev, li, button, 1);
	litest_button_click_debounced(dev, li, button, 0);

	litest_timeout_buttonscroll(li);

	litest_event(dev, EV_REL, REL_X, dx);
	litest_event(dev, EV_REL, REL_Y, dy);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	libinput_dispatch(li);
}

void
litest_keyboard_key(struct litest_device *d, unsigned int key, bool is_press)
{
	struct input_event click[] = {
		{ .type = EV_KEY, .code = key, .value = is_press ? 1 : 0 },
		{ .type = EV_SYN, .code = SYN_REPORT, .value = 0 },
	};

	ARRAY_FOR_EACH(click, ev)
		litest_event(d, ev->type, ev->code, ev->value);
}

void
litest_switch_action(struct litest_device *dev,
		     enum libinput_switch sw,
		     enum libinput_switch_state state)
{
	unsigned int code;

	switch (sw) {
	case LIBINPUT_SWITCH_LID:
		code = SW_LID;
		break;
	case LIBINPUT_SWITCH_TABLET_MODE:
		code = SW_TABLET_MODE;
		break;
	default:
		litest_abort_msg("Invalid switch %d", sw);
		break;
	}
	litest_event(dev, EV_SW, code, state);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
}

static int
litest_scale_axis(const struct litest_device *d, unsigned int axis, double val)
{
	const struct input_absinfo *abs;

	litest_assert_double_ge(val, 0.0);
	/* major/minor must be able to beyond 100% for large fingers */
	if (axis != ABS_MT_TOUCH_MAJOR && axis != ABS_MT_TOUCH_MINOR) {
		litest_assert_double_le(val, 100.0);
	}

	abs = libevdev_get_abs_info(d->evdev, axis);
	litest_assert_notnull(abs);

	return (abs->maximum - abs->minimum) * val / 100.0 + abs->minimum;
}

static inline int
litest_scale_range(int min, int max, double val)
{
	litest_assert_int_ge((int)val, 0);
	litest_assert_int_le((int)val, 100);

	return (max - min) * val / 100.0 + min;
}

int
litest_scale(const struct litest_device *d, unsigned int axis, double val)
{
	int min, max;

	litest_assert_double_ge(val, 0.0);
	/* major/minor must be able to beyond 100% for large fingers */
	if (axis != ABS_MT_TOUCH_MAJOR && axis != ABS_MT_TOUCH_MINOR)
		litest_assert_double_le(val, 100.0);

	if (axis <= ABS_Y) {
		min = d->interface->min[axis];
		max = d->interface->max[axis];

		return litest_scale_range(min, max, val);
	} else {
		return litest_scale_axis(d, axis, val);
	}
}

static inline int
auto_assign_pad_value(struct litest_device *dev, struct input_event *ev, double value)
{
	const struct input_absinfo *abs;

	if (ev->value != LITEST_AUTO_ASSIGN)
		return value;

	if (ev->type == EV_REL) {
		switch (ev->code) {
		case REL_WHEEL:
		case REL_HWHEEL:
		case REL_DIAL:
			assert(fmod(value, 120.0) ==
			       0.0); /* Fractions not supported yet */
			return value / 120.0;
		default:
			return value;
		}
	} else if (ev->type != EV_ABS) {
		return value;
	}

	abs = libevdev_get_abs_info(dev->evdev, ev->code);
	litest_assert_notnull(abs);

	if (ev->code == ABS_RX || ev->code == ABS_RY) {
		double min = abs->minimum != 0 ? log2(abs->minimum) : 0,
		       max = abs->maximum != 0 ? log2(abs->maximum) : 0;

		/* Value 0 is reserved for finger up, so a value of 0% is
		 * actually 1 */
		if (value == 0.0) {
			return 1;
		} else {
			value = litest_scale_range(min, max, value);
			return pow(2, value);
		}
	} else {
		return litest_scale_range(abs->minimum, abs->maximum, value);
	}
}

static void
litest_pad_events(struct litest_device *d, struct input_event *evs, double value)
{
	while (evs && (int16_t)evs->type != -1 && (int16_t)evs->code != -1) {
		if (evs->type == EV_SYN && evs->code == SYN_REPORT)
			value = 0;
		value = auto_assign_pad_value(d, evs, value);
		litest_event(d, evs->type, evs->code, value);
		evs++;
	}
}

void
litest_pad_ring_start(struct litest_device *d, double value)
{
	litest_pad_events(d, d->interface->pad_ring_start_events, value);
}

void
litest_pad_ring_change(struct litest_device *d, double value)
{
	litest_pad_events(d, d->interface->pad_ring_change_events, value);
}

void
litest_pad_ring_end(struct litest_device *d)
{
	struct input_event *ev;

	ev = d->interface->pad_ring_end_events;
	while (ev && (int16_t)ev->type != -1 && (int16_t)ev->code != -1) {
		litest_event(d, ev->type, ev->code, ev->value);
		ev++;
	}
}

void
litest_pad_strip_start(struct litest_device *d, double value)
{
	litest_pad_events(d, d->interface->pad_strip_start_events, value);
}

void
litest_pad_strip_change(struct litest_device *d, double value)
{
	litest_pad_events(d, d->interface->pad_strip_change_events, value);
}

void
litest_pad_strip_end(struct litest_device *d)
{
	struct input_event *ev;

	ev = d->interface->pad_strip_end_events;
	while (ev && (int16_t)ev->type != -1 && (int16_t)ev->code != -1) {
		litest_event(d, ev->type, ev->code, ev->value);
		ev++;
	}
}

void
litest_wait_for_event(struct libinput *li)
{
	return litest_wait_for_event_of_type(li, -1);
}

void
_litest_wait_for_event_of_type(struct libinput *li, const char *func, int lineno, ...)
{
	va_list args;
	enum libinput_event_type types[32] = { LIBINPUT_EVENT_NONE };
	size_t ntypes = 0;
	enum libinput_event_type type;
	struct pollfd fds;

	va_start(args, lineno);
	type = va_arg(args, int);
	while ((int)type != -1) {
		litest_assert_int_gt(type, 0U);
		litest_assert_int_lt(ntypes, ARRAY_LENGTH(types));
		types[ntypes++] = type;
		type = va_arg(args, int);
	}
	va_end(args);

	fds.fd = libinput_get_fd(li);
	fds.events = POLLIN;
	fds.revents = 0;

	const int timeout = 2000;
	uint64_t expiry = 0;
	int rc = now_in_us(&expiry);
	expiry += ms2us(timeout);
	litest_assert_errno_success(rc);

	while (1) {
		size_t i;
		enum libinput_event_type type;

		while ((type = libinput_next_event_type(li)) == LIBINPUT_EVENT_NONE) {
			int rc = poll(&fds, 1, timeout);
			litest_assert_errno_success(rc);
			litest_assert_int_gt(rc, 0);
			litest_dispatch(li);
		}

		if (type == LIBINPUT_EVENT_NONE) {
			uint64_t now;
			now_in_us(&now);
			if (now > expiry) {
				_litest_abort_msg(
					NULL,
					lineno,
					func,
					"Waited >%dms for events, but no events are pending",
					timeout);
			}
		}

		/* no event mask means wait for any event */
		if (ntypes == 0)
			return;

		for (i = 0; i < ntypes; i++) {
			if (type == types[i])
				return;
		}

		_destroy_(libinput_event) *event = libinput_get_event(li);
		if (verbose) {
			litest_print_event(event, "Discarding event while waiting: ");
		}
	}
}

void
litest_drain_events(struct libinput *li)
{
	do {
		libinput_dispatch(li);

		_destroy_(libinput_event) *event = libinput_get_event(li);
		if (!event)
			break;

		if (verbose) {
			litest_print_event(event, "litest: draining event: ");
		}
	} while (true);
}

void
_litest_drain_events_of_type(struct libinput *li, ...)
{
	enum libinput_event_type type;
	enum libinput_event_type types[32] = { LIBINPUT_EVENT_NONE };
	size_t ntypes = 0;
	va_list args;

	va_start(args, li);
	type = va_arg(args, int);
	while ((int)type != -1) {
		litest_assert_int_gt(type, 0U);
		litest_assert_int_lt(ntypes, ARRAY_LENGTH(types));
		types[ntypes++] = type;
		type = va_arg(args, int);
	}
	va_end(args);

	libinput_dispatch(li);
	type = libinput_next_event_type(li);
	while (type != LIBINPUT_EVENT_NONE) {
		bool found = false;

		type = libinput_next_event_type(li);

		for (size_t i = 0; i < ntypes; i++) {
			if (type == types[i]) {
				found = true;
				break;
			}
		}
		if (!found)
			return;

		_destroy_(libinput_event) *event = libinput_get_event(li);
		if (verbose)
			litest_print_event(event, "litest: draining typed event: ");
		libinput_dispatch(li);
	}
}

const char *
litest_event_type_str(enum libinput_event_type type)
{
	const char *str = NULL;

	switch (type) {
	case LIBINPUT_EVENT_NONE:
		abort();
	case LIBINPUT_EVENT_DEVICE_ADDED:
		str = "ADDED";
		break;
	case LIBINPUT_EVENT_DEVICE_REMOVED:
		str = "REMOVED";
		break;
	case LIBINPUT_EVENT_KEYBOARD_KEY:
		str = "KEY";
		break;
	case LIBINPUT_EVENT_POINTER_MOTION:
		str = "MOTION";
		break;
	case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
		str = "ABSOLUTE";
		break;
	case LIBINPUT_EVENT_POINTER_BUTTON:
		str = "BUTTON";
		break;
	case LIBINPUT_EVENT_POINTER_AXIS:
		str = "AXIS";
		break;
	case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
		str = "SCROLL_WHEEL";
		break;
	case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
		str = "SCROLL_FINGER";
		break;
	case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
		str = "SCROLL_CONTINUOUS";
		break;
	case LIBINPUT_EVENT_TOUCH_DOWN:
		str = "TOUCH DOWN";
		break;
	case LIBINPUT_EVENT_TOUCH_UP:
		str = "TOUCH UP";
		break;
	case LIBINPUT_EVENT_TOUCH_MOTION:
		str = "TOUCH MOTION";
		break;
	case LIBINPUT_EVENT_TOUCH_CANCEL:
		str = "TOUCH CANCEL";
		break;
	case LIBINPUT_EVENT_TOUCH_FRAME:
		str = "TOUCH FRAME";
		break;
	case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
		str = "GESTURE SWIPE BEGIN";
		break;
	case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
		str = "GESTURE SWIPE UPDATE";
		break;
	case LIBINPUT_EVENT_GESTURE_SWIPE_END:
		str = "GESTURE SWIPE END";
		break;
	case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
		str = "GESTURE PINCH BEGIN";
		break;
	case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
		str = "GESTURE PINCH UPDATE";
		break;
	case LIBINPUT_EVENT_GESTURE_PINCH_END:
		str = "GESTURE PINCH END";
		break;
	case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN:
		str = "GESTURE HOLD BEGIN";
		break;
	case LIBINPUT_EVENT_GESTURE_HOLD_END:
		str = "GESTURE HOLD END";
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_AXIS:
		str = "TABLET TOOL AXIS";
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY:
		str = "TABLET TOOL PROX";
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_TIP:
		str = "TABLET TOOL TIP";
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_BUTTON:
		str = "TABLET TOOL BUTTON";
		break;
	case LIBINPUT_EVENT_TABLET_PAD_BUTTON:
		str = "TABLET PAD BUTTON";
		break;
	case LIBINPUT_EVENT_TABLET_PAD_RING:
		str = "TABLET PAD RING";
		break;
	case LIBINPUT_EVENT_TABLET_PAD_STRIP:
		str = "TABLET PAD STRIP";
		break;
	case LIBINPUT_EVENT_TABLET_PAD_KEY:
		str = "TABLET PAD KEY";
		break;
	case LIBINPUT_EVENT_TABLET_PAD_DIAL:
		str = "TABLET PAD DIAL";
		break;
	case LIBINPUT_EVENT_SWITCH_TOGGLE:
		str = "SWITCH TOGGLE";
		break;
	}
	return str;
}

static const char *
litest_event_get_type_str(struct libinput_event *event)
{
	return litest_event_type_str(libinput_event_get_type(event));
}

static void
litest_print_event(struct libinput_event *event, const char *message)
{
	_autofree_ char *event_str = libinput_event_to_str(event, 0, NULL);
	fprintf(stderr, "litest: %s %s\n", message, event_str);
}

void
_litest_assert_event_type_is_one_of(struct libinput_event *event,
				    const char *func,
				    int lineno,
				    ...)
{
	va_list args;
	enum libinput_event_type expected_type;
	enum libinput_event_type actual_type = libinput_event_get_type(event);
	bool match = false;

	va_start(args, lineno);
	expected_type = va_arg(args, int);
	while ((int)expected_type != -1 && !match) {
		match = (actual_type == expected_type);
		expected_type = va_arg(args, int);
	}
	va_end(args);

	if (match)
		return;

	fprintf(stderr,
		"FAILED EVENT TYPE: %s: have %s (%d) but want ",
		libinput_device_get_name(libinput_event_get_device(event)),
		litest_event_get_type_str(event),
		libinput_event_get_type(event));

	va_start(args, lineno);
	expected_type = va_arg(args, int);
	while ((int)expected_type != -1) {
		fprintf(stderr,
			"%s (%d)",
			litest_event_type_str(expected_type),
			expected_type);
		expected_type = va_arg(args, int);

		if ((int)expected_type != -1)
			fprintf(stderr, " || ");
	}
	va_end(args);
	fprintf(stderr, "\n");

	litest_print_event(event, "Wrong event is:");
	litest_backtrace(func);
	litest_runner_abort();
}

void
_litest_assert_event_type(struct libinput_event *event,
			  enum libinput_event_type want,
			  const char *func,
			  int lineno)
{
	_litest_assert_event_type_is_one_of(event, func, lineno, want, -1);
}

void
_litest_assert_event_type_not_one_of(struct libinput_event *event,
				     const char *func,
				     int lineno,
				     ...)
{
	va_list args;
	enum libinput_event_type not_expected_type;
	enum libinput_event_type actual_type = libinput_event_get_type(event);
	bool match = false;

	va_start(args, lineno);
	not_expected_type = va_arg(args, int);
	while ((int)not_expected_type != -1 && !match) {
		match = (actual_type == not_expected_type);
		not_expected_type = va_arg(args, int);
	}
	va_end(args);

	if (!match)
		return;

	fprintf(stderr,
		"FAILED EVENT TYPE: %s: have %s (%d) but didn't want that\n",
		libinput_device_get_name(libinput_event_get_device(event)),
		litest_event_get_type_str(event),
		libinput_event_get_type(event));

	litest_print_event(event, "\nWrong event is: ");
	litest_backtrace(func);
	litest_runner_abort();
}

void
_litest_assert_empty_queue(struct libinput *li, const char *func, int line)
{
	bool empty_queue = true;
	struct libinput_event *event;

	_litest_checkpoint(func, line, ANSI_BRIGHT_CYAN, "asserting empty queue");

	libinput_dispatch(li);
	while ((event = libinput_get_event(li))) {
		empty_queue = false;
		litest_print_event(event, "Unexpected event: ");
		libinput_event_destroy(event);
		libinput_dispatch(li);
	}

	litest_assert(empty_queue);
}

static struct libevdev_uinput *
litest_create_uinput(const char *name,
		     const struct input_id *id,
		     const struct input_absinfo *abs_info,
		     const int *events)
{
	struct libevdev_uinput *uinput;
	_free_(libevdev) *dev = libevdev_new();
	int type, code;
	int rc;
	const struct input_absinfo *abs;
	const struct input_absinfo default_abs = { .value = 0,
						   .minimum = 0,
						   .maximum = 100,
						   .fuzz = 0,
						   .flat = 0,
						   .resolution = 100 };
	/* See kernel commit 206f533a0a7c ("Input: uinput - reject requests with
	 * unreasonable number of slots") */
	const struct input_absinfo default_abs_mt_slot = { .value = 0,
							   .minimum = 0,
							   .maximum = 64,
							   .fuzz = 0,
							   .flat = 0,
							   .resolution = 100 };
	char buf[512];

	litest_assert_ptr_notnull(dev);

	snprintf(buf, sizeof(buf), "litest %s", name);
	libevdev_set_name(dev, buf);
	if (id) {
		libevdev_set_id_bustype(dev, id->bustype);
		libevdev_set_id_vendor(dev, id->vendor);
		libevdev_set_id_product(dev, id->product);
		libevdev_set_id_version(dev, id->version);
	}

	abs = abs_info;
	while (abs && abs->value != -1) {
		struct input_absinfo a = *abs;

		/* abs_info->value is used for the code and may be outside
		   of [min, max] */
		a.value = abs->minimum;
		rc = libevdev_enable_event_code(dev, EV_ABS, abs->value, &a);
		litest_assert_int_eq(rc, 0);
		abs++;
	}

	while (events && (type = *events++) != -1 && (code = *events++) != -1) {
		if (type == INPUT_PROP_MAX) {
			rc = libevdev_enable_property(dev, code);
		} else {
			const struct input_absinfo *abs = (code == ABS_MT_SLOT)
								  ? &default_abs_mt_slot
								  : &default_abs;
			rc = libevdev_enable_event_code(dev,
							type,
							code,
							type == EV_ABS ? abs : NULL);
		}
		litest_assert_int_eq(rc, 0);
	}

	rc = libevdev_uinput_create_from_device(dev,
						LIBEVDEV_UINPUT_OPEN_MANAGED,
						&uinput);
	litest_assert_msg(rc == 0,
			  "Failed to create uinput device: %s\n",
			  strerror(-rc));

	return uinput;
}

struct libevdev_uinput *
litest_create_uinput_device_from_description(const char *name,
					     const struct input_id *id,
					     const struct input_absinfo *abs_info,
					     const int *events)
{
	struct libevdev_uinput *uinput;
	const char *syspath;
	char path[PATH_MAX];

	_unref_(udev_monitor) *udev_monitor = udev_setup_monitor();
	_unref_(udev_device) *udev_device = NULL;

	uinput = litest_create_uinput(name, id, abs_info, events);

	syspath = libevdev_uinput_get_syspath(uinput);
	snprintf(path, sizeof(path), "%s/event", syspath);

	udev_device = udev_wait_for_device_event(udev_monitor, "add", path);

	litest_assert(udev_device_get_property_value(udev_device, "ID_INPUT"));

	return uinput;
}

static struct libevdev_uinput *
litest_create_uinput_abs_device_v(const char *name,
				  struct input_id *id,
				  const struct input_absinfo *abs,
				  va_list args)
{
	int events[KEY_MAX * 2 + 2]; /* increase this if not sufficient */
	int *event = events;
	int type, code;

	while ((type = va_arg(args, int)) != -1 && (code = va_arg(args, int)) != -1) {
		*event++ = type;
		*event++ = code;
		litest_assert(event < &events[ARRAY_LENGTH(events) - 2]);
	}

	*event++ = -1;
	*event++ = -1;

	return litest_create_uinput_device_from_description(name, id, abs, events);
}

struct libevdev_uinput *
litest_create_uinput_abs_device(const char *name,
				struct input_id *id,
				const struct input_absinfo *abs,
				...)
{
	struct libevdev_uinput *uinput;
	va_list args;

	va_start(args, abs);
	uinput = litest_create_uinput_abs_device_v(name, id, abs, args);
	va_end(args);

	return uinput;
}

struct libevdev_uinput *
litest_create_uinput_device(const char *name, struct input_id *id, ...)
{
	struct libevdev_uinput *uinput;
	va_list args;

	va_start(args, id);
	uinput = litest_create_uinput_abs_device_v(name, id, NULL, args);
	va_end(args);

	return uinput;
}

struct libinput_event_pointer *
litest_is_button_event(struct libinput_event *event,
		       unsigned int button,
		       enum libinput_button_state state)
{
	struct libinput_event_pointer *ptrev;
	enum libinput_event_type type = LIBINPUT_EVENT_POINTER_BUTTON;

	litest_assert_ptr_notnull(event);
	litest_assert_event_type(event, type);
	ptrev = libinput_event_get_pointer_event(event);
	litest_assert_int_eq(libinput_event_pointer_get_button(ptrev), button);
	litest_assert_int_eq(libinput_event_pointer_get_button_state(ptrev), state);

	return ptrev;
}

struct libinput_event_pointer *
litest_is_axis_event(struct libinput_event *event,
		     enum libinput_event_type axis_type,
		     enum libinput_pointer_axis axis,
		     enum libinput_pointer_axis_source source)
{
	struct libinput_event_pointer *ptrev;

	litest_assert(axis_type == LIBINPUT_EVENT_POINTER_SCROLL_WHEEL ||
		      axis_type == LIBINPUT_EVENT_POINTER_SCROLL_FINGER ||
		      axis_type == LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS);

	litest_assert_ptr_notnull(event);
	litest_assert_event_type_is_one_of(event,
					   LIBINPUT_EVENT_POINTER_AXIS,
					   axis_type);
	ptrev = libinput_event_get_pointer_event(event);
	litest_assert(libinput_event_pointer_has_axis(ptrev, axis));

	if (source != 0)
		litest_assert_int_eq(litest_event_pointer_get_axis_source(ptrev),
				     source);

	return ptrev;
}

bool
litest_is_high_res_axis_event(struct libinput_event *event)
{
	litest_assert_event_type_is_one_of(event,
					   LIBINPUT_EVENT_POINTER_AXIS,
					   LIBINPUT_EVENT_POINTER_SCROLL_WHEEL,
					   LIBINPUT_EVENT_POINTER_SCROLL_FINGER,
					   LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS);

	return (libinput_event_get_type(event) != LIBINPUT_EVENT_POINTER_AXIS);
}

struct libinput_event_pointer *
litest_is_motion_event(struct libinput_event *event)
{
	struct libinput_event_pointer *ptrev;
	enum libinput_event_type type = LIBINPUT_EVENT_POINTER_MOTION;
	double x, y, ux, uy;

	litest_assert_ptr_notnull(event);
	litest_assert_event_type(event, type);
	ptrev = libinput_event_get_pointer_event(event);

	x = libinput_event_pointer_get_dx(ptrev);
	y = libinput_event_pointer_get_dy(ptrev);
	ux = libinput_event_pointer_get_dx_unaccelerated(ptrev);
	uy = libinput_event_pointer_get_dy_unaccelerated(ptrev);

	/* No 0 delta motion events */
	litest_assert(x != 0.0 || y != 0.0 || ux != 0.0 || uy != 0.0);

	return ptrev;
}

void
_litest_assert_key_event(struct libinput *li,
			 unsigned int key,
			 enum libinput_key_state state,
			 const char *func,
			 int lineno)
{
	litest_wait_for_event(li);
	_destroy_(libinput_event) *event = libinput_get_event(li);
	litest_is_keyboard_event(event, key, state);
}

void
_litest_assert_button_event(struct libinput *li,
			    unsigned int button,
			    enum libinput_button_state state,
			    const char *func,
			    int line)
{
	_litest_checkpoint(func,
			   line,
			   ANSI_CYAN,
			   "asserting button event %s (%d) state %d",
			   libevdev_event_code_get_name(EV_KEY, button),
			   button,
			   state);

	litest_wait_for_event(li);
	_destroy_(libinput_event) *event = libinput_get_event(li);
	litest_is_button_event(event, button, state);
}

struct libinput_event_touch *
litest_is_touch_event(struct libinput_event *event, enum libinput_event_type type)
{
	struct libinput_event_touch *touch;

	litest_assert_ptr_notnull(event);

	if (type == 0)
		type = libinput_event_get_type(event);

	switch (type) {
	case LIBINPUT_EVENT_TOUCH_DOWN:
	case LIBINPUT_EVENT_TOUCH_UP:
	case LIBINPUT_EVENT_TOUCH_MOTION:
	case LIBINPUT_EVENT_TOUCH_FRAME:
	case LIBINPUT_EVENT_TOUCH_CANCEL:
		litest_assert_event_type(event, type);
		break;
	default:
		litest_abort_msg("%s: invalid touch type %d", __func__, type);
	}

	touch = libinput_event_get_touch_event(event);

	return touch;
}

struct libinput_event_keyboard *
litest_is_keyboard_event(struct libinput_event *event,
			 unsigned int key,
			 enum libinput_key_state state)
{
	struct libinput_event_keyboard *kevent;
	enum libinput_event_type type = LIBINPUT_EVENT_KEYBOARD_KEY;

	litest_assert_ptr_notnull(event);
	litest_assert_event_type(event, type);

	kevent = libinput_event_get_keyboard_event(event);
	litest_assert_ptr_notnull(kevent);

	litest_assert_int_eq(libinput_event_keyboard_get_key(kevent), key);
	litest_assert_int_eq(libinput_event_keyboard_get_key_state(kevent), state);
	return kevent;
}

struct libinput_event_gesture *
litest_is_gesture_event(struct libinput_event *event,
			enum libinput_event_type type,
			int nfingers)
{
	struct libinput_event_gesture *gevent;

	litest_assert_ptr_notnull(event);
	litest_assert_event_type(event, type);

	gevent = libinput_event_get_gesture_event(event);
	litest_assert_ptr_notnull(gevent);

	if (nfingers != -1)
		litest_assert_int_eq(libinput_event_gesture_get_finger_count(gevent),
				     nfingers);
	return gevent;
}

void
_litest_assert_gesture_event(struct libinput *li,
			     enum libinput_event_type type,
			     int nfingers,
			     const char *func,
			     int line)
{

	_litest_checkpoint(func,
			   line,
			   ANSI_CYAN,
			   "asserting gesture event %s %dfg",
			   litest_event_type_str(type),
			   nfingers);

	litest_wait_for_event(li);

	_destroy_(libinput_event) *event = libinput_get_event(li);
	litest_is_gesture_event(event, type, nfingers);
}

struct libinput_event_tablet_tool *
litest_is_tablet_event(struct libinput_event *event, enum libinput_event_type type)
{
	struct libinput_event_tablet_tool *tevent;

	litest_assert_ptr_notnull(event);
	litest_assert_event_type(event, type);

	tevent = libinput_event_get_tablet_tool_event(event);
	litest_assert_ptr_notnull(tevent);

	return tevent;
}

void
_litest_assert_tablet_button_event(struct libinput *li,
				   unsigned int button,
				   enum libinput_button_state state,
				   const char *func,
				   int lineno)
{
	struct libinput_event_tablet_tool *tev;
	enum libinput_event_type type = LIBINPUT_EVENT_TABLET_TOOL_BUTTON;

	_litest_checkpoint(func,
			   lineno,
			   ANSI_CYAN,
			   "asserting tablet button event button %d down: %s",
			   button,
			   yesno(state));

	litest_wait_for_event(li);

	_destroy_(libinput_event) *event = libinput_get_event(li);
	litest_assert_notnull(event);
	litest_assert_event_type(event, type);
	tev = libinput_event_get_tablet_tool_event(event);
	litest_assert_int_eq(libinput_event_tablet_tool_get_button(tev), button);
	litest_assert_int_eq(libinput_event_tablet_tool_get_button_state(tev), state);
}

struct libinput_event_tablet_tool *
litest_is_proximity_event(struct libinput_event *event,
			  enum libinput_tablet_tool_proximity_state state)
{
	struct libinput_event_tablet_tool *tev;
	enum libinput_event_type type = LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY;

	litest_assert_notnull(event);
	litest_assert_event_type(event, type);
	tev = libinput_event_get_tablet_tool_event(event);
	litest_assert_int_eq(libinput_event_tablet_tool_get_proximity_state(tev),
			     state);
	return tev;
}

double
litest_event_pointer_get_value(struct libinput_event_pointer *ptrev,
			       enum libinput_pointer_axis axis)
{
	struct libinput_event *event;
	enum libinput_event_type type;

	event = libinput_event_pointer_get_base_event(ptrev);
	type = libinput_event_get_type(event);

	switch (type) {
	case LIBINPUT_EVENT_POINTER_AXIS:
		return libinput_event_pointer_get_axis_value(ptrev, axis);
	case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
		return libinput_event_pointer_get_scroll_value_v120(ptrev, axis);
	case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
	case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
		return libinput_event_pointer_get_scroll_value(ptrev, axis);
	default:
		abort();
	}
}

enum libinput_pointer_axis_source
litest_event_pointer_get_axis_source(struct libinput_event_pointer *ptrev)
{
	struct libinput_event *event;
	enum libinput_event_type type;

	event = libinput_event_pointer_get_base_event(ptrev);
	type = libinput_event_get_type(event);

	if (type == LIBINPUT_EVENT_POINTER_AXIS)
		return libinput_event_pointer_get_axis_source(ptrev);

	switch (type) {
	case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
		return LIBINPUT_POINTER_AXIS_SOURCE_WHEEL;
	case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
		return LIBINPUT_POINTER_AXIS_SOURCE_FINGER;
	case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
		return LIBINPUT_POINTER_AXIS_SOURCE_CONTINUOUS;
	default:
		abort();
	}
}

void
_litest_assert_tablet_axis_event(struct libinput *li, const char *func, int lineno)
{
	_litest_checkpoint(func, lineno, ANSI_CYAN, "asserting axis event");

	litest_wait_for_event(li);
	_destroy_(libinput_event) *event = libinput_get_event(li);
	litest_is_tablet_event(event, LIBINPUT_EVENT_TABLET_TOOL_AXIS);
}

void
_litest_assert_tablet_proximity_event(struct libinput *li,
				      enum libinput_tablet_tool_proximity_state state,
				      const char *func,
				      int lineno)
{
	_litest_checkpoint(func,
			   lineno,
			   ANSI_CYAN,
			   "asserting proximity %s event",
			   state ? "in" : "out");

	litest_wait_for_event(li);
	_destroy_(libinput_event) *event = libinput_get_event(li);
	litest_is_proximity_event(event, state);
}

void
_litest_assert_tablet_tip_event(struct libinput *li,
				enum libinput_tablet_tool_tip_state state,
				const char *func,
				int lineno)
{
	struct libinput_event_tablet_tool *tev;
	enum libinput_event_type type = LIBINPUT_EVENT_TABLET_TOOL_TIP;

	_litest_checkpoint(func,
			   lineno,
			   ANSI_CYAN,
			   "asserting tip %s event",
			   state ? "down" : "up");

	litest_wait_for_event(li);

	_destroy_(libinput_event) *event = libinput_get_event(li);
	litest_assert_notnull(event);
	litest_assert_event_type(event, type);
	tev = libinput_event_get_tablet_tool_event(event);
	litest_assert_int_eq(libinput_event_tablet_tool_get_tip_state(tev), state);
}

struct libinput_event_tablet_pad *
litest_is_pad_button_event(struct libinput_event *event,
			   unsigned int button,
			   enum libinput_button_state state)
{
	struct libinput_event_tablet_pad *p;
	enum libinput_event_type type = LIBINPUT_EVENT_TABLET_PAD_BUTTON;

	litest_assert_ptr_notnull(event);
	litest_assert_event_type(event, type);

	p = libinput_event_get_tablet_pad_event(event);
	litest_assert_ptr_notnull(p);

	litest_assert_int_eq(libinput_event_tablet_pad_get_button_number(p), button);
	litest_assert_int_eq(libinput_event_tablet_pad_get_button_state(p), state);

	return p;
}

struct libinput_event_tablet_pad *
litest_is_pad_dial_event(struct libinput_event *event, unsigned int number)
{
	struct libinput_event_tablet_pad *p;
	enum libinput_event_type type = LIBINPUT_EVENT_TABLET_PAD_DIAL;

	litest_assert_ptr_notnull(event);
	litest_assert_event_type(event, type);
	p = libinput_event_get_tablet_pad_event(event);

	litest_assert_int_eq(libinput_event_tablet_pad_get_dial_number(p), number);

	return p;
}

struct libinput_event_tablet_pad *
litest_is_pad_ring_event(struct libinput_event *event,
			 unsigned int number,
			 enum libinput_tablet_pad_ring_axis_source source)
{
	struct libinput_event_tablet_pad *p;
	enum libinput_event_type type = LIBINPUT_EVENT_TABLET_PAD_RING;

	litest_assert_ptr_notnull(event);
	litest_assert_event_type(event, type);
	p = libinput_event_get_tablet_pad_event(event);

	litest_assert_int_eq(libinput_event_tablet_pad_get_ring_number(p), number);
	litest_assert_int_eq(libinput_event_tablet_pad_get_ring_source(p), source);

	return p;
}

struct libinput_event_tablet_pad *
litest_is_pad_strip_event(struct libinput_event *event,
			  unsigned int number,
			  enum libinput_tablet_pad_strip_axis_source source)
{
	struct libinput_event_tablet_pad *p;
	enum libinput_event_type type = LIBINPUT_EVENT_TABLET_PAD_STRIP;

	litest_assert_ptr_notnull(event);
	litest_assert_event_type(event, type);
	p = libinput_event_get_tablet_pad_event(event);

	litest_assert_int_eq(libinput_event_tablet_pad_get_strip_number(p), number);
	litest_assert_int_eq(libinput_event_tablet_pad_get_strip_source(p), source);

	return p;
}

struct libinput_event_tablet_pad *
litest_is_pad_key_event(struct libinput_event *event,
			unsigned int key,
			enum libinput_key_state state)
{
	struct libinput_event_tablet_pad *p;
	enum libinput_event_type type = LIBINPUT_EVENT_TABLET_PAD_KEY;

	litest_assert(event != NULL);
	litest_assert_event_type(event, type);

	p = libinput_event_get_tablet_pad_event(event);
	litest_assert(p != NULL);

	litest_assert_int_eq(libinput_event_tablet_pad_get_key(p), key);
	litest_assert_int_eq(libinput_event_tablet_pad_get_key_state(p), state);

	return p;
}

struct libinput_event_switch *
litest_is_switch_event(struct libinput_event *event,
		       enum libinput_switch sw,
		       enum libinput_switch_state state)
{
	struct libinput_event_switch *swev;
	enum libinput_event_type type = LIBINPUT_EVENT_SWITCH_TOGGLE;

	litest_assert_notnull(event);
	litest_assert_event_type(event, type);
	swev = libinput_event_get_switch_event(event);

	litest_assert_int_eq(libinput_event_switch_get_switch(swev), sw);
	litest_assert_int_eq(libinput_event_switch_get_switch_state(swev), state);

	return swev;
}

void
_litest_assert_switch_event(struct libinput *li,
			    enum libinput_switch sw,
			    enum libinput_switch_state state,
			    const char *func,
			    int lineno)
{
	litest_wait_for_event(li);

	_destroy_(libinput_event) *event = libinput_get_event(li);
	litest_is_switch_event(event, sw, state);
}

void
_litest_assert_pad_button_event(struct libinput *li,
				unsigned int button,
				enum libinput_button_state state,
				const char *func,
				int lineno)
{
	litest_wait_for_event(li);

	_destroy_(libinput_event) *event = libinput_get_event(li);
	litest_is_pad_button_event(event, button, state);
}

void
_litest_assert_pad_key_event(struct libinput *li,
			     unsigned int key,
			     enum libinput_key_state state,
			     const char *func,
			     int lineno)
{
	litest_wait_for_event(li);

	_destroy_(libinput_event) *event = libinput_get_event(li);
	litest_is_pad_key_event(event, key, state);
}

void
litest_assert_scroll(struct libinput *li,
		     enum libinput_event_type axis_type,
		     enum libinput_pointer_axis axis,
		     int minimum_movement)
{
	struct libinput_event *event;
	struct libinput_event_pointer *ptrev;
	bool last_hi_res_event_found, last_low_res_event_found;
	int value;
	int nevents = 0;

	litest_assert(axis_type == LIBINPUT_EVENT_POINTER_SCROLL_WHEEL ||
		      axis_type == LIBINPUT_EVENT_POINTER_SCROLL_FINGER ||
		      axis_type == LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS);

	last_hi_res_event_found = false;
	last_low_res_event_found = false;
	event = libinput_get_event(li);
	litest_assert_ptr_notnull(event);

	while (event) {
		int min = minimum_movement;

		ptrev = litest_is_axis_event(event, axis_type, axis, 0);
		nevents++;

		/* Due to how the hysteresis works on touchpad
		 * events, the first event is reduced by the
		 * hysteresis margin that can cause the first event
		 * go under the minimum we expect for all other
		 * events */
		if (nevents == 1)
			min = minimum_movement / 2;

		value = litest_event_pointer_get_value(ptrev, axis);
		if (litest_is_high_res_axis_event(event)) {
			litest_assert(!last_hi_res_event_found);

			if (axis_type == LIBINPUT_EVENT_POINTER_SCROLL_WHEEL)
				min *= 120;

			if (value == 0)
				last_hi_res_event_found = true;
		} else {
			litest_assert(!last_low_res_event_found);

			if (value == 0)
				last_low_res_event_found = true;
		}

		if (value != 0) {
			if (minimum_movement > 0)
				litest_assert_int_ge(value, min);
			else
				litest_assert_int_le(value, min);
		}

		libinput_event_destroy(event);
		event = libinput_get_event(li);
	}

	litest_assert(last_low_res_event_found);
	litest_assert(last_hi_res_event_found);
}

void
litest_assert_axis_end_sequence(struct libinput *li,
				enum libinput_event_type axis_type,
				enum libinput_pointer_axis axis,
				enum libinput_pointer_axis_source source)
{
	struct libinput_event_pointer *ptrev;
	bool last_hi_res_event_found, last_low_res_event_found;
	double val;
	int i;

	litest_assert(axis_type == LIBINPUT_EVENT_POINTER_SCROLL_WHEEL ||
		      axis_type == LIBINPUT_EVENT_POINTER_SCROLL_FINGER ||
		      axis_type == LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS);

	last_hi_res_event_found = false;
	last_low_res_event_found = false;

	/* both high and low scroll end events must be sent */
	for (i = 0; i < 2; i++) {
		_destroy_(libinput_event) *event = libinput_get_event(li);
		ptrev = litest_is_axis_event(event, axis_type, axis, source);
		val = litest_event_pointer_get_value(ptrev, axis);
		litest_assert(val == 0.0);

		if (litest_is_high_res_axis_event(event)) {
			litest_assert(!last_hi_res_event_found);
			last_hi_res_event_found = true;
		} else {
			litest_assert(!last_low_res_event_found);
			last_low_res_event_found = true;
		}
	}

	litest_assert(last_low_res_event_found);
	litest_assert(last_hi_res_event_found);
}

void
_litest_assert_only_typed_events(struct libinput *li,
				 enum libinput_event_type type,
				 const char *func,
				 int line)
{
	struct libinput_event *event;

	litest_assert(type != LIBINPUT_EVENT_NONE);

	_litest_checkpoint(func,
			   line,
			   ANSI_CYAN,
			   "asserting only typed events %s",
			   litest_event_type_str(type));

	libinput_dispatch(li);
	event = libinput_get_event(li);
	litest_assert_notnull(event);

	while (event) {
		litest_assert_event_type(event, type);
		libinput_event_destroy(event);
		libinput_dispatch(li);
		event = libinput_get_event(li);
	}
}

void
litest_assert_only_axis_events(struct libinput *li, enum libinput_event_type axis_type)
{
	struct libinput_event *event;

	litest_assert(axis_type == LIBINPUT_EVENT_POINTER_SCROLL_WHEEL ||
		      axis_type == LIBINPUT_EVENT_POINTER_SCROLL_FINGER ||
		      axis_type == LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS);

	libinput_dispatch(li);
	event = libinput_get_event(li);
	litest_assert_notnull(event);

	while (event) {
		litest_assert_event_type_is_one_of(event,
						   LIBINPUT_EVENT_POINTER_AXIS,
						   axis_type);
		libinput_event_destroy(event);
		libinput_dispatch(li);
		event = libinput_get_event(li);
	}
}

void
litest_assert_no_typed_events(struct libinput *li, enum libinput_event_type type)
{
	struct libinput_event *event;

	litest_assert(type != LIBINPUT_EVENT_NONE);

	libinput_dispatch(li);
	event = libinput_get_event(li);

	while (event) {
		litest_assert_int_ne(libinput_event_get_type(event), type);
		libinput_event_destroy(event);
		libinput_dispatch(li);
		event = libinput_get_event(li);
	}
}

void
litest_assert_touch_sequence(struct libinput *li)
{
	struct libinput_event *event;
	struct libinput_event_touch *tev;
	int slot;

	event = libinput_get_event(li);
	tev = litest_is_touch_event(event, LIBINPUT_EVENT_TOUCH_DOWN);
	slot = libinput_event_touch_get_slot(tev);
	libinput_event_destroy(event);

	event = libinput_get_event(li);
	litest_is_touch_event(event, LIBINPUT_EVENT_TOUCH_FRAME);
	libinput_event_destroy(event);

	event = libinput_get_event(li);
	do {
		tev = litest_is_touch_event(event, LIBINPUT_EVENT_TOUCH_MOTION);
		litest_assert_int_eq(slot, libinput_event_touch_get_slot(tev));
		libinput_event_destroy(event);

		event = libinput_get_event(li);
		litest_is_touch_event(event, LIBINPUT_EVENT_TOUCH_FRAME);
		libinput_event_destroy(event);

		event = libinput_get_event(li);
		litest_assert_notnull(event);
	} while (libinput_event_get_type(event) != LIBINPUT_EVENT_TOUCH_UP);

	tev = litest_is_touch_event(event, LIBINPUT_EVENT_TOUCH_UP);
	litest_assert_int_eq(slot, libinput_event_touch_get_slot(tev));
	libinput_event_destroy(event);
	event = libinput_get_event(li);
	litest_is_touch_event(event, LIBINPUT_EVENT_TOUCH_FRAME);
	libinput_event_destroy(event);
}

void
litest_assert_touch_motion_frame(struct libinput *li)
{
	struct libinput_event *event;

	/* expect at least one, but maybe more */
	event = libinput_get_event(li);
	litest_is_touch_event(event, LIBINPUT_EVENT_TOUCH_MOTION);
	libinput_event_destroy(event);

	event = libinput_get_event(li);
	litest_is_touch_event(event, LIBINPUT_EVENT_TOUCH_FRAME);
	libinput_event_destroy(event);

	event = libinput_get_event(li);
	while (event) {
		litest_is_touch_event(event, LIBINPUT_EVENT_TOUCH_MOTION);
		libinput_event_destroy(event);

		event = libinput_get_event(li);
		litest_is_touch_event(event, LIBINPUT_EVENT_TOUCH_FRAME);
		libinput_event_destroy(event);

		event = libinput_get_event(li);
	}
}

void
litest_assert_touch_down_frame(struct libinput *li)
{
	_destroy_(libinput_event) *down = libinput_get_event(li);
	litest_is_touch_event(down, LIBINPUT_EVENT_TOUCH_DOWN);

	_destroy_(libinput_event) *frame = libinput_get_event(li);
	litest_is_touch_event(frame, LIBINPUT_EVENT_TOUCH_FRAME);
}

void
litest_assert_touch_up_frame(struct libinput *li)
{
	_destroy_(libinput_event) *up = libinput_get_event(li);
	litest_is_touch_event(up, LIBINPUT_EVENT_TOUCH_UP);

	_destroy_(libinput_event) *frame = libinput_get_event(li);
	litest_is_touch_event(frame, LIBINPUT_EVENT_TOUCH_FRAME);
}

void
litest_assert_touch_cancel(struct libinput *li)
{
	_destroy_(libinput_event) *cancel = libinput_get_event(li);
	litest_is_touch_event(cancel, LIBINPUT_EVENT_TOUCH_CANCEL);

	_destroy_(libinput_event) *frame = libinput_get_event(li);
	litest_is_touch_event(frame, LIBINPUT_EVENT_TOUCH_FRAME);
}

void
_litest_timeout(struct libinput *li, const char *func, int lineno, int millis)
{
	if (li)
		_litest_dispatch(li, func, lineno);
	msleep(millis);
	if (li)
		_litest_dispatch(li, func, lineno);
}

void
_litest_assert_logcapture_no_errors(struct litest_logcapture *capture,
				    const char *file,
				    const char *func,
				    int line)
{
	litest_assert_ptr_notnull(capture);

	if (capture->errors &&
	    strv_for_each((const char **)capture->errors, is_actual_error, NULL)) {
		_autofree_ char *errors = strv_join(capture->errors, "\n");
		_litest_abort_msg(file,
				  line,
				  func,
				  "Unexpected errors in log capture:\n%s\n",
				  errors);
	}
}

void
litest_logcapture_destroy(struct litest_logcapture *c)
{
	strv_free(c->errors);
	strv_free(c->infos);
	strv_free(c->debugs);
	free(c);
}

LIBINPUT_ATTRIBUTE_PRINTF(3, 0)
static void
litest_log_handler_msgcapture(struct libinput *libinput,
			      enum libinput_log_priority pri,
			      const char *format,
			      va_list args)
{
	struct litest_user_data *user_data = libinput_get_user_data(libinput);
	struct litest_logcapture *capture = user_data->private;
	const char *priority = NULL;
	const char *color =
		use_colors ? ANSI_RGB(255, 255, 70) ANSI_RGB_BG(40, 40, 40) : "";

	switch (pri) {
	case LIBINPUT_LOG_PRIORITY_ERROR:
		priority = "error ";
		break;
	case LIBINPUT_LOG_PRIORITY_INFO:
		priority = "info ";
		break;
	case LIBINPUT_LOG_PRIORITY_DEBUG:
		priority = "debug ";
		break;
	}

	_autofree_ char *message = strdup_vprintf(format, args);

	fprintf(stderr,
		"%slitest captured: %-6s%s %s",
		color,
		priority,
		use_colors ? ANSI_NORMAL : "",
		message);

	switch (pri) {
	case LIBINPUT_LOG_PRIORITY_ERROR:
		capture->errors = strv_append_take(capture->errors, &message);
		break;
	case LIBINPUT_LOG_PRIORITY_INFO:
		capture->infos = strv_append_take(capture->infos, &message);
		break;
	case LIBINPUT_LOG_PRIORITY_DEBUG:
		capture->debugs = strv_append_take(capture->debugs, &message);
		break;
	}
}

struct litest_logcapture *
litest_logcapture_setup(struct libinput *li)
{
	struct litest_logcapture *c = zalloc(sizeof(*c));
	litest_context_set_user_data(li, c);
	libinput_log_set_handler(li, litest_log_handler_msgcapture);
	return c;
}

struct litest_logcapture *
litest_logcapture_remove(struct libinput *li, struct litest_logcapture *capture)
{
	litest_restore_log_handler(li);
	litest_logcapture_destroy(capture);
	return NULL;
}

#define litest_with_logcapture(li_, capture_)                                  \
  for (struct litest_logcapture *capture_ = litest_logcapture_setup(li_);      \
       capture_ != NULL; capture_ = litest_logcapture_remove(li_, capture_))

void
_litest_assert_strv_substring(char **strv,
			      char *substring,
			      const char *file,
			      const char *func,
			      int line)
{
	if (!strv) {
		_litest_abort_msg(file,
				  line,
				  func,
				  "Expected substring '%s' but strv is NULL",
				  substring);
	}

	bool found = strv_find_substring(strv, substring, NULL);
	if (!found) {
		_autofree_ char *strv_str = strv_join(strv, "', '");
		_litest_abort_msg(file,
				  line,
				  func,
				  "Expected substring '%s' not found in strv: ['%s']",
				  substring,
				  strv_str);
	}
}

void
litest_push_event_frame(struct litest_device *dev)
{
	litest_assert_int_ge(dev->skip_ev_syn, 0);
	dev->skip_ev_syn++;
}

void
litest_pop_event_frame(struct litest_device *dev)
{
	litest_assert_int_gt(dev->skip_ev_syn, 0);
	dev->skip_ev_syn--;
	if (dev->skip_ev_syn == 0)
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
}

void
litest_filter_event(struct litest_device *dev, unsigned int type, unsigned int code)
{
	libevdev_disable_event_code(dev->evdev, type, code);
}

void
litest_unfilter_event(struct litest_device *dev, unsigned int type, unsigned int code)
{
	/* would need an non-NULL argument for re-enabling, so simply abort
	 * until we need to be more sophisticated */
	litest_assert_int_ne(type, (unsigned int)EV_ABS);

	libevdev_enable_event_code(dev->evdev, type, code, NULL);
}

static void
send_abs_xy(struct litest_device *d, double x, double y)
{
	struct input_event e;
	int val;

	e.type = EV_ABS;
	e.code = ABS_X;
	e.value = LITEST_AUTO_ASSIGN;
	val = litest_auto_assign_value(d, &e, 0, x, y, NULL, true);
	litest_event(d, EV_ABS, ABS_X, val);

	e.code = ABS_Y;
	val = litest_auto_assign_value(d, &e, 0, x, y, NULL, true);
	litest_event(d, EV_ABS, ABS_Y, val);
}

static void
send_abs_mt_xy(struct litest_device *d, double x, double y)
{
	struct input_event e;
	int val;

	e.type = EV_ABS;
	e.code = ABS_MT_POSITION_X;
	e.value = LITEST_AUTO_ASSIGN;
	val = litest_auto_assign_value(d, &e, 0, x, y, NULL, true);
	litest_event(d, EV_ABS, ABS_MT_POSITION_X, val);

	e.code = ABS_MT_POSITION_Y;
	e.value = LITEST_AUTO_ASSIGN;
	val = litest_auto_assign_value(d, &e, 0, x, y, NULL, true);
	litest_event(d, EV_ABS, ABS_MT_POSITION_Y, val);
}

void
litest_semi_mt_touch_down(struct litest_device *d,
			  struct litest_semi_mt *semi_mt,
			  unsigned int slot,
			  double x,
			  double y)
{
	double t, l, r = 0, b = 0; /* top, left, right, bottom */

	if (d->ntouches_down > 2 || slot > 1)
		return;

	if (d->ntouches_down == 1) {
		l = x;
		t = y;
	} else {
		int other = (slot + 1) % 2;
		l = min(x, semi_mt->touches[other].x);
		t = min(y, semi_mt->touches[other].y);
		r = max(x, semi_mt->touches[other].x);
		b = max(y, semi_mt->touches[other].y);
	}

	send_abs_xy(d, l, t);

	litest_event(d, EV_ABS, ABS_MT_SLOT, 0);

	if (d->ntouches_down == 1)
		litest_event(d, EV_ABS, ABS_MT_TRACKING_ID, ++semi_mt->tracking_id);

	send_abs_mt_xy(d, l, t);

	if (d->ntouches_down == 2) {
		litest_event(d, EV_ABS, ABS_MT_SLOT, 1);
		litest_event(d, EV_ABS, ABS_MT_TRACKING_ID, ++semi_mt->tracking_id);

		send_abs_mt_xy(d, r, b);
	}

	litest_event(d, EV_SYN, SYN_REPORT, 0);

	semi_mt->touches[slot].x = x;
	semi_mt->touches[slot].y = y;
}

void
litest_semi_mt_touch_move(struct litest_device *d,
			  struct litest_semi_mt *semi_mt,
			  unsigned int slot,
			  double x,
			  double y)
{
	double t, l, r = 0, b = 0; /* top, left, right, bottom */

	if (d->ntouches_down > 2 || slot > 1)
		return;

	if (d->ntouches_down == 1) {
		l = x;
		t = y;
	} else {
		int other = (slot + 1) % 2;
		l = min(x, semi_mt->touches[other].x);
		t = min(y, semi_mt->touches[other].y);
		r = max(x, semi_mt->touches[other].x);
		b = max(y, semi_mt->touches[other].y);
	}

	send_abs_xy(d, l, t);

	litest_event(d, EV_ABS, ABS_MT_SLOT, 0);
	send_abs_mt_xy(d, l, t);

	if (d->ntouches_down == 2) {
		litest_event(d, EV_ABS, ABS_MT_SLOT, 1);
		send_abs_mt_xy(d, r, b);
	}

	litest_event(d, EV_SYN, SYN_REPORT, 0);

	semi_mt->touches[slot].x = x;
	semi_mt->touches[slot].y = y;
}

void
litest_semi_mt_touch_up(struct litest_device *d,
			struct litest_semi_mt *semi_mt,
			unsigned int slot)
{
	/* note: ntouches_down is decreased before we get here */
	if (d->ntouches_down >= 2 || slot > 1)
		return;

	litest_event(d, EV_ABS, ABS_MT_SLOT, d->ntouches_down);
	litest_event(d, EV_ABS, ABS_MT_TRACKING_ID, -1);

	/* if we have one finger left, send x/y coords for that finger left.
	   this is likely to happen with a real touchpad */
	if (d->ntouches_down == 1) {
		int other = (slot + 1) % 2;
		send_abs_xy(d, semi_mt->touches[other].x, semi_mt->touches[other].y);
		litest_event(d, EV_ABS, ABS_MT_SLOT, 0);
		send_abs_mt_xy(d, semi_mt->touches[other].x, semi_mt->touches[other].y);
	}

	litest_event(d, EV_SYN, SYN_REPORT, 0);
}

enum litest_mode
litest_parse_argv(int argc, char **argv, int *njobs_out)
{
	enum {
		OPT_EXIT_FIRST,
		OPT_FILTER_TEST,
		OPT_FILTER_DEVICE,
		OPT_FILTER_GROUP,
		OPT_FILTER_RANGEVAL,
		OPT_FILTER_DEVICELESS,
		OPT_FILTER_PARAMETER,
		OPT_OUTPUT_FILE,
		OPT_JOBS,
		OPT_LIST,
		OPT_VERBOSE,
	};
	static const struct option opts[] = {
		{ "filter-test", 1, 0, OPT_FILTER_TEST },
		{ "filter-device", 1, 0, OPT_FILTER_DEVICE },
		{ "filter-group", 1, 0, OPT_FILTER_GROUP },
		{ "filter-rangeval", 1, 0, OPT_FILTER_RANGEVAL },
		{ "filter-deviceless", 0, 0, OPT_FILTER_DEVICELESS },
		{ "filter-parameter", 1, 0, OPT_FILTER_PARAMETER },
		{ "output-file", 1, 0, OPT_OUTPUT_FILE },
		{ "exitfirst", 0, 0, OPT_EXIT_FIRST },
		{ "jobs", 1, 0, OPT_JOBS },
		{ "list", 0, 0, OPT_LIST },
		{ "verbose", 0, 0, OPT_VERBOSE },
		{ "help", 0, 0, 'h' },
		{ 0, 0, 0, 0 }
	};
	enum {
		JOBS_DEFAULT,
		JOBS_NONE,
		JOBS_SINGLE,
		JOBS_CUSTOM
	} want_jobs = JOBS_DEFAULT;
	char *jobs_env;
	int jobs = 0;

	/* If we are not running from the builddir, we assume we're running
	 * against the system as installed */
	if (!builddir_lookup(NULL))
		use_system_rules_quirks = true;

	if (in_debugger)
		want_jobs = JOBS_NONE;

	if ((jobs_env = getenv("LITEST_JOBS"))) {
		if (!safe_atoi(jobs_env, &jobs)) {
			fprintf(stderr,
				"LITEST_JOBS environment variable must be positive integer\n");
			exit(EXIT_FAILURE);
		}
	}

	while (1) {
		int c;
		int option_index = 0;

		c = getopt_long(argc, argv, "j:x", opts, &option_index);
		if (c == -1)
			break;
		switch (c) {
		default:
		case 'h':
			printf("Usage: %s [--verbose] [--jobs] [--filter-...]\n"
			       "\n"
			       "Options:\n"
			       "    -x | --exitfirst\n"
			       "          Exit instantly on first failed test\n"
			       "    --filter-test=.... \n"
			       "          Glob to filter on test names\n"
			       "    --filter-device=.... \n"
			       "          Glob to filter on device names\n"
			       "    --filter-group=.... \n"
			       "          Glob to filter on test groups\n"
			       "    --filter-rangeval=N \n"
			       "          Only run tests with the given range value\n"
			       "    --filter-deviceless=.... \n"
			       "          Glob to filter on tests that do not create test devices\n"
			       "    --filter-parameter=param1:glob,param2:glob,... \n"
			       "          Glob(s) to filter on the given parameters in their string "
			       "representation.\n"
			       "          Boolean parameters are filtered via 'true' and 'false'.\n"
			       "    --verbose\n"
			       "          Enable verbose output\n"
			       "    --jobs 8\n"
			       "          Number of parallel test suites to run (default: 8).\n"
			       "	  This overrides the LITEST_JOBS environment variable.\n"
			       "    --list\n"
			       "          List all tests\n"
			       "\n"
			       "See the libinput-test-suite(1) man page for details.\n",
			       program_invocation_short_name);
			exit(c != 'h');
			break;
		case OPT_FILTER_TEST:
			filter_test = optarg;
			if (want_jobs == JOBS_DEFAULT)
				want_jobs = JOBS_SINGLE;
			break;
		case OPT_FILTER_DEVICE:
			filter_device = optarg;
			if (want_jobs == JOBS_DEFAULT)
				want_jobs = JOBS_SINGLE;
			break;
		case OPT_FILTER_DEVICELESS:
			run_deviceless = true;
			break;
		case OPT_FILTER_GROUP:
			filter_group = optarg;
			break;
		case OPT_FILTER_RANGEVAL:
			filter_rangeval = atoi(optarg);
			break;
		case OPT_FILTER_PARAMETER: {
			size_t nelems;
			_autostrvfree_ char **params =
				strv_from_string(optarg, ",", &nelems);
			const size_t max_filters = ARRAY_LENGTH(filter_params) - 1;
			if (nelems >= max_filters) {
				fprintf(stderr,
					"Only %zd parameter filters are supported\n",
					max_filters);
				exit(1);
			}
			for (size_t i = 0; i < nelems; i++) {
				size_t n;
				_autostrvfree_ char **strv =
					strv_from_string(params[i], ":", &n);
				assert(n == 2);

				const char *name = strv[0];
				const char *glob = strv[1];

				struct param_filter *f = &filter_params[i];
				snprintf(f->name, sizeof(f->name), "%s", name);
				snprintf(f->glob, sizeof(f->glob), "%s", glob);
			}
			break;
		}
		case 'j':
		case OPT_JOBS:
			jobs = atoi(optarg);
			want_jobs = JOBS_CUSTOM;
			break;
		case OPT_LIST:
			return LITEST_MODE_LIST;
		case OPT_VERBOSE:
			verbose = true;
			break;
		case OPT_OUTPUT_FILE:
			outfile = fopen(optarg, "w+");
			if (!outfile) {
				fprintf(stderr, "Failed to open %s: %m\n", optarg);
				exit(1);
			}
			break;
		case 'x':
		case OPT_EXIT_FIRST:
			exit_first = true;
			break;
		}
	}

	switch (want_jobs) {
	case JOBS_SINGLE:
		jobs = 1;
		break;
	case JOBS_NONE:
		jobs = 0;
		break;
	default:
		break;
	}

	*njobs_out = jobs;

	return LITEST_MODE_TEST;
}
