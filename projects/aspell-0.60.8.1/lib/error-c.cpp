/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "error.hpp"

namespace acommon {

struct Error;
struct ErrorInfo;

extern "C" int aspell_error_is_a(const Error * ths, const ErrorInfo * e)
{
  return ths->is_a(e);
}



}

