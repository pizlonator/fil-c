#ifndef __aspeller_data_util_hh__
#define __aspeller_data_util_hh__

#include <ctime>

//POSIX headers
#include <sys/stat.h>

#include "parm_string.hpp"

using namespace acommon;

namespace aspeller {

  template <class Itr>
  struct CharStrParms {
    typedef const char * Value;
    typedef Itr          Iterator;
    Iterator   end_;
    CharStrParms(Iterator e) : end_(e) {}
    bool endf(Iterator i) const {return i == end_;}
      Value deref(Iterator i) const {return *i;}
    Value end_state() const {return 0;}
  };
  
  template <class Itr>
  struct StrParms {
    typedef const char * Value;
    typedef Itr          Iterator;
    Iterator   end_;
    StrParms(Iterator e) : end_(e) {}
    bool endf(Iterator i) const {return i == end_;}
    Value deref(Iterator i) const {return i->c_str();}
    Value end_state() const {return 0;}
  };
  
  inline time_t modification_date(ParmString file) {
    struct stat file_stat;
    if (stat(file, &file_stat) == 0)
      return file_stat.st_mtime;
    else 
      return 0;
  }
}

#endif
