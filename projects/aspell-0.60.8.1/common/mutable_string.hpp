// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_MUTABLE_STRING__HPP
#define ASPELL_MUTABLE_STRING__HPP

#include <string.h>

#include "parm_string.hpp"

namespace acommon {

  struct MutableString {
    char * str;
    unsigned int size;

    MutableString() : str(0), size(0) {}
    MutableString(char * str0) : str(str0), size(strlen(str)) {}
    MutableString(char * str0, unsigned int sz) : str(str0), size(sz) {}

    bool empty() const {return size == 0;}
    operator char * () const {return str;}
    operator ParmString () const {return ParmString(str, size);}
    char * begin() const {return str;}
    char * end() const {return str + size;}
  };

  static inline bool operator==(MutableString s1, MutableString s2)
  {
    if (s1.size != s2.size)
      return false;
    else
      return memcmp(s1,s2,s1.size) == 0;
  }
  static inline bool operator==(const char * s1, MutableString s2)
  {
    if ( s1 == NULL ) {
      return s2.size == 0;
    }
    return strcmp(s1,s2) == 0;
  }
  static inline bool operator==(MutableString s1, const char * s2)
  {
    if ( s2 == NULL ) {
      return s1.size == 0;
    }
    return strcmp(s1,s2) == 0;
  }

  static inline bool operator!=(MutableString s1, MutableString s2)
  {
    if (s1.size != s2.size)
      return true;
    else
      return memcmp(s1,s2,s1.size) != 0;
  }
  static inline bool operator!=(const char * s1, MutableString s2)
  {
    if ( s1 == NULL ) {
      return s2.size != 0;
    }
    return strcmp(s1,s2) != 0;
  }
  static inline bool operator!=(MutableString s1, const char * s2)
  {
    if ( s2 == NULL ) {
      return s1.size != 0;
    }
    return strcmp(s1,s2) != 0;
  }
}

#endif
