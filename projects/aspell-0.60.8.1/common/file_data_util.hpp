#ifndef aspeller_file_data_util__hh
#define aspeller_file_data_util__hh

#include "parm_string.hpp"

namespace acommon {class Config;}

namespace acommon {

  void fill_data_dir(const Config *, String & dir1, String & dir2);
  const String & find_file(String & path,
                           const String & dir1, const String & dir2, 
                           const String & name, const char * extension);
  bool find_file(String & file,
                 const String & dir1, const String & dir2, 
                 const String & name, 
                 ParmString preext, ParmString ext);
}

#endif
