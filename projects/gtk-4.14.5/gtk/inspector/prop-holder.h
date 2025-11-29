#pragma once

#include <gtk/gtk.h>

#define PROP_TYPE_HOLDER (prop_holder_get_type ())

G_DECLARE_FINAL_TYPE (PropHolder, prop_holder, PROP, HOLDER, GObject)

PropHolder * prop_holder_new         (GObject    *object,
                                      GParamSpec *pspeC);

GObject    *prop_holder_get_object   (PropHolder *holder);
GParamSpec *prop_holder_get_pspec    (PropHolder *holder);
const char *prop_holder_get_name     (PropHolder *holder);

