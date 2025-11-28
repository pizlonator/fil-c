/* mutest-utils.c: Utilities
 *
 * µTest - Copyright 2019  Emmanuele Bassi
 *
 * SPDX-License-Identifier: MIT
 */

#include "mutest-private.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef OS_WINDOWS
#include <windows.h>
#include <io.h>
#endif

#ifdef HAVE_MACH_MACH_TIME_H
#include <mach/mach_time.h>
#endif

// mutest_get_current_time:
//
// Returns: the current time, in microseconds
#ifdef HAVE_QUERY_PERFORMANCE_COUNTER
static double usec_per_tick;

int64_t
mutest_get_current_time (void)
{
  if (mutest_unlikely (usec_per_tick == 0))
    {
      LARGE_INTEGER freq;

      if (!QueryPerformanceFrequency (&freq) || freq.QuadPart == 0)
        mutest_assert_if_reached ("QueryPerformanceFrequency failed");

      usec_per_tick = (double) 1000000 / freq.QuadPart;
    }

  LARGE_INTEGER ticks;

  if (QueryPerformanceCounter (&ticks))
    return (int64_t) (ticks.QuadPart * usec_per_tick);

  usec_per_tick = 0;

  return 0;
}
#elif defined(HAVE_CLOCK_GETTIME)
int64_t
mutest_get_current_time (void)
{
  struct timespec ts;
  int res;

  res = clock_gettime (CLOCK_MONOTONIC, &ts);

  if (res != 0)
    return 0;

  return (((int64_t) ts.tv_sec) * 1000000) + (ts.tv_nsec / 1000);
}
#elif defined(HAVE_MACH_MACH_TIME_H)
int64_t
mutest_get_current_time (void)
{
  static mach_timebase_info_data_t timebase_info;

  if (timebase_info.denom == 0)
    {
      mach_timebase_info (&timebase_info);

      if (timebase_info.number % 1000 == 0)
        timebase_info.number /= 1000;
      else
        timebase_info.number *= 1000;

      if (timebase_info.denom % timebase_info.number == 0)
        {
          timebase_info.denom /= timebase_info.number;
          timebase_info.number = 1;
        }
    }

  return mach_absolute_time () / timebase_info.denom;
}
#elif defined(HAVE_GETTIMEOFDAY)
int64_t
mutest_get_current_time (void)
{
  struct timeval r;

  // Fall back to gettimeofday(); it's not even remotely monotonic,
  // but it's likely better than nothing
  gettimeofday (&r, NULL);

  return (((int64_t) r.tv_sec) * 1000000) + r.tv_usec;
}
#else
# error "muTest requires a monotonic clock implementation"
#endif

char *
mutest_strdup_and_len (const char *str,
                       size_t *len_p)
{
  if (str == NULL)
    {
      if (len_p != NULL)
        *len_p = 0;

      return NULL;
    }

  size_t len = strlen (str) + 1;
  char *res = malloc (len * sizeof (char));
  if (res == NULL)
    mutest_oom_abort ();

  memcpy (res, str, len);

  if (len_p != NULL)
    *len_p = len - 1;

  return res;
}

char *
mutest_strdup (const char *str)
{
  return mutest_strdup_and_len (str, NULL);
}

char *
mutest_strndup (const char *str,
                size_t len)
{
  if (str == NULL || len == 0)
    return NULL;

  size_t str_len = strlen (str) + 1;
  if (len >= str_len)
    return mutest_strdup (str);

  char *res = malloc ((len + 1) * sizeof (char));
  if (res == NULL)
    mutest_oom_abort ();

  memcpy (res, str, len * sizeof (char));
  res[len] = '\0';

  return res;
}

char *
mutest_getenv (const char *str)
{
#ifdef HAVE__DUPENV_S
  size_t len = 0;
  char *res = NULL;

  errno_t err = _dupenv_s (&res, &len, str);
  if (err)
    return NULL;

  return res;
#else
  return mutest_strdup (getenv (str));
#endif
}

void
mutest_print (FILE *stream,
              const char *first_fragment,
              ...)
{
  va_list args;

  va_start (args, first_fragment);
  
  const char *fragment = first_fragment;
  while (fragment != NULL)
    {
#ifdef OS_WINDOWS
      if (fragment[0] != '\0')
        fputs (fragment, stream);
#else
      if (fragment[0] != '\0')
        {
          size_t fragment_len = strlen (fragment);
          ssize_t res = write (fileno (stream), fragment, fragment_len);
          if (res < 0)
            {
              perror ("write");
              abort ();
            }
        }
#endif

      fragment = va_arg (args, char *);
    }

  va_end (args);

#ifdef OS_WINDOWS
  fputc ('\n', stream);
#else
  if (write (fileno (stream), "\n", 1) < 0)
    {
      perror ("write");
      abort ();
    }
#endif
}

void
mutest_assert_message (const char *file,
                       int         line,
                       const char *func,
                       const char *message)
{
  char lstr[32];

  /* We can be called by an OOM handler, or a signal handler,
   * so we should not:
   *
   *  - allocate memory
   *  - re-enter into mutest code
   */
  snprintf (lstr, 32, "%d", line);

  if (mutest_use_colors ())
    mutest_print (stderr,
                  MUTEST_COLOR_RED, "ERROR", MUTEST_COLOR_NONE, ": ",
                  file, ":", lstr, ":", func != NULL ? func : "<local>",
                  " ",
                  message,
                  NULL);
  else
    mutest_print (stderr,
                  "ERROR: ", file, ":", lstr, ":", func != NULL ? func : "<local>", " ",
                  message,
                  NULL);

  abort ();
}

double
mutest_format_time (int64_t t,
                    const char **unit)
{
  if (t > 1000000)
    {
      *unit = "s";
      return (double) t / 1000000.0;
    }

  if (t > 1000)
    {
      *unit = "ms";
      return (double) t / 1000.0;
    }

  *unit = "µs";
  return (double) t;
}

static char *
mutest_stpcpy (char *dest,
               const char *src)
{
#ifdef HAVE_STPCPY
  return stpcpy (dest, src);
#else
  char *d = dest;
  const char *s = src;

  do
    *d++ = *s;
  while (*s++ != '\0');

  return d - 1;
#endif
}

static char *
string_split (const char *str,
              char indent_ch,
              size_t indent_len,
              size_t max_width)
{
  if (indent_len >= max_width)
    mutest_assert_if_reached ("invalid indentation length");

  const size_t max_len = max_width - indent_len;

  size_t n_lines = 0;
  char **lines = malloc (sizeof (char *) * 16);
  if (mutest_unlikely (lines == NULL))
    mutest_oom_abort ();

  const char *p = str;
  while (*p != '\0')
    {
      size_t remainder = strlen (p);
      const char *line_start = p;
      const char *line_end = line_start + remainder;

      size_t line_len = line_end - line_start;
      bool trailing_space = false;

      if (line_len > max_len)
        {
          line_len = max_len - 1;
          line_end = line_start + line_len;

          while (line_len > 0)
            {
              if (*line_end == ' ')
                {
                  trailing_space = true;
                  break;
                }

              line_end -= 1;
              line_len -= 1;
            }
        }
      char *line = malloc (sizeof (char) * (line_len + 1));
      if (mutest_unlikely (line == NULL))
        mutest_oom_abort ();

      memcpy (line, p, line_len);
      line[line_len] = '\0';

      if ((n_lines + 1) % 16 == 0)
        lines = realloc (lines, sizeof (char *) * (16 + n_lines));

      lines[n_lines] = line;

      p += line_len + (trailing_space ? 1 : 0);
      n_lines += 1;
    }

  if (n_lines == 0)
    mutest_assert_if_reached ("invalid line length");

  size_t len = strlen (lines[0]) + 1;
  size_t i;

  for (i = 1; i < n_lines; i++)
    len += indent_len + strlen (lines[i]);
  len += 1 * (i - 1);

  char *indent = NULL;
  if (indent_len > 0)
    {
      indent = malloc (sizeof (char) * (indent_len + 1));
      if (mutest_unlikely (indent == NULL))
        mutest_oom_abort ();

      for (i = 0; i < indent_len; i++)
        indent[i] = indent_ch;

      indent[indent_len] = '\0';
    }

  char *res = malloc (sizeof (char) * (len + 1));
  if (mutest_unlikely (res == NULL))
    mutest_oom_abort ();

  char *ptr = mutest_stpcpy (res, *lines);
  for (i = 1; i < n_lines; i++)
    {
      ptr = mutest_stpcpy (ptr, "\n");
      if (indent != NULL)
        ptr = mutest_stpcpy (ptr, indent);
      ptr = mutest_stpcpy (ptr, lines[i]);
      free (lines[i]);
    }

  res[len] = '\0';

  free (indent);
  free (lines);

  return res;
}


char *
mutest_format_string_for_display (const char *str,
                                  char indent_ch,
                                  size_t indent_len)
{
  mutest_state_t *state = mutest_get_global_state ();

  if (!state->is_tty)
    return mutest_strdup (str);

  if ((size_t) state->term_width > (strlen (str) + indent_len))
    return mutest_strdup (str);

  return string_split (str, indent_ch, indent_len, state->term_width);
}

static struct {
  const mutest_formatter_t *(* get_formatter) (void);
  const char *formatter;
} output_formatters[] = {
  [MUTEST_OUTPUT_MOCHA] = {
    .get_formatter = mutest_get_mocha_formatter,
    .formatter = "mocha",
  },
  [MUTEST_OUTPUT_TAP] = {
    .get_formatter = mutest_get_tap_formatter,
    .formatter = "TAP",
  },
};

static const mutest_formatter_t *
mutest_get_formatter (void)
{
  mutest_output_format_t format = mutest_get_output_format ();

  return output_formatters[format].get_formatter ();
}

void
mutest_format_spec_preamble (mutest_spec_t *spec)
{
  const mutest_formatter_t *vtable = mutest_get_formatter ();

  if (vtable->spec_preamble != NULL)
    vtable->spec_preamble (spec);
}

void
mutest_format_spec_results (mutest_spec_t *spec)
{
  const mutest_formatter_t *vtable = mutest_get_formatter ();

  if (vtable->spec_results != NULL)
    vtable->spec_results (spec);
}

void
mutest_format_suite_results (mutest_suite_t *suite)
{
  const mutest_formatter_t *vtable = mutest_get_formatter ();

  if (vtable->suite_results != NULL)
    vtable->suite_results (suite);
}

void
mutest_format_total_results (mutest_state_t *state)
{
  const mutest_formatter_t *vtable = mutest_get_formatter ();

  if (vtable->total_results != NULL)
    vtable->total_results (state);
}

void
mutest_format_suite_preamble (mutest_suite_t *suite)
{
  const mutest_formatter_t *vtable = mutest_get_formatter ();

  if (vtable->suite_preamble != NULL)
    vtable->suite_preamble (suite);
}

void
mutest_format_expect_fail (mutest_expect_t *expect,
                           bool negate,
                           mutest_expect_res_t *check,
                           const char *check_repr)
{
  const mutest_formatter_t *vtable = mutest_get_formatter ();

  if (vtable->expect_fail != NULL)
    vtable->expect_fail (expect, negate, check, check_repr);
}

void
mutest_format_expect_result (mutest_expect_t *expect)
{
  const mutest_formatter_t *vtable = mutest_get_formatter ();

  if (vtable->expect_result != NULL)
    vtable->expect_result (expect);
}
