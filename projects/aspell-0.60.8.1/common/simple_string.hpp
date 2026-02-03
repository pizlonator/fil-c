// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_SIMPLE_STRING__HPP
#define ASPELL_SIMPLE_STRING__HPP

#include <string.h>

#include "parm_string.hpp"

namespace acommon {

  struct SimpleString {
    const char * str;
    unsigned int size;

    SimpleString() : str(0), size(0) {}
    SimpleString(const char * str0) : str(str0), size(strlen(str)) {}
    SimpleString(const char * str0, unsigned int sz) : str(str0), size(sz) {}
    SimpleString(ParmString str0) : str(str0), size(str0.size()) {}

    bool empty() const {return size == 0;}
    operator const char * () const {return str;}
    operator ParmString () const {return ParmString(str, size);}
    const char * begin() const {return str;}
    const char * end() const {return str + size;}
  };

  static inline bool operator==(SimpleString s1, SimpleString s2)
  {
    if (s1.size != s2.size)
      return false;
    else
      return memcmp(s1,s2,s1.size) == 0;
  }
  static inline bool operator==(const char * s1, SimpleString s2)
  {
    return strcmp(s1,s2) == 0;
  }
  static inline bool operator==(SimpleString s1, const char * s2)
  {
    return strcmp(s1,s2) == 0;
  }

  static inline bool operator!=(SimpleString s1, SimpleString s2)
  {
    if (s1.size != s2.size)
      return true;
    else
      return memcmp(s1,s2,s1.size) != 0;
  }
  static inline bool operator!=(const char * s1, SimpleString s2)
  {
    return strcmp(s1,s2) != 0;
  }
  static inline bool operator!=(SimpleString s1, const char * s2)
  {
    return strcmp(s1,s2) != 0;
  }
}

#endif
