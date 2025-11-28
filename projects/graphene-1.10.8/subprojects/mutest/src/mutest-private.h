/* mutest-private.h: Private declarations
 *
 * µTest
 *
 * Copyright 2019  Emmanuele Bassi
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "config.h"

#include "mutest.h"

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

MUTEST_BEGIN_DECLS

typedef enum {
  MUTEST_RESULT_PASS,
  MUTEST_RESULT_FAIL,
  MUTEST_RESULT_SKIP
} mutest_result_t;

typedef enum {
  MUTEST_EXPECT_INVALID,

  MUTEST_EXPECT_BOOLEAN,
  MUTEST_EXPECT_INT,
  MUTEST_EXPECT_INT_RANGE,
  MUTEST_EXPECT_FLOAT,
  MUTEST_EXPECT_FLOAT_RANGE,
  MUTEST_EXPECT_STR,
  MUTEST_EXPECT_POINTER
} mutest_expect_type_t;

typedef enum {
  MUTEST_COLLECT_NONE = 0,
  MUTEST_COLLECT_INT = 1 << 0,
  MUTEST_COLLECT_FLOAT = 1 << 1,
  MUTEST_COLLECT_STRING = 1 << 2,
  MUTEST_COLLECT_POINTER = 1 << 3,
  MUTEST_COLLECT_BOOLEAN = 1 << 4,
  MUTEST_COLLECT_PRECISION = 1 << 5,
  MUTEST_COLLECT_RANGE = 1 << 6,
  MUTEST_COLLECT_MATCHING_TYPE = 1 << 7,

  MUTEST_COLLECT_NUMBER = MUTEST_COLLECT_INT | MUTEST_COLLECT_FLOAT,
  MUTEST_COLLECT_SCALAR = MUTEST_COLLECT_INT |
                          MUTEST_COLLECT_FLOAT |
                          MUTEST_COLLECT_STRING |
                          MUTEST_COLLECT_POINTER |
                          MUTEST_COLLECT_BOOLEAN
} mutest_collect_type_t;

typedef enum {
  MUTEST_OUTPUT_MOCHA,
  MUTEST_OUTPUT_TAP
} mutest_output_format_t;

typedef struct {
  bool initialized;

  int term_width;
  int term_height;

  bool is_tty;
  bool use_colors;

  mutest_suite_t *current_suite;
  mutest_spec_t *current_spec;

  int n_suites;
  int total_tests;
  int total_pass;
  int total_fail;
  int total_skip;

  int64_t start_time;
  int64_t end_time;

  mutest_output_format_t output_format;

  mutest_hook_func_t before_hook;
  mutest_hook_func_t after_hook;
} mutest_state_t;

typedef struct {
  void (* suite_preamble) (mutest_suite_t *suite);
  void (* spec_preamble) (mutest_spec_t *spec);
  void (* expect_result) (mutest_expect_t *expect);
  void (* expect_fail) (mutest_expect_t *expect,
                        bool negate,
                        mutest_expect_res_t *check,
                        const char *check_repr);
  void (* spec_results) (mutest_spec_t *spec);
  void (* suite_results) (mutest_suite_t *suite);
  void (* total_results) (mutest_state_t *state);
} mutest_formatter_t;

typedef mutest_expect_res_t *(* mutest_collect_func_t) (mutest_expect_type_t expect_type,
                                                        mutest_collect_type_t collect_type,
                                                        va_list *args);

struct _mutest_expect_res_t
{
  mutest_expect_type_t expect_type;

  union {
    bool v_bool;

    struct {
      int value;
      int tolerance;
    } v_int;

    struct {
      int min;
      int max;
    } v_irange;

    struct {
      double value;
      double tolerance;
    } v_float;

    struct {
      double min;
      double max;
    } v_frange;

    struct {
      char *str;
      size_t len;
    } v_str;

    void *v_pointer;
  } expect;
};

struct _mutest_expect_t
{
  const char *file;
  int line;
  const char *func_name;

  const char *description;

  const char *skip_reason;

  mutest_expect_res_t *value;

  mutest_result_t result;
};

struct _mutest_spec_t
{
  const char *file;
  int line;
  const char *func_name;

  const char *description;

  int n_expects;
  int pass;
  int fail;
  int skip;

  int64_t start_time;
  int64_t end_time;

  bool skip_all;
  const char *skip_reason;
};

struct _mutest_suite_t
{
  const char *file;
  int line;
  const char *func_name;

  const char *description;

  int64_t start_time;
  int64_t end_time;

  mutest_hook_func_t before_each_hook;
  mutest_hook_func_t after_each_hook;

  int n_specs;
  int pass;
  int fail;
  int skip;

  bool skip_all;
  const char *skip_reason;
};

#define mutest_oom_abort() \
  mutest_assert_message (__FILE__, __LINE__, __func__, "out-of-memory")

#define mutest_assert_if_reached(str) \
  mutest_assert_message (__FILE__, __LINE__, __func__, (str))

#define mutest_assert(x) \
  mutest_assert_message (__FILE__, __LINE__, __func__, #x)

#if defined(__GNUC__) && __GNUC__ > 3
# define mutest_likely(x)       (__builtin_expect((x) ? 1 : 0, 1))
# define mutest_unlikely(x)     (__builtin_expect((x) ? 1 : 0, 0))
#else
# define mutest_likely(x)       (x)
# define mutest_unlikely(x)     (x)
#endif

#define ANSI_ESCAPE "\033"

#define MUTEST_COLOR_NONE               ANSI_ESCAPE "[0m"
#define MUTEST_COLOR_RED                ANSI_ESCAPE "[1;31m"
#define MUTEST_COLOR_GREEN              ANSI_ESCAPE "[1;32m"
#define MUTEST_COLOR_YELLOW             ANSI_ESCAPE "[1;33m"
#define MUTEST_COLOR_BLUE               ANSI_ESCAPE "[1;34m"
#define MUTEST_COLOR_LIGHT_GREY         ANSI_ESCAPE "[1;37m"
#define MUTEST_COLOR_DARK_GREY          ANSI_ESCAPE "[1;90m"

#define MUTEST_BOLD_DEFAULT             ANSI_ESCAPE "[1;39m"
#define MUTEST_DIM_DEFAULT              ANSI_ESCAPE "[2;39m"
#define MUTEST_UNDERLINE_DEFAULT        ANSI_ESCAPE "[4;39m"

mutest_expect_res_t *
mutest_expect_res_alloc (mutest_expect_type_t type);

void
mutest_expect_res_free (mutest_expect_res_t *res);

char *
mutest_strdup (const char *str);

char *
mutest_strndup (const char *str,
                size_t len);

char *
mutest_strdup_and_len (const char *str,
                       size_t *len);

char *
mutest_getenv (const char *env_name);

void
mutest_print (FILE *stram,
              const char *first_fragment,
              ...) MUTEST_NULL_TERMINATED;

void
mutest_assert_message (const char *file,
                       int line,
                       const char *func,
                       const char *message) MUTEST_NO_RETURN;

mutest_state_t *
mutest_get_global_state (void);

int
mutest_get_term_width (void);

int
mutest_get_term_height (void);

bool
mutest_is_term_tty (void);

bool
mutest_use_colors (void);

mutest_output_format_t
mutest_get_output_format (void);

void
mutest_set_current_suite (mutest_suite_t *suite);

mutest_suite_t *
mutest_get_current_suite (void);

void
mutest_set_current_spec (mutest_spec_t *spec);

mutest_spec_t *
mutest_get_current_spec (void);

int64_t
mutest_get_current_time (void);

double
mutest_format_time (int64_t t,
                    const char **unit);

char *
mutest_format_string_for_display (const char *str,
                                  char indent_ch,
                                  size_t indent_len);

void
mutest_expect_res_to_string (mutest_expect_res_t *res,
                             char *buf,
                             size_t len);

void
mutest_expect_diagnostic (mutest_expect_t *expect,
                          bool negate,
                          mutest_expect_res_t *check,
                          const char *check_repr,
                          char **diagnostic,
                          char **location);

void
mutest_spec_add_expect_result (mutest_spec_t *spec,
                               mutest_expect_t *expect);

void
mutest_suite_add_spec_results (mutest_suite_t *suite,
                               mutest_spec_t *spec);

void
mutest_add_suite_results (mutest_suite_t *suite);

int
mutest_get_results (int *total_pass,
                    int *total_fail,
                    int *total_skip);

void
mutest_format_suite_preamble (mutest_suite_t *suite);

void
mutest_format_spec_preamble (mutest_spec_t *spec);

void
mutest_format_expect_result (mutest_expect_t *expect);

void
mutest_format_expect_fail (mutest_expect_t *expect,
                           bool negate,
                           mutest_expect_res_t *check,
                           const char *check_repr);
void
mutest_format_spec_results (mutest_spec_t *spec);

void
mutest_format_suite_results (mutest_suite_t *suite);

void
mutest_format_total_results (mutest_state_t *state);

const mutest_formatter_t *
mutest_get_mocha_formatter (void);

const mutest_formatter_t *
mutest_get_tap_formatter (void);

MUTEST_END_DECLS
