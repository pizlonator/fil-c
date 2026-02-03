#ifndef ASPELLER_DATA_ID__HPP
#define ASPELLER_DATA_ID__HPP

#include "settings.h"

#include <sys/stat.h>

namespace aspeller {
  
  class Dict::Id {
  public: // but don't use
    const Dict * ptr;
    const char * file_name;
#ifdef USE_FILE_INO
    ino_t           ino;
    dev_t           dev;
#endif
  public:
    Id(Dict * p, const FileName & fn = FileName());
  };

  inline bool Dict::cache_key_eq(const Id & o) 
  {
    return *id_ == o;
  }

}

#endif
