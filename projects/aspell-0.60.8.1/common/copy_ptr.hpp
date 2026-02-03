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

#ifndef autil__copy_ptr
#define autil__copy_ptr

#include "generic_copy_ptr-t.hpp"

namespace acommon {

  template <typename T>
  class CopyPtr 
  {
    struct Parms {
      T * clone(const T * ptr) const
		{ return new T(*ptr);}
      void assign(T * & rhs, const T * lhs) const
		{ *rhs = *lhs; }
      void del(T * ptr)
		{ delete ptr; }

    };
    GenericCopyPtr<T, Parms> impl;

  public:

    explicit CopyPtr(T * p = 0) : impl(p) {}
    CopyPtr(const CopyPtr & other) : impl(other.impl) {}

    CopyPtr & operator=(const CopyPtr & other) {
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

