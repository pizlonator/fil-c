// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <stdarg.h>
#include <stdio.h>

#ifndef va_copy
#  ifdef __va_copy
#    define va_copy __va_copy
#  else
#    define va_copy(dst, src) memcpy(&dst, &src, sizeof(va_list))
#  endif
#endif

#include "string.hpp"
#include "asc_ctype.hpp"

namespace acommon {
  
  // reserve space for at least s+1 characters
  void String::reserve_i(size_t s)
  {
    size_t old_size = end_ - begin_;
    size_t new_size = (storage_end_ - begin_) * 3 / 2;
    if (new_size < 64) new_size = 64;
    if (new_size < s + 1) new_size = s + 1;
    if (old_size == 0) {
      if (begin_) free(begin_);
      begin_ = (char *)malloc(new_size);
    } else {
      begin_ = (char *)realloc(begin_, new_size);
    }
    end_ = begin_ + old_size;
    storage_end_ = begin_ + new_size;
  }

  int String::vprintf(const char * format, va_list ap0)
  {
    reserve(size() + 64);
    int res = 0;
    va_list ap;
  loop: {
      int avail = storage_end_ - end_;
      if (res < 0 && avail > 1024*1024) 
        return -1; // to avoid an infinite loop in case a neg result
                   // really means an error and not just "not enough
                   // space"
      va_copy(ap,ap0);
      res = vsnprintf(end_, avail, format, ap);
      va_end(ap);
      if (res < 0) {
        reserve_i(); goto loop;
      } else if (res > avail) {
        reserve_i(size() + res); goto loop;
      }
    }
    end_ += res;
    return res;
  }
  
  bool StringIStream::append_line(String & str, char d)
  {
    if (in_str[0] == '\0') return false;
    const char * end = in_str;
    while (*end != d && *end != '\0') ++end;
    str.append(in_str, end - in_str);
    in_str = end;
    if (*in_str == d) ++in_str;
    return true;
  }

  bool StringIStream::read(void * data, unsigned int size)
  {
    char * str = static_cast<char *>(data);
    while (*in_str != '\0' && size != 0) {
      *str = *in_str;
      ++in_str;
      ++str;
      ++size;
    }
    return size == 0;
  }
}
