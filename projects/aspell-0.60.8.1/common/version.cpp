#include "settings.h"

#ifdef NDEBUG
#  define NDEBUG_STR " NDEBUG"
#else
#  define NDEBUG_STR
#endif

#ifdef SLOPPY_NULL_TERM_STRINGS
#  define SLOPPY_STR " SLOPPY"
#else
#  define SLOPPY_STR
#endif

extern "C" const char * aspell_version_string() {
  return VERSION NDEBUG_STR SLOPPY_STR;
}
