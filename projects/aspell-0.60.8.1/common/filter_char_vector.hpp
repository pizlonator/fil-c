#ifndef acommon_filter_char_vector__hh
#define acommon_filter_char_vector__hh

// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "vector.hpp"
#include "filter_char.hpp"

namespace acommon {

  class FilterCharVector : public Vector<FilterChar>
  {
    typedef Vector<FilterChar> Base;
  public:
    void append(const char * str, FilterChar::Width w) {
      Base::append(FilterChar(*str, w));
      ++str;
      for (;*str; ++str)
	Base::append(FilterChar(*str, 1));
    }
    void append(FilterChar::Chr c, FilterChar::Width w) {
      Base::append(FilterChar(c, w));
    }
    void append(FilterChar t) {
      Base::append(t);
    }
    void append(FilterChar::Chr t) {
      Base::append(FilterChar(t));
    }
    void append(const FilterChar * begin, unsigned int size) {
      Base::append(begin, size);
    }
  };

}

#endif
