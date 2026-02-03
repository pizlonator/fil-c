/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_MUTABLE_CONTAINER__HPP
#define ASPELL_MUTABLE_CONTAINER__HPP

#include "parm_string.hpp"

namespace acommon {

class AddableContainer {
public:
  virtual PosibErr<bool> add(ParmStr to_add) = 0;
  virtual ~AddableContainer() {}
};


class MutableContainer : public AddableContainer {
public:
  virtual PosibErr<bool> remove(ParmStr to_rem) = 0;
  virtual PosibErr<void> clear() = 0;
};


}

#endif /* ASPELL_MUTABLE_CONTAINER__HPP */
