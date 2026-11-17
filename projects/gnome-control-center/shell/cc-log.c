/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-log.c
 *
 * Copyright © 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 * Copyright 2022 Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <glib.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "cc-log.h"

#define DEFAULT_DOMAIN_PREFIX "cc"
#define BLUETOOTH_DOMAIN_PREFIX "Bluetooth"

char *domains;
static int verbosity;
gboolean any_domain;
gboolean no_anonymize;
gboolean stderr_is_journal;
gboolean fatal_criticals, fatal_warnings;

/* Copied from GLib, LGPLv2.1+ */
static void
_g_log_abort (gboolean breakpoint)
{
  gboolean debugger_present;

  if (g_test_subprocess ())
    {
      /* If this is a test case subprocess then it probably caused
       * this error message on purpose, so just exit() rather than
       * abort()ing, to avoid triggering any system crash-reporting
       * daemon.
       */
      _exit (1);
    }

#ifdef G_OS_WIN32
  debugger_present = IsDebuggerPresent ();
#else
  /* Assume GDB is attached. */
  debugger_present = TRUE;
#endif /* !G_OS_WIN32 */

  if (debugger_present && breakpoint)
    G_BREAKPOINT ();
  else
    g_abort ();
}

static gboolean
should_show_log_for_level (GLogLevelFlags log_level,
                           int            verbosity_level)
{
  if (verbosity_level >= 3)
    return TRUE;

  if (log_level & CC_LOG_LEVEL_TRACE)
    return verbosity_level >= 2;

  if (log_level & G_LOG_LEVEL_DEBUG)
    return verbosity_level >= 1;

  if (log_level & G_LOG_LEVEL_INFO)
    return TRUE;

  if (log_level & G_LOG_LEVEL_MESSAGE)
    return TRUE;

  return FALSE;
}

static gboolean
matches_domain (const char *log_domains,
                const char *domain)
{
  g_auto(GStrv) domain_list = NULL;

  if (!log_domains || !*log_domains ||
      !domain || !*domain)
    return FALSE;

  domain_list = g_strsplit (log_domains, ",", -1);

  for (guint i = 0; domain_list[i]; i++)
    {
      if (g_str_has_prefix (domain, domain_list[i]))
        return TRUE;
    }

  return FALSE;
}

static gboolean
should_log (const char     *log_domain,
            GLogLevelFlags  log_level)
{
  g_assert (log_domain);

  /* Ignore custom flags set */
  log_level = log_level & ~CC_LOG_DETAILED;

  /* Don't skip serious logs */
  if (log_level & (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING))
    return TRUE;

  if (any_domain && domains) {
    /* If domain is “all” show logs upto debug regardless of the verbosity */
    if (log_level & ~CC_LOG_LEVEL_TRACE)
      return TRUE;

    /* If the log is trace level, log if verbosity >= 2 */
    return verbosity >= 2;
  }

  if (!domains &&
      (g_str_has_prefix (log_domain, DEFAULT_DOMAIN_PREFIX) ||
       g_str_has_prefix (log_domain, BLUETOOTH_DOMAIN_PREFIX)))
    return should_show_log_for_level (log_level, verbosity);

  if (domains && matches_domain (domains, log_domain))
    return should_show_log_for_level (log_level, verbosity);

  /* If we didn't handle domains in the preceding statement,
   * we should no longer log them */
  if (domains)
    return FALSE;

  /* GdkPixbuf and Gvc logs are too much verbose, skip unless asked not to. */
  if (verbosity < 4 &&
      (g_strcmp0 (log_domain, "GdkPixbuf") == 0 ||
       g_strcmp0 (log_domain, "Gvc") == 0) &&
      (!domains || !strstr (domains, log_domain)))
    return FALSE;

  if (verbosity >= 3)
    return TRUE;

  return FALSE;
}

static void
log_str_append_log_domain (GString    *log_str,
                           const char *log_domain,
                           gboolean    color)
{
  static const char *colors[] = {
    "\033[1;32m",
    "\033[1;33m",
    "\033[1;35m",
    "\033[1;36m",
    "\033[1;91m",
    "\033[1;92m",
    "\033[1;93m",
    "\033[1;94m",
    "\033[1;95m",
    "\033[1;96m",
  };
  guint i;

  g_assert (log_domain && *log_domain);

  i = g_str_hash (log_domain) % G_N_ELEMENTS (colors);

  if (color)
    g_string_append (log_str, colors[i]);
  g_string_append_printf (log_str, "%20s", log_domain);

  if (color)
    g_string_append (log_str, "\033[0m");
}

static const char *
get_log_level_prefix (GLogLevelFlags log_level,
                      gboolean       use_color)
{
  /* Ignore custom flags set */
  log_level = log_level & ~CC_LOG_DETAILED;

  if (use_color)
    {
      switch ((int)log_level)        /* Same colors as used in GLib */
        {
        case G_LOG_LEVEL_ERROR:    return "   \033[1;31mERROR\033[0m";
        case G_LOG_LEVEL_CRITICAL: return "\033[1;35mCRITICAL\033[0m";
        case G_LOG_LEVEL_WARNING:  return " \033[1;33mWARNING\033[0m";
        case G_LOG_LEVEL_MESSAGE:  return " \033[1;32mMESSAGE\033[0m";
        case G_LOG_LEVEL_INFO:     return "    \033[1;32mINFO\033[0m";
        case G_LOG_LEVEL_DEBUG:    return "   \033[1;32mDEBUG\033[0m";
        case CC_LOG_LEVEL_TRACE:   return "   \033[1;36mTRACE\033[0m";
        default:                   return " UNKNOWN";
        }
    }
  else
    {
      switch ((int)log_level)
        {
        case G_LOG_LEVEL_ERROR:    return "   ERROR";
        case G_LOG_LEVEL_CRITICAL: return "CRITICAL";
        case G_LOG_LEVEL_WARNING:  return " WARNING";
        case G_LOG_LEVEL_MESSAGE:  return " MESSAGE";
        case G_LOG_LEVEL_INFO:     return "    INFO";
        case G_LOG_LEVEL_DEBUG:    return "   DEBUG";
        case CC_LOG_LEVEL_TRACE:   return "   TRACE";
        default:                   return " UNKNOWN";
        }
    }
}

static GLogWriterOutput
cc_log_write (GLogLevelFlags   log_level,
              const char      *log_domain,
              const char      *log_message,
              const GLogField *fields,
              gsize            n_fields,
              gpointer         user_data)
{
  g_autoptr(GString) log_str = NULL;
  FILE *stream = stdout;
  gboolean can_color;

  if (stderr_is_journal &&
      g_log_writer_journald (log_level, fields, n_fields, user_data) == G_LOG_WRITER_HANDLED)
    return G_LOG_WRITER_HANDLED;

  if (log_level & (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING))
    stream = stderr;

  log_str = g_string_new (NULL);

  /* Add local time */
  {
    char buffer[32];
    struct tm tm_now;
    time_t sec_now;
    gint64 now;

    now = g_get_real_time ();
    sec_now = now / G_USEC_PER_SEC;
    tm_now = *localtime (&sec_now);
    strftime (buffer, sizeof (buffer), "%H:%M:%S", &tm_now);

    g_string_append_printf (log_str, "%s.%04d ", buffer,
                            (int)((now % G_USEC_PER_SEC) / 100));
  }

  can_color = g_log_writer_supports_color (fileno (stream));
  log_str_append_log_domain (log_str, log_domain, can_color);
  g_string_append_printf (log_str, "[%5d]:", getpid ());

  g_string_append_printf (log_str, "%s: ", get_log_level_prefix (log_level, can_color));

  if (log_level & CC_LOG_DETAILED)
    {
      const char *code_func = NULL, *code_line = NULL;
      for (guint i = 0; i < n_fields; i++)
        {
          const GLogField *field = &fields[i];

          if (!code_func && g_strcmp0 (field->key, "CODE_FUNC") == 0)
            code_func = field->value;
          else if (!code_line && g_strcmp0 (field->key, "CODE_LINE") == 0)
            code_line = field->value;

          if (code_func && code_line)
            break;
        }

      if (code_func)
        {
          g_string_append_printf (log_str, "%s():", code_func);

          if (code_line)
            g_string_append_printf (log_str, "%s:", code_line);
          g_string_append_c (log_str, ' ');
        }
    }

  g_string_append (log_str, log_message);

  fprintf (stream, "%s\n", log_str->str);
  fflush (stream);

  if (fatal_criticals &&
      (log_level & G_LOG_LEVEL_CRITICAL))
    log_level |= G_LOG_FLAG_FATAL;
  else if (fatal_warnings &&
           (log_level & (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING)))
    log_level |= G_LOG_FLAG_FATAL;

  if (log_level & (G_LOG_FLAG_FATAL | G_LOG_LEVEL_ERROR))
    _g_log_abort (!(log_level & G_LOG_FLAG_RECURSION));

  return G_LOG_WRITER_HANDLED;
}

static GLogWriterOutput
cc_log_handler (GLogLevelFlags   log_level,
                const GLogField *fields,
                gsize            n_fields,
                gpointer         user_data)
{
  const char *log_domain = NULL;
  const char *log_message = NULL;

  for (guint i = 0; (!log_domain || !log_message) && i < n_fields; i++)
    {
      const GLogField *field = &fields[i];

      if (g_strcmp0 (field->key, "GLIB_DOMAIN") == 0)
        log_domain = field->value;
      else if (g_strcmp0 (field->key, "MESSAGE") == 0)
        log_message = field->value;
    }

  if (!log_domain)
    log_domain = "**";

  if (!log_message)
    log_message = "(NULL) message";

  if (!should_log (log_domain, log_level))
    return G_LOG_WRITER_HANDLED;

  return cc_log_write (log_level, log_domain, log_message,
                       fields, n_fields, user_data);
}

static void
cc_log_finalize (void)
{
  g_clear_pointer (&domains, g_free);
}

void
cc_log_init (void)
{
  static gsize initialized = 0;

  if (g_once_init_enter (&initialized))
    {
      domains = g_strdup (g_getenv ("G_MESSAGES_DEBUG"));

      if (domains && !*domains)
        g_clear_pointer (&domains, g_free);

      if (!domains || g_str_equal (domains, "all"))
        any_domain = TRUE;

      if (domains && strstr (domains, "no-anonymize"))
        {
          any_domain = TRUE;
          no_anonymize = TRUE;
          g_clear_pointer (&domains, g_free);
        }

      if (g_strcmp0 (g_getenv ("G_DEBUG"), "fatal-criticals") == 0)
        fatal_criticals = TRUE;
      else if (g_strcmp0 (g_getenv ("G_DEBUG"), "fatal-warnings") == 0)
        fatal_warnings = TRUE;

      stderr_is_journal = g_log_writer_is_journald (fileno (stderr));
      g_log_set_writer_func (cc_log_handler, NULL, NULL);
      g_once_init_leave (&initialized, 1);
      atexit (cc_log_finalize);
    }
}

void
cc_log_increase_verbosity (void)
{
  verbosity++;
}

int
cc_log_get_verbosity (void)
{
  return verbosity;
}

void
cc_log (const char     *domain,
        GLogLevelFlags  log_level,
        const char     *value,
        const char     *file,
        const char     *line,
        const char     *func,
        const char     *message_format,
        ...)
{
  g_autoptr(GString) str = NULL;
  va_list args;

  if (!message_format || !*message_format)
    return;

  if (!should_log (domain, log_level))
    return;

  str = g_string_new (NULL);
  va_start (args, message_format);
  g_string_append_vprintf (str, message_format, args);
  va_end (args);

  cc_log_anonymize_value (str, value);
  g_log_structured (domain, log_level,
                    "CODE_FILE", file,
                    "CODE_LINE", line,
                    "CODE_FUNC", func,
                    "MESSAGE", "%s", str->str);
}

void
cc_log_anonymize_value (GString    *str,
                        const char *value)
{
  gunichar c, next_c, prev_c;

  if (!value || !*value)
    return;

  g_assert (str);

  if (str->len && str->str[str->len - 1] != ' ')
    g_string_append_c (str, ' ');

  if (no_anonymize)
    {
      g_string_append (str, value);
      return;
    }

  if (!g_utf8_validate (value, -1, NULL))
    {
      g_string_append (str, "******");
      return;
    }

  c = g_utf8_get_char (value);
  g_string_append_unichar (str, c);

  value = g_utf8_next_char (value);

  while (*value)
    {
      prev_c = c;
      c = g_utf8_get_char (value);

      value = g_utf8_next_char (value);
      next_c = g_utf8_get_char (value);

      if (!g_unichar_isalnum (c))
        g_string_append_unichar (str, c);
      else if (!g_unichar_isalnum (prev_c) || !g_unichar_isalnum (next_c))
        g_string_append_unichar (str, c);
      else
        g_string_append_c (str, '#');
    }
}
