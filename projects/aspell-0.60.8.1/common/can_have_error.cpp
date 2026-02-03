// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "error.hpp"
#include "can_have_error.hpp"

namespace acommon {

  CanHaveError::CanHaveError(Error * e)
    : err_(e)
  {}

  CanHaveError::~CanHaveError() 
  {}

  CanHaveError::CanHaveError(const CanHaveError & other)
    : err_(other.err_) {}


  CanHaveError & CanHaveError::operator=(const CanHaveError & other)
  {
    err_ = other.err_;
    return *this;
  }


}
