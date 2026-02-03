#ifndef FILTER_ENTRY_HEADER
#define FILTER_ENTRY_HEADER

#include "indiv_filter.hpp"

namespace acommon {
  
  typedef IndividualFilter * (FilterFun) ();

  struct FilterEntry
  {
    const char * name;
    FilterFun * decoder;
    FilterFun * filter;
    FilterFun * encoder;
  };
};
#endif
  
