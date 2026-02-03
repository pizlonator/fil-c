/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "convert.hpp"
#include "string_enumeration.hpp"

namespace acommon {

class StringEnumeration;

extern "C" void delete_aspell_string_enumeration(StringEnumeration * ths)
{
  delete ths;
}

extern "C" StringEnumeration * aspell_string_enumeration_clone(const StringEnumeration * ths)
{
  return ths->clone();
}

extern "C" void aspell_string_enumeration_assign(StringEnumeration * ths, const StringEnumeration * other)
{
  ths->assign(other);
}

extern "C" int aspell_string_enumeration_at_end(const StringEnumeration * ths)
{
  return ths->at_end();
}

extern "C" const char * aspell_string_enumeration_next(StringEnumeration * ths)
{
  const char * s = ths->next();
  if (s == 0 || ths->from_internal_ == 0) {
    return s;
  } else {
    ths->temp_str.clear();
    ths->from_internal_->convert(s,-1,ths->temp_str);
    ths->from_internal_->append_null(ths->temp_str);
    return ths->temp_str.data();
  }
}

extern "C" const void * aspell_string_enumeration_next_wide(StringEnumeration * ths, int type_width)
{
  const char * s = ths->next();
  if (s == 0) {
    return s;
  } else if (ths->from_internal_ == 0) {
    assert(type_width == 1);
    return s;
  } else {
    assert(type_width == ths->from_internal_->out_type_width());
    ths->temp_str.clear();
    ths->from_internal_->convert(s,-1,ths->temp_str);
    ths->from_internal_->append_null(ths->temp_str);
    return ths->temp_str.data();
  }
}



}

