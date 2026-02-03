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

#ifndef autil__generic_copy_ptr
#define autil__generic_copy_ptr

namespace acommon {

  // Parms is expected to have the following members
  //   T * Parms::clone(const T *);
  //   void Parms::assign(T * &, const T *);
  //   void Parms::del(T *);
  // All members can assume that all pointers are not null

  template <typename T, typename Parms> 
  class GenericCopyPtr {

    T *   ptr_;
    Parms parms_;

  public:

    explicit GenericCopyPtr(T * p = 0, const Parms & parms = Parms()) 
      : ptr_(p), parms_(parms) {}

    GenericCopyPtr(const GenericCopyPtr & other);
    GenericCopyPtr & operator= (const GenericCopyPtr & other) {
      assign(other.ptr_, parms_);
      return *this;
    }

    // assign makes a copy of other
    void assign(const T * other, const Parms & parms = Parms());

    // reset takes ownership of other
    void reset(T * other = 0, const Parms & parms = Parms());

    T & operator*  () const {return *ptr_;}
    T * operator-> () const {return ptr_;}
    T * get() const {return ptr_;}
    operator T * () const {return ptr_;}
    
    T * release() {T * tmp = ptr_; ptr_ = 0; return tmp;}

    const Parms & parms() {return parms_;}
    const Parms & parms() const {return parms_;}

    ~GenericCopyPtr();
  };

}

#endif

