// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_STRING_ENUMERATION__HPP
#define ASPELL_STRING_ENUMERATION__HPP

#include "parm_string.hpp"
#include "type_id.hpp"
#include "char_vector.hpp"

namespace acommon {

  class StringEnumeration;
  class Convert;

  class StringEnumeration {
  public:
    typedef const char * Value;
    virtual bool at_end() const = 0;
    virtual const char * next() = 0;
    int ref_count_;
    TypeId type_id_;
    unsigned int type_id() { return type_id_.num; }
    int copyable_;
    int copyable() { return copyable_; }
    virtual StringEnumeration * clone() const = 0;
    virtual void assign(const StringEnumeration * other) = 0;
    CharVector temp_str;
    Convert * from_internal_;
    StringEnumeration() : ref_count_(0), copyable_(2), from_internal_(0) {}
    virtual ~StringEnumeration() {}
  };

}

#endif /* ASPELL_STRING_ENUMERATION__HPP */
