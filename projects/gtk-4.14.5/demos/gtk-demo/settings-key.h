#pragma once

#include <gtk/gtk.h>

#include <stdlib.h>

/* Create an object that wraps GSettingsSchemaKey because that's a boxed type */
typedef struct _SettingsKey SettingsKey;
#define SETTINGS_TYPE_KEY (settings_key_get_type ())
G_DECLARE_FINAL_TYPE (SettingsKey, settings_key, SETTINGS, KEY, GObject);

SettingsKey *           settings_key_new                        (GSettings              *settings,
                                                                 GSettingsSchemaKey     *key);

GSettingsSchemaKey *    settings_key_get_key                    (SettingsKey            *self);
GSettings *             settings_key_get_settings               (SettingsKey            *self);
char *                  settings_key_get_search_string          (SettingsKey            *self);
