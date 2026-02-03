// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "settings.h"
#include "indiv_filter.hpp"
#include "key_info.hpp"

namespace {
  using namespace acommon;

  class UrlFilter : public IndividualFilter {
  public:
    PosibErr<bool> setup(Config *);
    void reset() {}
    void process(FilterChar * &, FilterChar * &);
  };

  PosibErr<bool> UrlFilter::setup(Config *) 
  {
    name_ = "url-filter";
    order_num_ = 0.95;
    return true;
  }

  static bool url_char(char c)
  {
    return c != '"' && c != ' ' && c != '\n' && c != '\t';
  }
 
  void UrlFilter::process(FilterChar * & str, FilterChar * & end)
  {
    for (FilterChar * cur = str; cur < end; ++cur) 
    {
      if (!url_char(*cur)) continue;
      FilterChar * cur0 = cur;
      bool blank_out = false;
      int point_chars = 0;
      // only consider special url deciding characters if they are in
      // the middle of a word
      for (++cur; cur + 1 < end && url_char(cur[1]); ++cur) {
        if (blank_out) continue;
        if ((cur[0] == '/' && (point_chars > 0 || cur[1] == '/'))
            || cur[0] == '@') {
          blank_out = true;
        } else if (cur[0] == '.' && cur[1] != '.') { 
          // count multiple '.' as one
          if (point_chars < 1) ++point_chars;
          else                 blank_out = true;
        }
      }
      ++cur;
      if (blank_out) {
	for (; cur0 != cur; ++cur0) *cur0 = ' ';
      }
    }
  }
}

C_EXPORT 
IndividualFilter * new_aspell_url_filter() {
  return new UrlFilter;                                
}

