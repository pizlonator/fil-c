// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include <string.h>

#include "config.hpp"
#include "errors.hpp"
#include "filter.hpp"

namespace acommon {
  
  extern void setup_static_filters(Config * config);

  Config * new_config() 
  {
    Config * config = new_basic_config();
    setup_static_filters(config);
    return config;
  }

}
