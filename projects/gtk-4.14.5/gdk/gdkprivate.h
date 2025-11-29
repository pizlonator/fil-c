#pragma once

#include "gdk/gdktypes.h"

/* Private API for use in GTK+ */

void            gdk_pre_parse                   (void);

gboolean gdk_running_in_sandbox (void);
gboolean gdk_should_use_portal (void);

const char *   gdk_get_startup_notification_id (void);

PangoDirection gdk_unichar_direction (gunichar    ch) G_GNUC_CONST;
PangoDirection gdk_find_base_dir     (const char *text,
                                      int         len);

void gdk_source_set_static_name_by_id (guint       tag,
                                       const char *name);

#ifndef I_
#define I_(string) g_intern_static_string (string)
#endif
