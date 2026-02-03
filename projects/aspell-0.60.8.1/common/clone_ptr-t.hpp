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

#ifndef autil__clone_ptr_t
#define autil__clone_ptr_t

#include "clone_ptr.hpp"
#include <typeinfo>
#include "generic_copy_ptr-t.hpp"

namespace acommon {

  template <typename T>
  inline T * ClonePtr<T>::Parms::clone(const T * ptr) const {
    return ptr->clone();
  }

  template <typename T>
  void ClonePtr<T>::Parms::assign(T * & rhs, const T * lhs) const {
    if (typeid(*rhs) == typeid(*lhs)) {
      rhs->assign(lhs);
    } else {
      T * temp = rhs;
      rhs = lhs->clone();
      delete temp;
    }
  }

  template <typename T>
  inline void ClonePtr<T>::Parms::del(T * ptr) {
    delete ptr;
  }

}

#endif
