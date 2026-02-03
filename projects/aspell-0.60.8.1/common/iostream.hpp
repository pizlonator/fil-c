// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_IOSTREAM__HPP
#define ASPELL_IOSTREAM__HPP

#include "fstream.hpp"

namespace acommon {

  // These streams for the time being will be based on stdin, stdout,
  // and stderr respectfully.  So it is safe to use the standard C
  // functions.  It is also safe to assume that modifications to the
  // state of the standard streams will effect these.

  extern FStream CIN;
  extern FStream COUT;
  extern FStream CERR;
}

#endif
