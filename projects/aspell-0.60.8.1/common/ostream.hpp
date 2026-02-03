// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_OSTREAM__HPP
#define ASPELL_OSTREAM__HPP

#include <stdarg.h>

#include "parm_string.hpp"

namespace acommon {

  // FIXME: Add Print Method compatible with printf and friends.
  //   Than avoid code bloat by using it in many places instead of
  //   out << "Bla " << something << " djdkdk " << something else << "\n"

  class OStream {
  public:
    virtual void write (char c) = 0;
    virtual void write (ParmStr) = 0;
    virtual void write (const void *, unsigned int) = 0;

    virtual int vprintf(const char *format, va_list ap) = 0;

#ifdef __GNUC__
    __attribute__ ((format (printf,2,3)))
#endif
      int printf(const char * format, ...)
    {
      va_list ap;
      va_start(ap, format);
      int res = vprintf(format, ap);
      va_end(ap);
      return res;
    }

    void put (char c) {write(c);}
    void put (ParmStr str) {write(str);}

    virtual void printl(ParmStr l) 
    {
      write(l);
      write('\n');
    }

    void write16(unsigned short v) {write(&v, 2);}
    void write32(unsigned int v) {write(&v, 4);}

    OStream & operator << (char c) {
      write(c);
      return *this;
    }

    OStream & operator << (ParmStr in) {
      write(in);
      return *this;
    }

    virtual ~OStream() {}
  };
  
}

#endif
