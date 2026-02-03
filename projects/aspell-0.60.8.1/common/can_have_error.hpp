/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_CAN_HAVE_ERROR__HPP
#define ASPELL_CAN_HAVE_ERROR__HPP

#include "copy_ptr.hpp"
#include "error.hpp"

namespace acommon {

struct Error;

class CanHaveError {
 public:
  CanHaveError(Error * e = 0);
  CopyPtr<Error> err_;
  virtual ~CanHaveError();
  CanHaveError(const CanHaveError &);
  CanHaveError & operator=(const CanHaveError &);
};


}

#endif /* ASPELL_CAN_HAVE_ERROR__HPP */
