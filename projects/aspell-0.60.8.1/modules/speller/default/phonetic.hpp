// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef __aspeller_phonetic__
#define __aspeller_phonetic__

#include "string.hpp"

using namespace acommon;

namespace acommon {struct Conv;}

namespace aspeller {

  class Language;

  class Soundslike {
  public:
    virtual String soundslike_chars() const = 0;
    // string must be null terminated even if len is given
    virtual char * to_soundslike(char *, const char *, int len = -1) const = 0;
    virtual const char * name() const = 0;
    virtual const char * version() const = 0;
    virtual PosibErr<void> setup(Conv &) = 0;
    virtual ~Soundslike() {}
  };

  PosibErr<Soundslike *> new_soundslike(ParmString name,
                                        Conv & conv,
                                        const Language * lang);
};

#endif
