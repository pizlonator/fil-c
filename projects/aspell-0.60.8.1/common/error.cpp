// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <string.h>
#include <stdlib.h>

#include "error.hpp"

namespace acommon {

  bool Error::is_a(ErrorInfo const * to_find) const 
  {
    const ErrorInfo * e = err;
    while (e) {
      if (e == to_find) return true;
      e = e->isa;
    }
    return false;
  }

  Error::Error(const Error & other)
  {
    if (other.mesg) {
      mesg = (char *)malloc(strlen(other.mesg) + 1);
      strcpy(const_cast<char *>(mesg), other.mesg);
    }
    err = other.err;
  }

  Error & Error::operator=(const Error & other)
  {
    if (mesg)
      free(const_cast<char *>(mesg));
    if (other.mesg) {
      unsigned int len = strlen(other.mesg) + 1;
      mesg = (char *)malloc(len);
      memcpy(const_cast<char *>(mesg), other.mesg, len);
    }
    err = other.err;
    return *this;
  }

  Error::~Error()
  {
    if (mesg)
      free(const_cast<char *>(mesg));
  }

}
