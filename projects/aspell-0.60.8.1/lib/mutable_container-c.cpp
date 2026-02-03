/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "mutable_container.hpp"
#include "posib_err.hpp"

namespace acommon {

class MutableContainer;

extern "C" int aspell_mutable_container_add(MutableContainer * ths, const char * to_add)
{
  return ths->add(to_add);
}

extern "C" int aspell_mutable_container_remove(MutableContainer * ths, const char * to_rem)
{
  return ths->remove(to_rem);
}

extern "C" void aspell_mutable_container_clear(MutableContainer * ths)
{
  ths->clear();
}

extern "C" MutableContainer * aspell_mutable_container_to_mutable_container(MutableContainer * ths)
{
  return ths;
}



}

