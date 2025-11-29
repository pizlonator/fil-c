#pragma once

#include <pango/pango.h>

G_BEGIN_DECLS

const char * get_language_name (PangoLanguage *language);
const char * get_language_name_for_tag (guint32 tag);

G_END_DECLS

