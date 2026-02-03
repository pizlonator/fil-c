/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "error.hpp"
#include "filter.hpp"

namespace acommon {

class CanHaveError;
struct Error;
class Filter;

extern "C" void delete_aspell_filter(Filter * ths)
{
  delete ths;
}

extern "C" unsigned int aspell_filter_error_number(const Filter * ths)
{
  return ths->err_ == 0 ? 0 : 1;
}

extern "C" const char * aspell_filter_error_message(const Filter * ths)
{
  return ths->err_ ? ths->err_->mesg : "";
}

extern "C" const Error * aspell_filter_error(const Filter * ths)
{
  return ths->err_;
}

extern "C" Filter * to_aspell_filter(CanHaveError * obj)
{
  return static_cast<Filter *>(obj);
}



}

