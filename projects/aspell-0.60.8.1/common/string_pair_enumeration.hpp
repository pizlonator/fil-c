/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_STRING_PAIR_ENUMERATION__HPP
#define ASPELL_STRING_PAIR_ENUMERATION__HPP

#include "string_pair.hpp"

namespace acommon {

class StringPairEnumeration;

class StringPairEnumeration {
 public:
  virtual bool at_end() const = 0;
  virtual StringPair next() = 0;
  virtual StringPairEnumeration * clone() const = 0;
  virtual void assign(const StringPairEnumeration * other) = 0;
  StringPairEnumeration() {}
  virtual ~StringPairEnumeration() {}
};


}

#endif /* ASPELL_STRING_PAIR_ENUMERATION__HPP */
