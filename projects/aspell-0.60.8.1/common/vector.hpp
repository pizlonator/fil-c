// This file is part of The New Aspell
// Copyright (C) 2001-2003 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_VECTOR__HPP
#define ASPELL_VECTOR__HPP

#include <vector>

namespace acommon
{
  template <typename T>
  class Vector : public std::vector<T>
  {
  public:

    Vector() {}
    Vector(unsigned int s) : std::vector<T>(s) {}
    Vector(unsigned int s, const T & val) : std::vector<T>(s, val) {}

    void append(T t) {
      this->push_back(t);
    }
    void append(const T * begin, unsigned int size) {
      this->insert(this->end(), begin, begin+size);
    }
    void append(const T * begin, const T * end) {
      this->insert(this->end(), begin, end);
    }
    int alloc(int s) {
      int pos = this->size();
      this->resize(pos + s);
      return pos;
    }

#if __cplusplus < 201103L
    T * data() {return &*this->begin();}
    const T * data() const {return &*this->begin();}
#else
    using std::vector<T>::data;
#endif

    T * data(int pos) {return data() + pos;}
    T * data_end() {return data() + this->size();}

    T * pbegin() {return data();}
    T * pend()   {return data() + this->size();}

    const T * pbegin() const {return data();}
    const T * pend()   const {return data() + this->size();}

    template <typename U>
    U * datap() { 
      return reinterpret_cast<U * >(data());
    }
    template <typename U>
    U * datap(int pos) {
      return reinterpret_cast<U * >(data() + pos);
    }

    void pop_front() {this->erase(this->begin());}
    void push_front(const T & v) {this->insert(this->begin(), v);}
  };
}

#endif
