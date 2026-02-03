/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "can_have_error.hpp"
#include "error.hpp"

namespace acommon {

class CanHaveError;
struct Error;

extern "C" unsigned int aspell_error_number(const CanHaveError * ths)
{
  return ths->err_ == 0 ? 0 : 1;
}

extern "C" const char * aspell_error_message(const CanHaveError * ths)
{
  return ths->err_ ? ths->err_->mesg : "";
}

extern "C" const Error * aspell_error(const CanHaveError * ths)
{
  return ths->err_;
}

extern "C" void delete_aspell_can_have_error(CanHaveError * ths)
{
  delete ths;
}



}

