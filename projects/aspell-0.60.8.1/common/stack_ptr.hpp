// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef stack_ptr
#define stack_ptr

#include <assert.h>

namespace acommon {
  
  template <typename T>
  class StackPtr {
    T * ptr;

    // to avoid operator* being used unexpectedly.  for example 
    // without this the following will compile
    //   PosibErr<T> fun(); 
    //   PosibErr<StackPtr<T > > pe = fun();
    // and operator* and StackPtr(T *) will be used.  The explicit
    // doesn't protect us here due to PosibErr
    StackPtr(const StackPtr & other);
    // because I am paranoid
    StackPtr & operator=(const StackPtr & other);

  public:

    explicit StackPtr(T * p = 0) : ptr(p) {}

    StackPtr(StackPtr & other) : ptr (other.release()) {}

    ~StackPtr() {del();}

    StackPtr & operator=(StackPtr & other) 
      {reset(other.release()); return *this;}

    T & operator*  () const {return *ptr;}
    T * operator-> () const {return ptr;}
    T * get()         const {return ptr;}
    operator T * ()   const {return ptr;}

    T * release () {T * p = ptr; ptr = 0; return p;}

    void del() {delete ptr; ptr = 0;}
    void reset(T * p) {assert(ptr==0); ptr = p;}
    StackPtr & operator=(T * p) {reset(p); return *this;}
    
  };
}

#endif

