#ifndef autil__basic_list_hh
#define autil__basic_list_hh

#include <list>
//#include <ext/slist>

//#include <assert.h>

// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

// BasicList is a simple list structure which can either be
//   implemented as a singly or doubly linked list.  I created it
//   because a Singly liked list is not part of the C++ standard however
//   the Doubly Linked list does not have the same methods as the doubly
//   linked list because the singly linked list has a bunch of special
//   methods to address the fact that it is far faster to insert
//   elements before the position pointed to by an iterator is far faster
//   than inserting elements after the current position which is what is
//   normally done.  Thus it is not possibly to simply substitute a singly
//   linked list with a doubly linked list by changing the name of the
//   container used.  This list currently acts as a wrapper for the list
//   (doubly linked list) STL class however it can also very easily be
//   written as a wrapper for the slist (singly linked list) class when
//   it is available (slist is part of the SGI STL but not the C++
//   standard) for better performance.

namespace acommon {

  template <typename T>
  class BasicList {
    //typedef __gnu_cxx::slist<T> List;
    typedef std::list<T> List;
    List data_;
  public:
    // treat the iterators as forward iterators only
    typedef typename List::iterator       iterator;
    typedef typename List::const_iterator const_iterator;
    typedef typename List::size_type      size_type;
    bool empty() {return data_.empty();}
    void clear() {data_.clear();}
    size_type size() {return data_.size();}
    iterator begin() {return data_.begin();}
    iterator end()   {return data_.end();}
    const_iterator begin() const {return data_.begin();}
    const_iterator end()   const {return data_.end();}
    void push_front(const T & item) {data_.push_front(item);}
    void pop_front() {data_.pop_front();}
          T & front() {return data_.front();}
    const T & front() const {return data_.front();}
    void swap(BasicList & other) {data_.swap(other.data_);}
    void sort() {data_.sort();}
    template<class Pred> void sort(Pred pr) {data_.sort(pr);}
    void splice_into (BasicList & other, iterator prev, iterator cur)
    {
      //++prev;
      //assert (prev == cur);
      data_.splice(data_.begin(),other.data_,cur);
      //data_.splice_after(data_.begin(), prev);
    }
    template<class Pred>
    void merge(BasicList & other, Pred pr) {data_.merge(other.data_, pr);}
  };

}

#endif
