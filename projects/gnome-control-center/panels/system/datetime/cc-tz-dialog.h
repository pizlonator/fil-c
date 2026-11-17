#pragma once

#include <adwaita.h>

#include "cc-tz-item.h"

G_BEGIN_DECLS

#define CC_TYPE_TZ_DIALOG (cc_tz_dialog_get_type ())

G_DECLARE_FINAL_TYPE (CcTzDialog, cc_tz_dialog, CC, TZ_DIALOG, AdwWindow)

GtkWidget   *cc_tz_dialog_new                   (void);
gboolean     cc_tz_dialog_set_tz                (CcTzDialog *self,
                                                 const char *timezone);
CcTzItem    *cc_tz_dialog_get_selected_tz       (CcTzDialog *self);
TzLocation  *cc_tz_dialog_get_selected_location (CcTzDialog *self);

G_END_DECLS
