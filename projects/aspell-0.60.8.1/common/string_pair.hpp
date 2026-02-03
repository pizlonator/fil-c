/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_STRING_PAIR__HPP
#define ASPELL_STRING_PAIR__HPP

namespace acommon {

struct StringPair {
  const char * first;
  const char * second;
  StringPair(const char * f, const char * s)
    : first(f), second(s) {}
  StringPair() : first(""), second("") {}
};

}

#endif /* ASPELL_STRING_PAIR__HPP */
