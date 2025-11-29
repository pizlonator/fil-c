
#pragma once

#include <gtk/gtk.h>

#define ACTION_TYPE_HOLDER (action_holder_get_type ())

G_DECLARE_FINAL_TYPE (ActionHolder, action_holder, ACTION, HOLDER, GObject)

ActionHolder * action_holder_new     (GObject    *owner,
                                      const char *name);

GObject      *action_holder_get_owner (ActionHolder *holder);
const char   *action_holder_get_name  (ActionHolder *holder);
void          action_holder_changed   (ActionHolder *holder);
