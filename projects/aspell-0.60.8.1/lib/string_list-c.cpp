/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "posib_err.hpp"
#include "string_list.hpp"

namespace acommon {

class MutableContainer;
class StringEnumeration;
class StringList;

extern "C" StringList * new_aspell_string_list()
{
  return new_string_list();
}

extern "C" int aspell_string_list_empty(const StringList * ths)
{
  return ths->empty();
}

extern "C" unsigned int aspell_string_list_size(const StringList * ths)
{
  return ths->size();
}

extern "C" StringEnumeration * aspell_string_list_elements(const StringList * ths)
{
  return ths->elements();
}

extern "C" int aspell_string_list_add(StringList * ths, const char * to_add)
{
  return ths->add(to_add);
}

extern "C" int aspell_string_list_remove(StringList * ths, const char * to_rem)
{
  return ths->remove(to_rem);
}

extern "C" void aspell_string_list_clear(StringList * ths)
{
  ths->clear();
}

extern "C" MutableContainer * aspell_string_list_to_mutable_container(StringList * ths)
{
  return ths;
}

extern "C" void delete_aspell_string_list(StringList * ths)
{
  delete ths;
}

extern "C" StringList * aspell_string_list_clone(const StringList * ths)
{
  return ths->clone();
}

extern "C" void aspell_string_list_assign(StringList * ths, const StringList * other)
{
  ths->assign(other);
}



}

