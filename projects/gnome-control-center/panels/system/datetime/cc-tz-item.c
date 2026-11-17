/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright 2022 Purism SPC
 * Copyright 2022 Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-tz-item"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>
#include <libgnome-desktop/gnome-wall-clock.h>

#include "cc-tz-item.h"

#define DEFAULT_TZ "Europe/London"
#define GETTEXT_PACKAGE_TIMEZONES GETTEXT_PACKAGE "-timezones"

struct _CcTzItem
{
  GObject         parent_instance;

  GSettings      *desktop_settings;
  GTimeZone      *tz;
  GnomeWallClock *wall_clock;

  TzLocation     *tz_location;
  TzInfo         *tz_info;

  char           *name;
  char           *country;
  char           *time;
  char           *offset;    /* eg: UTC+530 */
  char           *zone;
};

G_DEFINE_TYPE (CcTzItem, cc_tz_item, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_COUNTRY,
  PROP_NAME,
  PROP_OFFSET,
  PROP_TIME,
  PROP_ZONE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/* Adapted from cc-datetime-panel.c */
static void
generate_city_name (CcTzItem   *self,
                    TzLocation *loc)
{
  g_auto(GStrv) split_translated = NULL;
  gint length;

  /* Load the translation for it */
  self->zone = g_strdup (dgettext (GETTEXT_PACKAGE_TIMEZONES, loc->zone));
  g_strdelimit (self->zone, "_", ' ');
  split_translated = g_regex_split_simple ("[\\x{2044}\\x{2215}\\x{29f8}\\x{ff0f}/]",
                                           self->zone,
                                           0, 0);

  length = g_strv_length (split_translated);
  self->country = gnome_get_country_from_code (loc->country, NULL);
  self->name = g_strdup (split_translated[length-1]);
}

static const char *
tz_item_get_time (CcTzItem *self)
{
  g_autoptr(GDateTime) now = NULL;
  GDesktopClockFormat format;

  g_assert (CC_IS_TZ_ITEM (self));

  if (self->time)
    return self->time;

  now = g_date_time_new_now (self->tz);
  format = g_settings_get_enum (self->desktop_settings, "clock-format");

  self->time = gnome_wall_clock_string_for_datetime (self->wall_clock, now, format, TRUE, FALSE, FALSE);

  return self->time;
}

static void
tz_item_clock_changed_cb (CcTzItem *self)
{
  gboolean had_time;

  g_assert (CC_IS_TZ_ITEM (self));

  had_time = !!self->time;

  /* Clear the time, so that it'll be re-created when asked for one */
  g_clear_pointer (&self->time, g_free);

  if (had_time)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIME]);
}

static void
cc_tz_item_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  CcTzItem *self = (CcTzItem *)object;

  switch (prop_id)
    {
    case PROP_COUNTRY:
      g_value_set_string (value, self->country);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_OFFSET:
      g_value_set_string (value, self->offset);
      break;

    case PROP_TIME:
      g_value_set_string (value, tz_item_get_time (self));
      break;

    case PROP_ZONE:
      g_value_set_string (value, self->zone);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_tz_item_finalize (GObject *object)
{
  CcTzItem *self = (CcTzItem *)object;

  g_clear_object (&self->desktop_settings);
  g_clear_object (&self->wall_clock);

  g_clear_pointer (&self->tz, g_time_zone_unref);
  g_clear_pointer (&self->tz_info, tz_info_free);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->country, g_free);
  g_clear_pointer (&self->time, g_free);
  g_clear_pointer (&self->offset, g_free);
  g_clear_pointer (&self->zone, g_free);

  G_OBJECT_CLASS (cc_tz_item_parent_class)->finalize (object);
}

static void
cc_tz_item_class_init (CcTzItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_tz_item_get_property;
  object_class->finalize = cc_tz_item_finalize;

  properties[PROP_COUNTRY] =
    g_param_spec_string ("country",
                         "Timezone Country",
                         "Timezone Country",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "Timezone Name",
                         "Timezone Name",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_OFFSET] =
    g_param_spec_string ("offset",
                         "Timezone offset",
                         "Timezone offset",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_TIME] =
    g_param_spec_string ("time",
                         "Timezone time",
                         "Timezone time",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ZONE] =
    g_param_spec_string ("zone",
                         "Timezone zone",
                         "Timezone zone",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
cc_tz_item_init (CcTzItem *self)
{
  self->desktop_settings = g_settings_new ("org.gnome.desktop.interface");
  self->wall_clock = gnome_wall_clock_new ();

  g_signal_connect_object (self->wall_clock, "notify::clock",
                           G_CALLBACK (tz_item_clock_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->desktop_settings, "changed::clock-format",
                           G_CALLBACK (tz_item_clock_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

CcTzItem *
cc_tz_item_new (TzLocation *location)
{
  CcTzItem *self;
  GString *offset;

  g_return_val_if_fail (location, NULL);

  self = g_object_new (CC_TYPE_TZ_ITEM, NULL);
  self->tz_location = location;
  self->tz_info = tz_info_from_location (location);
  generate_city_name (self, location);

  self->tz = g_time_zone_new_offset (self->tz_info->utc_offset);

  offset = g_string_new (g_time_zone_get_identifier (self->tz));
  /* Strip the seconds, eg: +05:30:00 -> +05:30 */
  g_string_set_size (offset, offset->len - 3);
  /* eg: +05:30 -> +0530*/
  g_string_replace (offset, ":", "", 0);

  /* If the timezone is UTC remove the time, which will always be [+]0000 */
  if (g_str_has_suffix (offset->str, "0000"))
    g_string_set_size (offset, 0);

  /* eg: +0530 -> UTC+0530 */
  g_string_prepend (offset, "UTC");

  self->offset = g_string_free (offset, FALSE);

  return self;
}

TzLocation *
cc_tz_item_get_location (CcTzItem *self)
{
  g_return_val_if_fail (CC_IS_TZ_ITEM (self), NULL);

  return self->tz_location;
}
