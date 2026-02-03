// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_PARM_STRING__HPP
#define ASPELL_PARM_STRING__HPP

#include <string.h>
#include <limits.h>

//
// ParmString is a special string class that is designed to be used as
// a parameter for a function that is expecting a string It will allow
// either a "const char *" or "String" class to be passed in.  It will
// automatically convert to a "const char *".  The string can also be
// accesses via the "str" method.  Usage example:
//
// void foo(ParmString s1, s2) {
//   const char *  str0 = s1;
//   unsigned int size0 = s2.size()
//   if (s1 == s2 || s2 == "bar") {
//     ...
//   }
// }
// ...
// String s1 = "...";
// foo(s1);
// const char * s2 = "...";
// foo(s2);
//
// The string is expected to be null terminated, even if size is give
// during construction.
//

namespace acommon {

  template<typename Ret> class PosibErr;

  class String;

  class ParmString {
  public:
    ParmString() : str_(0) {}
    ParmString(const char * str, unsigned int sz = UINT_MAX) 
      : str_(str), size_(sz) {}
    inline ParmString(const String &);
    inline ParmString(const PosibErr<const char *> &);
    inline ParmString(const PosibErr<String> &);

    void set(const char * str, unsigned int sz = UINT_MAX) {
      str_ = str, size_ = sz;
    }

    bool empty() const {
      return str_ == 0 || str_[0] == '\0';
    }
    unsigned int size() const {
      if (size_ != UINT_MAX) return size_;
      else return size_ = strlen(str_);
    }
    bool have_size() const {return size_ != UINT_MAX;}
    operator const char * () const {
      return str_;
    }
    const char * str () const {
      return str_;
    }
  public: // but don't use unless you have to
    const char * str_;
    mutable unsigned int size_;
  };

  typedef const ParmString & ParmStr;

  static inline bool operator== (ParmStr s1, ParmStr s2)
  {
    if (s1.str() == 0 || s2.str() == 0)
      return s1.str() == s2.str();
    return strcmp(s1,s2) == 0;
  }
  static inline bool operator== (const char * s1, ParmStr s2)
  {
    if (s1 == 0 || s2.str() == 0)
      return s1 == s2.str();
    return strcmp(s1,s2) == 0;
  }
  static inline bool operator== (ParmStr s1, const char * s2)
  {
    if (s1.str() == 0 || s2 == 0)
      return s1.str() == s2;
    return strcmp(s1,s2) == 0;
  }
  static inline bool operator!= (ParmStr s1, ParmStr s2)
  {
    if (s1.str() == 0 || s2.str() == 0)
      return s1.str() != s2.str();
    return strcmp(s1,s2) != 0;
  }
  static inline bool operator!= (const char * s1, ParmStr s2)
  {
    if (s1 == 0 || s2.str() == 0)
      return s1 != s2.str();
    return strcmp(s1,s2) != 0;
  }
  static inline bool operator!= (ParmStr s1, const char * s2)
  {
    if (s1.str() == 0 || s2 == 0)
      return s1.str() != s2;
    return strcmp(s1,s2) != 0;
  }

}

#endif
