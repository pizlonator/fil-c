// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "iostream.hpp"

namespace acommon {
  FStream CIN(stdin, false);
  FStream COUT(stdout, false);
  FStream CERR(stderr, false);
}
