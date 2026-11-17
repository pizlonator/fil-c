/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Generic timezone utilities.
 *
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Authors: Hans Petter Jansson <hpj@ximian.com>
 *
 * Largely based on Michael Fulbright's work on Anaconda.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */


#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "tz.h"
#include "cc-system-resources.h"


/* Forward declarations for private functions */

static float convert_pos (gchar *pos, int digits);
static int compare_country_names (const void *a, const void *b);
static void sort_locations_by_country (GPtrArray *locations);
static gchar * tz_data_file_get (void);
static void load_backward_tz (TzDB *tz_db);

/* ---------------- *
 * Public interface *
 * ---------------- */
TzDB *
tz_load_db (void)
{
	g_autofree gchar *tz_data_file = NULL;
	TzDB *tz_db;
	FILE *tzfile;
	char buf[4096];

	tz_data_file = tz_data_file_get ();
	if (!tz_data_file) {
		g_warning ("Could not get the TimeZone data file name");
		return NULL;
	}
	tzfile = fopen (tz_data_file, "r");
	if (!tzfile) {
		g_warning ("Could not open *%s*\n", tz_data_file);
		return NULL;
	}

	tz_db = g_new0 (TzDB, 1);
	tz_db->locations = g_ptr_array_new ();

	while (fgets (buf, sizeof(buf), tzfile))
	{
		g_auto(GStrv) tmpstrarr = NULL;
		g_autofree gchar *latstr = NULL;
		g_autofree gchar *lngstr = NULL;
		gchar *p;
		TzLocation *loc;

		if (*buf == '#') continue;

		g_strchomp(buf);
		tmpstrarr = g_strsplit(buf,"\t", 6);

		latstr = g_strdup (tmpstrarr[1]);
		p = latstr + 1;
		while (*p != '-' && *p != '+') p++;
		lngstr = g_strdup (p);
		*p = '\0';

		loc = g_new0 (TzLocation, 1);
		loc->country = g_strdup (tmpstrarr[0]);
		loc->zone = g_strdup (tmpstrarr[2]);
		loc->latitude  = convert_pos (latstr, 2);
		loc->longitude = convert_pos (lngstr, 3);

#ifdef __sun
		if (tmpstrarr[3] && *tmpstrarr[3] == '-' && tmpstrarr[4])
			loc->comment = g_strdup (tmpstrarr[4]);

		if (tmpstrarr[3] && *tmpstrarr[3] != '-' && !islower(loc->zone)) {
			TzLocation *locgrp;

			/* duplicate entry */
			locgrp = g_new0 (TzLocation, 1);
			locgrp->country = g_strdup (tmpstrarr[0]);
			locgrp->zone = g_strdup (tmpstrarr[3]);
			locgrp->latitude  = convert_pos (latstr, 2);
			locgrp->longitude = convert_pos (lngstr, 3);
			locgrp->comment = (tmpstrarr[4]) ? g_strdup (tmpstrarr[4]) : NULL;

			g_ptr_array_add (tz_db->locations, (gpointer) locgrp);
		}
#else
		loc->comment = (tmpstrarr[3]) ? g_strdup(tmpstrarr[3]) : NULL;
#endif

		g_ptr_array_add (tz_db->locations, (gpointer) loc);
	}

	fclose (tzfile);

	/* now sort by country */
	sort_locations_by_country (tz_db->locations);

	/* Load up the hashtable of backward links */
	load_backward_tz (tz_db);

	return tz_db;
}

static void
tz_location_free (TzLocation *loc)
{
	g_free (loc->country);
	g_free (loc->zone);
	g_free (loc->comment);

	g_free (loc);
}

void
tz_db_free (TzDB *db)
{
	g_ptr_array_foreach (db->locations, (GFunc) tz_location_free, NULL);
	g_ptr_array_free (db->locations, TRUE);
	g_hash_table_destroy (db->backward);
	g_free (db);
}

GPtrArray *
tz_get_locations (TzDB *db)
{
	return db->locations;
}


gchar *
tz_location_get_country (TzLocation *loc)
{
	return loc->country;
}


gchar *
tz_location_get_zone (TzLocation *loc)
{
	return loc->zone;
}


gchar *
tz_location_get_comment (TzLocation *loc)
{
	return loc->comment;
}


void
tz_location_get_position (TzLocation *loc, double *longitude, double *latitude)
{
	*longitude = loc->longitude;
	*latitude = loc->latitude;
}

/* For timezone map display purposes, we try to highlight regions of the
 * world that keep the same time.  There is no reasonable API to discover
 * this; at the moment we just group timezones by their non-daylight-savings
 * UTC offset and hope that's good enough.  However, in some cases that
 * produces confusing results.  For example, Irish Standard Time is legally
 * defined as the country's summer time, with a negative DST offset in
 * winter; but this results in the same observed clock times as countries
 * that observe Western European (Summer) Time, not those that observe
 * Central European (Summer) Time, so we should group Ireland with the
 * former, matching the grouping implied by data/timezone_*.png.
 *
 * This is something of a hack, and there remain other problems with
 * timezone grouping: for example, grouping timezones north and south of the
 * equator together where DST is observed at different times of the year is
 * dubious.
 */
struct {
	const char *zone;
	gint offset;
} base_offset_overrides[] = {
	{ "Europe/Dublin",  0 },
};

glong
tz_location_get_base_utc_offset (TzLocation *loc)
{
	g_autoptr(TzInfo) tz_info = NULL;
	glong offset;
	guint i;

	tz_info = tz_info_from_location (loc);
	offset = tz_info->utc_offset + (tz_info->daylight ? -3600 : 0);

	for (i = 0; i < G_N_ELEMENTS (base_offset_overrides); i++) {
		if (g_str_equal (loc->zone, base_offset_overrides[i].zone)) {
			offset = base_offset_overrides[i].offset;
			break;
		}
	}

	return offset;
}

TzInfo *
tz_info_from_location (TzLocation *loc)
{
	TzInfo *tzinfo;
	time_t curtime;
	struct tm *curzone;
	g_autofree gchar *tz_env_value = NULL;

	g_return_val_if_fail (loc != NULL, NULL);
	g_return_val_if_fail (loc->zone != NULL, NULL);

	tz_env_value = g_strdup (getenv ("TZ"));
	setenv ("TZ", loc->zone, 1);

#if 0
	tzset ();
#endif
	tzinfo = g_new0 (TzInfo, 1);

	curtime = time (NULL);
	curzone = localtime (&curtime);

#ifndef __sun
	tzinfo->tzname = g_strdup (curzone->tm_zone);
	tzinfo->utc_offset = curzone->tm_gmtoff;
#else
	tzinfo->tzname = NULL;
	tzinfo->utc_offset = 0;
#endif

	tzinfo->daylight = curzone->tm_isdst;

	if (tz_env_value)
		setenv ("TZ", tz_env_value, 1);
	else
		unsetenv ("TZ");

	return tzinfo;
}


void
tz_info_free (TzInfo *tzinfo)
{
	g_return_if_fail (tzinfo != NULL);

	if (tzinfo->tzname) g_free (tzinfo->tzname);
	g_free (tzinfo);
}

struct {
	const char *orig;
	const char *dest;
} aliases[] = {
	{ "Asia/Istanbul",  "Europe/Istanbul" },	/* Istanbul is in both Europe and Asia */
	{ "Europe/Nicosia", "Asia/Nicosia" },		/* Ditto */
	{ "EET",            "Europe/Istanbul" },	/* Same tz as the 2 above */
	{ "HST",            "Pacific/Honolulu" },
	{ "WET",            "Europe/Brussels" },	/* Other name for the mainland Europe tz */
	{ "CET",            "Europe/Brussels" },	/* ditto */
	{ "MET",            "Europe/Brussels" },
	{ "Etc/Zulu",       "Etc/GMT" },
	{ "Etc/UTC",        "Etc/GMT" },
	{ "GMT",            "Etc/GMT" },
	{ "Greenwich",      "Etc/GMT" },
	{ "Etc/UCT",        "Etc/GMT" },
	{ "Etc/GMT0",       "Etc/GMT" },
	{ "Etc/GMT+0",      "Etc/GMT" },
	{ "Etc/GMT-0",      "Etc/GMT" },
	{ "Etc/Universal",  "Etc/GMT" },
	{ "PST8PDT",        "America/Los_Angeles" },	/* Other name for the Atlantic tz */
	{ "EST",            "America/New_York" },	/* Other name for the Eastern tz */
	{ "EST5EDT",        "America/New_York" },	/* ditto */
	{ "CST6CDT",        "America/Chicago" },	/* Other name for the Central tz */
	{ "MST",            "America/Denver" },		/* Other name for the mountain tz */
	{ "MST7MDT",        "America/Denver" },		/* ditto */
};

static gboolean
compare_timezones (const char *a,
		   const char *b)
{
	if (g_str_equal (a, b))
		return TRUE;
	if (strchr (b, '/') == NULL) {
		g_autofree gchar *prefixed = NULL;

		prefixed = g_strdup_printf ("/%s", b);
		if (g_str_has_suffix (a, prefixed))
			return TRUE;
	}

	return FALSE;
}

char *
tz_info_get_clean_name (TzDB *tz_db,
			const char *tz)
{
	char *ret;
	const char *timezone;
	guint i;
	gboolean replaced;

	/* Remove useless prefixes */
	if (g_str_has_prefix (tz, "right/"))
		tz = tz + strlen ("right/");
	else if (g_str_has_prefix (tz, "posix/"))
		tz = tz + strlen ("posix/");

	/* Here start the crazies */
	replaced = FALSE;

	for (i = 0; i < G_N_ELEMENTS (aliases); i++) {
		if (compare_timezones (tz, aliases[i].orig)) {
			replaced = TRUE;
			timezone = aliases[i].dest;
			break;
		}
	}

	/* Try again! */
	if (!replaced) {
		/* Ignore crazy solar times from the '80s */
		if (g_str_has_prefix (tz, "Asia/Riyadh") ||
		    g_str_has_prefix (tz, "Mideast/Riyadh")) {
			timezone = "Asia/Riyadh";
			replaced = TRUE;
		}
	}

	if (!replaced)
		timezone = tz;

	ret = g_hash_table_lookup (tz_db->backward, timezone);
	if (ret == NULL)
		return g_strdup (timezone);
	return g_strdup (ret);
}

/* ----------------- *
 * Private functions *
 * ----------------- */

static gchar *
tz_data_file_get (void)
{
	gchar *file;

	file = g_strdup (TZ_DATA_FILE);

	return file;
}

static float
convert_pos (gchar *pos, int digits)
{
	gchar whole[10];
	gchar *fraction;
	gint i;
	float t1, t2;

	if (!pos || strlen(pos) < 4 || digits > 9) return 0.0;

	for (i = 0; i < digits + 1; i++) whole[i] = pos[i];
	whole[i] = '\0';
	fraction = pos + digits + 1;

	t1 = g_strtod (whole, NULL);
	t2 = g_strtod (fraction, NULL);

	if (t1 >= 0.0) return t1 + t2/pow (10.0, strlen(fraction));
	else return t1 - t2/pow (10.0, strlen(fraction));
}


#if 0

/* Currently not working */
static void
free_tzdata (TzLocation *tz)
{

	if (tz->country)
	  g_free(tz->country);
	if (tz->zone)
	  g_free(tz->zone);
	if (tz->comment)
	  g_free(tz->comment);

	g_free(tz);
}
#endif


static int
compare_country_names (const void *a, const void *b)
{
	const TzLocation *tza = * (TzLocation **) a;
	const TzLocation *tzb = * (TzLocation **) b;

	return strcmp (tza->zone, tzb->zone);
}


static void
sort_locations_by_country (GPtrArray *locations)
{
	qsort (locations->pdata, locations->len, sizeof (gpointer),
	       compare_country_names);
}

static void
load_backward_tz (TzDB *tz_db)
{
  g_auto(GStrv) lines = NULL;
  g_autoptr(GBytes) bytes = NULL;
  const char *contents;
  guint i;

  tz_db->backward = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  bytes = g_resources_lookup_data ("/org/gnome/control-center/system/datetime/backward",
                                   G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  contents = (const char *) g_bytes_get_data (bytes, NULL);

  lines = g_strsplit (contents, "\n", -1);

  for (i = 0; lines[i] != NULL; i++)
    {
      g_auto(GStrv) items = NULL;
      guint j;
      char *real, *alias;

      if (g_ascii_strncasecmp (lines[i], "Link\t", 5) != 0)
        continue;

      items = g_strsplit (lines[i], "\t", -1);
      real = NULL;
      alias = NULL;
      /* Skip the "Link<tab>" part */
      for (j = 1; items[j] != NULL; j++)
        {
          if (items[j][0] == '\0')
            continue;
          if (real == NULL)
            {
              real = items[j];
              continue;
            }
          alias = items[j];
          break;
        }

      if (real == NULL || alias == NULL)
        g_warning ("Could not parse line: %s", lines[i]);

      /* We don't need more than one name for it */
      if (g_str_equal (real, "Etc/UTC") ||
          g_str_equal (real, "Etc/UCT"))
        real = "Etc/GMT";

      g_hash_table_insert (tz_db->backward, g_strdup (alias), g_strdup (real));
    }
}

