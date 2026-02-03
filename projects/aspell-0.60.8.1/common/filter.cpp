/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "settings.h"

#include "config.hpp"
#include "filter.hpp"
#include "speller.hpp"
#include "indiv_filter.hpp"
#include "strtonum.hpp"
#include "errors.hpp"
#include "asc_ctype.hpp"

#ifdef HAVE_LIBDL
#  include <dlfcn.h>
#endif

namespace acommon {

  FilterHandle::~FilterHandle() 
  {
    //FIXME: This causes a seg fault
    //if (handle) dlclose(handle);
  } 

  Filter::Filter() {}

  void Filter::add_filter(IndividualFilter * filter)
  {
    Filters::iterator cur, end;
    cur = filters_.begin();
    end = filters_.end();
    while ((cur != end) && (filter->order_num() > (*cur)->order_num())){
      ++cur;
    }
    filters_.insert(cur, filter);
  }

  void Filter::reset()
  {
    Filters::iterator cur, end;
    cur = filters_.begin();
    end = filters_.end();
    for (; cur != end; ++cur)
      (*cur)->reset();
  }

  void Filter::process(FilterChar * & start, FilterChar * & stop)
  {
    Filters::iterator cur, end;
    cur = filters_.begin();
    end = filters_.end();
    for (; cur != end; ++cur)
      (*cur)->process(start, stop);
  }

  void Filter::clear()
  {
    Filters::iterator cur, end;
    cur = filters_.begin();
    end = filters_.end();
    for (; cur != end; ++cur){
      delete *cur;
    }
    filters_.clear();
  }

  Filter::~Filter() 
  {
    clear();
  }

  static PosibErr<int> version_compare(const char * x, const char * y)
  {
    do {
      int xn = 0, yn = 0;
      if (*x) {
        if (!asc_isdigit(*x)) return make_err(bad_version_string);
        xn = strtoi_c(x, &x);}
      if (*y) {
        if (!asc_isdigit(*y)) return make_err(bad_version_string);
        yn = strtoi_c(y, &y);}
      int diff = xn - yn;
      if (diff != 0) return diff;
      if (*x) {
        if (*x != '.') return make_err(bad_version_string);
        ++x;}
      if (*y) {
        if (*y != '.') return make_err(bad_version_string);
        ++y;}
    } while (*x || *y);
    return 0;
  }

  PosibErr<bool> verify_version(const char * rel_op, 
                                const char * actual, const char * required) 
  {
    assert(actual != NULL && required != NULL);

    RET_ON_ERR_SET(version_compare(actual, required), int, cmp);

    if (cmp == 0 && strchr(rel_op, '=')) return true;
    if (cmp < 0  && strchr(rel_op, '<')) return true;
    if (cmp > 0  && strchr(rel_op, '>')) return true;
    return false;
  }

  PosibErr<void> check_version(const char * requirement)
  {
    const char * s = requirement;
    
    if (*s == '>' || *s == '<') s++;
    if (*s == '=')              s++;

    String rel_op(requirement, s - requirement);
    String req_ver(s);
    
    char act_ver[] = PACKAGE_VERSION;

    char * seek = act_ver;
    while (*seek && *seek != '-') ++seek; // remove any part after the '-'
    *seek = '\0';

    PosibErr<bool> peb = verify_version(rel_op.str(), act_ver, req_ver.str());
    
    if (peb.has_err()) {
      peb.ignore_err();
      return make_err(confusing_version);
    } else if ( peb == false ) {
      return make_err(bad_version);
    } else {
      return no_err;
    }
  }
}

