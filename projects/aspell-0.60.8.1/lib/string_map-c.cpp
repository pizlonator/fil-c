/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "posib_err.hpp"
#include "string_map.hpp"

namespace acommon {

class MutableContainer;
class StringMap;
class StringPairEnumeration;

extern "C" StringMap * new_aspell_string_map()
{
  return new_string_map();
}

extern "C" int aspell_string_map_add(StringMap * ths, const char * to_add)
{
  return ths->add(to_add);
}

extern "C" int aspell_string_map_remove(StringMap * ths, const char * to_rem)
{
  return ths->remove(to_rem);
}

extern "C" void aspell_string_map_clear(StringMap * ths)
{
  ths->clear();
}

extern "C" MutableContainer * aspell_string_map_to_mutable_container(StringMap * ths)
{
  return ths;
}

extern "C" void delete_aspell_string_map(StringMap * ths)
{
  delete ths;
}

extern "C" StringMap * aspell_string_map_clone(const StringMap * ths)
{
  return ths->clone();
}

extern "C" void aspell_string_map_assign(StringMap * ths, const StringMap * other)
{
  ths->assign(other);
}

extern "C" int aspell_string_map_empty(const StringMap * ths)
{
  return ths->empty();
}

extern "C" unsigned int aspell_string_map_size(const StringMap * ths)
{
  return ths->size();
}

extern "C" StringPairEnumeration * aspell_string_map_elements(const StringMap * ths)
{
  return ths->elements();
}

extern "C" int aspell_string_map_insert(StringMap * ths, const char * key, const char * value)
{
  return ths->insert(key, value);
}

extern "C" int aspell_string_map_replace(StringMap * ths, const char * key, const char * value)
{
  return ths->replace(key, value);
}

extern "C" const char * aspell_string_map_lookup(const StringMap * ths, const char * key)
{
  return ths->lookup(key);
}



}

