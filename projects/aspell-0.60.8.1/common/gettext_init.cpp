
#include "gettext.h"

#if ENABLE_NLS

#include "lock.hpp"

static acommon::Mutex lock;

static bool did_init = false;

extern "C" void aspell_gettext_init()
{
  {
    acommon::Lock l(&lock);
    if (did_init) return;
    did_init = true;
  }
  bindtextdomain("aspell", LOCALEDIR);
}

#else

extern "C" void aspell_gettext_init()
{
}

#endif
