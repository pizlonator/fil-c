
// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_VARARRAY__HPP
#define ASPELL_VARARRAY__HPP

#ifndef __GNUC__
#  include <stdlib.h>
#endif

namespace acommon {

// only use this on types with a trivial constructors destructor

#ifdef __GNUC__ // use variable arrays

#define VARARRAY(type, name, num) type name[num]
#define VARARRAYM(type, name, num, max) type name[num]

#else // use malloc

  struct MallocPtr {
    void * ptr; 
    MallocPtr() : ptr(0) {}; 
    ~MallocPtr() {if (ptr) free(ptr);}
  };

#define VARARRAY(type, name, num) \
  acommon::MallocPtr name##_data;             \
  name##_data.ptr = malloc(sizeof(type) * (num)); \
  type * name = (type *)name##_data.ptr

#define VARARRAYM(type, name, num, max) type name[max]

#endif

#if 0 // this version uses alloca

#define VARARRAY(type, name, num) \
  type * name = (type *)alloca(sizeof(type) * (num))

#define VARARRAYM(type, name, num, max) \
  type * name = (type *)alloca(sizeof(type) * (num))

#endif

}

#endif

