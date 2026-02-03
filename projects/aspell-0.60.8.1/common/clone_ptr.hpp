// Copyright (c) 2001
// Kevin Atkinson
//
// Permission to use, copy, modify, distribute and sell this software
// and its documentation for any purpose is hereby granted without
// fee, provided that the above copyright notice appear in all copies
// and that both that copyright notice and this permission notice
// appear in supporting documentation. Kevin Atkinson makes no
// representations about the suitability of this software for any
// purpose.  It is provided "as is" without express or implied
// warranty.

#ifndef autil__clone_ptr
#define autil__clone_ptr

#include "generic_copy_ptr.hpp"

namespace acommon {

  template <typename T>
  class ClonePtr 
  {
    struct Parms {
      T * clone(const T * ptr) const;
      void assign(T * & rhs, const T * lhs) const;
      void del(T * ptr);
    };
    GenericCopyPtr<T, Parms> impl;

  public:

    explicit ClonePtr(T * p = 0) : impl(p) {}
    ClonePtr(const ClonePtr & other) : impl(other.impl) {}

    ClonePtr & operator=(const ClonePtr & other) {
      impl = other.impl; 
      return *this;
    }

    void assign(const T * other) {impl.assign(other);}
    void reset(T * other = 0)    {impl.reset(other);}
    
    T & operator*  () const {return *impl;}
    T * operator-> () const {return impl;}
    T * get()         const {return impl;}
    operator T * ()   const {return impl;}

    T * release() {return impl.release();}
  };
  
}

#endif

