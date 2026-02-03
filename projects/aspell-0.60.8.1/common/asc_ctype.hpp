// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASC_CTYPE__HPP
#define ASC_CTYPE__HPP

namespace acommon {

  static inline bool asc_isspace(int c) 
  {
    return c==' ' || c=='\n' || c=='\r' || c=='\t' || c=='\f' || c=='\v';
  }

  static inline bool asc_isdigit(int c)
  {
    return '0' <= c && c <= '9';
  }
  static inline bool asc_islower(int c)
  {
    return 'a' <= c && c <= 'z';
  }
  static inline bool asc_isupper(int c)
  {
    return 'A' <= c && c <= 'Z';
  }
  static inline bool asc_isalpha(int c)
  {
    return asc_islower(c) || asc_isupper(c);
  }
  static inline int asc_tolower(int c)
  {
    return asc_isupper(c) ? c + 0x20 : c;
  }
  static inline int asc_toupper(int c)
  {
    return asc_islower(c) ? c - 0x20 : c;
  }
  
}

#endif
