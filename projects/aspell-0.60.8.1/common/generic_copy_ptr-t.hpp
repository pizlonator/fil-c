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

#ifndef autil__generic_copy_ptr_t
#define autil__generic_copy_ptr_t

#include "generic_copy_ptr.hpp"

namespace acommon {

  template <typename T, typename Parms> 
  GenericCopyPtr<T,Parms>::GenericCopyPtr(const GenericCopyPtr & other) 
  {
    if (other.ptr_ != 0) 
      ptr_ = parms_.clone(other.ptr_);
    else
      ptr_ = 0;
    parms_  = other.parms_;
  }
  
  template <typename T, typename Parms>
  void GenericCopyPtr<T,Parms>::assign(const T * other_ptr, 
				       const Parms & other_parms) 
  {
    if (other_ptr == 0) {
      if (ptr_ != 0) parms_.del(ptr_);
      ptr_ = 0;
    } else if (ptr_ == 0) {
      ptr_ = parms_.clone(other_ptr);
    } else {
      parms_.assign(ptr_, other_ptr);
    }
    parms_ = other_parms;
  }

  template <typename T, typename Parms>
  void GenericCopyPtr<T,Parms>::reset (T * other, const Parms & other_parms) 
  {
    if (ptr_ != 0) 
      parms_.del(ptr_);
    ptr_ = other;
    parms_ = other_parms;
  }

  template <typename T, typename Parms>
  GenericCopyPtr<T,Parms>::~GenericCopyPtr() 
  {
    if (ptr_ != 0)
      parms_.del(ptr_);
  }
  
}

#endif
