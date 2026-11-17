/*
 * Copyright (c) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include <string.h>
#include <glib/gi18n.h>


#include "cc-util.h"

/* Combining diacritical mark?
 *  Basic range: [0x0300,0x036F]
 *  Supplement:  [0x1DC0,0x1DFF]
 *  For Symbols: [0x20D0,0x20FF]
 *  Half marks:  [0xFE20,0xFE2F]
 */
#define IS_CDM_UCS4(c) (((c) >= 0x0300 && (c) <= 0x036F)  || \
                        ((c) >= 0x1DC0 && (c) <= 0x1DFF)  || \
                        ((c) >= 0x20D0 && (c) <= 0x20FF)  || \
                        ((c) >= 0xFE20 && (c) <= 0xFE2F))

#define IS_SOFT_HYPHEN(c) ((c) == 0x00AD)

/* Copied from tracker/src/libtracker-fts/tracker-parser-glib.c under the GPL
 * And then from gnome-shell/src/shell-util.c
 *
 * Originally written by Aleksander Morgado <aleksander@gnu.org>
 */
char *
cc_util_normalize_casefold_and_unaccent (const char *str)
{
  g_autofree gchar *normalized = NULL;
  gchar *tmp;
  int i = 0, j = 0, ilen;

  if (str == NULL)
    return NULL;

  normalized = g_utf8_normalize (str, -1, G_NORMALIZE_NFKD);
  tmp = g_utf8_casefold (normalized, -1);

  ilen = strlen (tmp);

  while (i < ilen)
    {
      gunichar unichar;
      gchar *next_utf8;
      gint utf8_len;

      /* Get next character of the word as UCS4 */
      unichar = g_utf8_get_char_validated (&tmp[i], -1);

      /* Invalid UTF-8 character or end of original string. */
      if (unichar == (gunichar) -1 ||
          unichar == (gunichar) -2)
        {
          break;
        }

      /* Find next UTF-8 character */
      next_utf8 = g_utf8_next_char (&tmp[i]);
      utf8_len = next_utf8 - &tmp[i];

      if (IS_CDM_UCS4 (unichar) || IS_SOFT_HYPHEN (unichar))
        {
          /* If the given unichar is a combining diacritical mark,
           * just update the original index, not the output one */
          i += utf8_len;
          continue;
        }

      /* If already found a previous combining
       * diacritical mark, indexes are different so
       * need to copy characters. As output and input
       * buffers may overlap, need to use memmove
       * instead of memcpy */
      if (i != j)
        {
          memmove (&tmp[j], &tmp[i], utf8_len);
        }

      /* Update both indexes */
      i += utf8_len;
      j += utf8_len;
    }

  /* Force proper string end */
  tmp[j] = '\0';

  return tmp;
}

char *
cc_util_get_smart_date (GDateTime *date)
{
        g_autoptr(GDateTime) today = NULL;
        g_autoptr(GDateTime) local = NULL;
        GTimeSpan span;

        if (date == NULL)
          return NULL;

        /* Set today date */
        local = g_date_time_new_now_local ();
        today = g_date_time_new_local (g_date_time_get_year (local),
                                       g_date_time_get_month (local),
                                       g_date_time_get_day_of_month (local),
                                       0, 0, 0);

        span = g_date_time_difference (today, date);
        if (span <= 0)
          {
            return g_strdup (_("Today"));
          }
        else if (span <= G_TIME_SPAN_DAY)
          {
            return g_strdup (_("Yesterday"));
          }
        else
          {
            if (g_date_time_get_year (date) == g_date_time_get_year (today))
              {
                /* Translators: This is a date format string in the style of "Feb 24". */
                /* xgettext:no-c-format */
                return g_date_time_format (date, _("%b %e"));
              }
            else
              {
                /* Translators: This is a date format string in the style of "Feb 24, 2013". */
                return g_date_time_format (date, _("%b %e, %Y"));
              }
          }
}

char *
cc_util_get_smart_date_time (GDateTime *date)
{
  g_autofree gchar *date_str = NULL;
  g_autofree gchar *smart_date = NULL;

  if (date == NULL)
    return NULL;

  smart_date = cc_util_get_smart_date (date);
  date_str = g_date_time_format (date, "\%X");
  /* TRANSLATORS: This is the datetime format in the style of
     "Aug 1, 10:10:10 PM", "Feb 24, 2013, 10:10:10 PM", "Today, 10:10:10 AM",
     and "Yesterday, 10:10:10 AM" */
  return g_strdup_printf (_("%1$s, %2$s"), smart_date, date_str);
}

/* Copied from src/plugins/properties/bacon-video-widget-properties.c
 * in totem */
char *
cc_util_time_to_string_text (gint64 msecs)
{
  g_autofree gchar *hours = NULL;
  g_autofree gchar *mins = NULL;
  g_autofree gchar *secs = NULL;
  gint sec, min, hour, _time;

  _time = (int) (msecs / 1000);
  sec = _time % 60;
  _time = _time - sec;
  min = (_time % (60*60)) / 60;
  _time = _time - (min * 60);
  hour = _time / (60*60);

  hours = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d hour", "%d hours", hour), hour);
  mins = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d minute", "%d minutes", min), min);
  secs = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d second", "%d seconds", sec), sec);

  if (hour > 0)
    {
      if (min > 0 && sec > 0)
        {
          /* 5 hours 2 minutes 12 seconds */
          return g_strdup_printf (C_("hours minutes seconds", "%s %s %s"), hours, mins, secs);
        }
      else if (min > 0)
        {
          /* 5 hours 2 minutes */
          return g_strdup_printf (C_("hours minutes", "%s %s"), hours, mins);
        }
      else
        {
          /* 5 hours */
          return g_strdup_printf (C_("hours", "%s"), hours);
        }
    }
  else if (min > 0)
    {
      if (sec > 0)
        {
          /* 2 minutes 12 seconds */
          return g_strdup_printf (C_("minutes seconds", "%s %s"), mins, secs);
        }
      else
        {
          /* 2 minutes */
          return g_strdup_printf (C_("minutes", "%s"), mins);
        }
    }
  else if (sec > 0)
    {
      /* 10 seconds */
      return g_strdup (secs);
    }
  else
    {
      /* 0 seconds */
      return g_strdup (_("0 seconds"));
    }
}
