// This file is part of The New Aspell
// Copyright (C) 2019 by Kevin Atkinson under the GNU LGPL license
// version 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_ISTREAM_ENUMERATION__HPP
#define ASPELL_ISTREAM_ENUMERATION__HPP

#include "fstream.hpp"
#include "string_enumeration.hpp"

namespace acommon {

  class IstreamEnumeration : public StringEnumeration {
    FStream * in;
    String data;
  public:
    IstreamEnumeration(FStream & i) : in(&i) {}
    IstreamEnumeration * clone() const {
      return new IstreamEnumeration(*this);
    }
    void assign (const StringEnumeration * other) {
      *this = *static_cast<const IstreamEnumeration *>(other);
    }
    Value next() {
      if (!in->getline(data)) return 0;
      else return data.c_str();
    }
    bool at_end() const {return *in;}
  };

}

#endif
