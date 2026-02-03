// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ACOMMON_FILTER__HPP
#define ACOMMON_FILTER__HPP

#include <assert.h>

#include "string.hpp"
#include "posib_err.hpp"
#include "filter_char.hpp"

namespace acommon {

  class Config;

  class FilterHandle {
  public:
    FilterHandle() : handle(0) {}
    ~FilterHandle();
    void * release() {
      void * tmp = handle;
      handle = NULL;
      return tmp;
    }
    operator bool() {return handle != NULL;}
    void * get() {return handle;}

    // The direct interface usually when new_filter ... functions are coded
    // manually
    FilterHandle & operator= (void * h) {
      assert(handle == NULL);
      handle = h; 
      return *this;
    }
  private:
    FilterHandle(const FilterHandle &);
    void operator = (const FilterHandle &);
    void * handle;
  };  

  class IndividualFilter {
  public:

    // sets up the filter 
    //
    // any options effecting this filter should start with the filter
    // name followed by a dash
    // should return true if the filter should be used false otherwise
    // (in which case it will be deleted)
    virtual PosibErr<bool> setup(Config *) = 0;

    // reset the internal state of the filter
    //
    // should be called whenever a new document is being filtered
    virtual void reset() = 0;

    // process the string
    //
    // The filter may either modify the string passed in or use its
    // own buffer for the output.  If the string uses its own buffer
    //  start and stop should be set to the beginning and one past the
    // end of its buffer respectfully.
    //
    // The string passed in should only be split on white space
    // characters.  Furthermore, between calls to reset, each string
    // should be passed in exactly once and in the order they appeared
    // in the document.  Passing in strings out of order, skipping
    // strings or passing them in more than once may lead to undefined
    // results.
    //
    // The following properties are guaranteed to be true and must
    // stay true:
    //   *stop  == '\0';
    //   strlen == stop - start;
    // this way it is always safe to look one character ahead.
    //
    virtual void process(FilterChar * & start, FilterChar * & stop) = 0;

    virtual ~IndividualFilter() {}

    const char * name() const {return name_.str();}
    double order_num() const {return order_num_;}

    FilterHandle handle;

  protected:

    IndividualFilter() : name_(0), order_num_(0.50) {}
    
    String name_; // must consist of 'a-z|0-9'
    double order_num_; // between 0 and 1 exclusive
  };

}

#endif
