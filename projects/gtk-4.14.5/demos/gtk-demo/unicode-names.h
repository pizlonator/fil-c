#pragma once

#include <glib.h>

const char * get_unicode_type_name    (GUnicodeType      type);
const char * get_break_type_name      (GUnicodeBreakType type);
const char * get_combining_class_name (int               cclass);
