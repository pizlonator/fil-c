/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_TYPE_ID__HPP
#define ASPELL_TYPE_ID__HPP

#include "parm_string.hpp"

namespace acommon {


union TypeId {
  unsigned int num;
  char str[4];
  TypeId(ParmString str);
  TypeId() : num(0) {}
};


}

#endif /* ASPELL_TYPE_ID__HPP */
