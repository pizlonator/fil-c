#pragma once

#include <gtk.h>

#define IB_TYPE_ICON (ib_icon_get_type ())
G_DECLARE_FINAL_TYPE (IbIcon, ib_icon, IB, ICON, GObject)

IbIcon *ib_icon_new                     (const char     *regular_name,
                                         const char     *symbolic_name,
                                         const char     *description,
                                         const char     *context);

const char *ib_icon_get_name            (IbIcon         *icon);
const char *ib_icon_get_regular_name    (IbIcon         *icon);
const char *ib_icon_get_symbolic_name   (IbIcon         *icon);
gboolean    ib_icon_get_use_symbolic    (IbIcon         *icon);
const char *ib_icon_get_description     (IbIcon         *icon);
const char *ib_icon_get_context         (IbIcon         *icon);
