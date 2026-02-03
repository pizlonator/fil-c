/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_KEY_INFO__HPP
#define ASPELL_KEY_INFO__HPP

#include "key_info.hpp"

namespace acommon {


enum KeyInfoType {KeyInfoString,KeyInfoInt,KeyInfoBool,KeyInfoList};
struct KeyInfo {
  const char * name;
  KeyInfoType type;
  const char * def;
  const char * desc;
  int flags;
  int other_data;
};


}

#endif /* ASPELL_KEY_INFO__HPP */
