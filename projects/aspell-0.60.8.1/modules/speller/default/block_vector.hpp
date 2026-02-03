// Copyright (c) 2000
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

#ifndef __autil_block_vector__
#define __autil_block_vector__

#include <stddef.h>
//#include <iterator>

namespace aspeller {

  template <typename T>
  class BlockVector {
    T * begin_;
    T * end_;
  public:
    typedef T value_type;
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;
    typedef T *       iterator;
    typedef T *       const_iterator;
    typedef T &       reference;
    typedef const T & const_reference;
    typedef T *       pointer;
    typedef T *       const_pointer;
    //typedef std::random_access_iterator_tag  iterator_category;
    typedef ptrdiff_t                        distance_type;

    BlockVector() : begin_(0), end_(0) {}
    BlockVector(size_type) : begin_(0), end_(0) {} // noop
    BlockVector(T * b, T * e) : begin_(b), end_(e) {}
    void set(T * b, T * e) {begin_ = b; end_ = e;}
    iterator begin() {return begin_;}
    iterator end()   {return end_;}
    const_iterator begin() const {return begin_;}
    const_iterator end()   const {return end_;}
    size_type size() const {return end_ - begin_;}
    bool empty() const {return begin_ != end_;}
    reference operator[](size_type i) {return *(begin_ + i);}
    const_reference operator[](size_type i) const {return *(begin_ + i);}
  };

}

#endif
