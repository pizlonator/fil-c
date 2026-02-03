// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_STRING__HPP
#define ASPELL_STRING__HPP

#include <string.h>
#include <stdlib.h>

#include <algorithm>

#include "hash_fun.hpp"
#include "parm_string.hpp"
#include "mutable_string.hpp"
#include "ostream.hpp"
#include "istream.hpp"

//
// acommon::String is similar to std::string, but without many of the
// extra non-stl like methods.  The string is guaranteed to be stored
// in a continues areas of memory but is not guaranteed to be null
// terminated.  However, space is always allocated for the null
// characters.  Thus, the c_str() method will never invalided any
// exiting pointers.  The string is also null terminated when accessed
// via the str() and mstr() methods.  In addition the method
// ensure_null_end() will null terminate the string.  Once null
// terminated the string will remain as such until the length of the
// string changes.
//

namespace acommon {

  template <typename Ret> class PosibErr;
  
  class String : public OStream
  {
  public:
    typedef const char * const_iterator;
    typedef char *       iterator;
    typedef size_t       size_type;

  private:
    // if begin_ != 0 than storage_end_ - begin_ > 1
    char * begin_;
    char * end_;
    char * storage_end_;

    void assign_only_nonnull(const char * b, unsigned size)
    {
      begin_ = (char *)malloc(size + 1);
      memmove(begin_, b, size);
      end_   = begin_ + size;
      storage_end_ = end_ + 1;
    }
    void zero() 
    {
      begin_ = 0;
      end_ = 0;
      storage_end_ = 0;
    }
    void assign_only(const char * b)
    {
      if (b && *b) assign_only_nonnull(b, strlen(b));
      else zero();
    }
    void assign_only(const char * b, unsigned size) 
    {
      if (b && size > 0) assign_only_nonnull(b, size);
      else zero();
    }
    void reserve_i(size_t s = 0);
  public:
    void reserve(size_t s) 
    {
      if (storage_end_ - begin_ >= (int)s + 1) return;
      reserve_i(s);
    }

    char * begin() {return begin_;}
    char * end() {return end_;}

    const char * begin() const {return begin_;}
    const char * end()   const {return end_;}

    char * pbegin() {return begin_;}
    char * pend() {return end_;}

    const char * pbegin() const {return begin_;}
    const char * pend()   const {return end_;}

    size_t size() const {return end_ - begin_;}
    bool empty() const {return begin_ == end_;}
    size_t max_size() const {return INT_MAX;}
    size_t capacity() const {return storage_end_ ? storage_end_ - begin_ - 1 : 0;}

    void ensure_null_end() const {
      if (!begin_) const_cast<String *>(this)->reserve_i();
      *end_ = '\0';
    }

    const char * c_str() const {
      if (begin_) {ensure_null_end(); return begin_;}
      else return "";
    }
    const char * str() const {return c_str();}
    char * mstr() 
    {
      if (!begin_) reserve_i();
      ensure_null_end();
      return begin_;
    }

    char * data() {return begin_;}
    const char * data() const {return begin_;}

    char * data(int pos) {return begin_ + pos;}
    char * data_end() {return end_;}

    template <typename U>
    U * datap() { 
      return reinterpret_cast<U * >(begin_);
    }
    template <typename U>
    U * datap(int pos) {
      return reinterpret_cast<U * >(begin_ + pos);
    }

    char & operator[] (size_t pos) {return begin_[pos];}
    char operator[] (size_t pos) const {return begin_[pos];}

    char & back() {return end_[-1];}
    char back() const {return end_[-1];}

    void clear() {end_ = begin_;}

    //
    // constructors
    //

    String() : begin_(0), end_(0), storage_end_(0) {}
    String(const char * s) {assign_only(s);}
    String(const char * s, unsigned size) {assign_only(s, size);}
    String(ParmStr s) {assign_only(s, s.size());}
    String(MutableString s) {assign_only(s.str, s.size);}
    String(const String & other) {assign_only(other.begin_, other.end_-other.begin_);}

    //
    // assign
    //

    void assign(const char * b, size_t size)
    {
      clear();
      if (size != 0) {
        reserve(size);
        memmove(begin_, b, size);
        end_   = begin_ + size;
      } 
    }
    void assign(const char * b) 
    {
      if (b) assign(b, strlen(b));
    }
    String & operator= (const char * s) {
      assign(s);
      return *this;
    }
    inline String & operator= (const PosibErr<const char *> & s);
    String & operator= (ParmStr s) {
      assign(s, s.size());
      return *this;
    }
    String & operator= (MutableString s) {
      assign(s.str, s.size);
      return *this;
    }
    String & operator= (const String & s) {
      assign(s.begin_, s.end_ - s.begin_);
      return *this;
    }
    /*inline*/ String & operator= (const PosibErr<String> & s);

    //
    // append
    //

    String & append(const void * str, unsigned int sz)
    {
      reserve(size() + sz);
      if (sz > 0) memcpy(end_, str, sz);
      end_ += sz;
      return *this;
    }
    String & append(const void * d, const void * e)
    {
      append(d, (const char *)e - (const char *)d);
      return *this;
    }
    String & append(String & str, unsigned int sz)
    {
      append(str.begin_, sz);
      return *this;
    }
    String & append(const char * str)
    {
      if (!end_) reserve_i();
      for (; *str && end_ != storage_end_ - 1; ++str, ++end_)
        *end_ = *str;
      if (end_ == storage_end_ - 1) append(str, strlen(str));
      return *this;
    }
    String & append(char c)
    {
      reserve(size() + 1);
      *end_ = c;
      ++end_;
      return *this;
    }

    String & operator+= (const char * s) {
      append(s);
      return *this;
    }
    String & operator+= (char c) {
      append(c);
      return *this;
    }
    String & operator+= (ParmStr s) {
      if (s.have_size())
        append(s, s.size());
      else
        append(s);
      return *this;
    }
    String & operator+= (MutableString s) {
      append(s.str, s.size);
      return *this;
    }
    String & operator+= (const String & s) {
      append(s.begin_, s.end_ - s.begin_);
      return *this;
    }

    //
    //
    //

    ~String() {if (begin_) free(begin_);}

    void swap(String & other) {
      std::swap(begin_, other.begin_);
      std::swap(end_, other.end_);
      std::swap(storage_end_, other.storage_end_);
    }

    //
    // 
    //

    int vprintf(const char * format, va_list ap);

    //
    //
    //

    void push_back(char c) {append(c);}

    void pop_back(size_t p = 1) {end_ -= p;}

    char * insert(size_t p, char c)
    {
      reserve(size() + 1);
      char * pos = begin_ + p;
      size_t to_move = end_ - pos;
      if (to_move) memmove(pos + 1, pos, to_move);
      *pos = c;
      ++end_;
      return pos;
    }
    char * insert(char * pos, char c) 
    {
      return insert(pos - begin_, c);
    }
    void insert(size_t p, const char * str, size_t sz)
    {
      reserve(size() + sz);
      char * pos = begin_ + p;
      size_t to_move = end_ - pos;
      if (to_move) memmove(pos + sz, pos, to_move);
      memcpy(pos, str, sz);
      end_ += sz;
    }
    void insert(char * pos, const char * f, const char * l) 
    {
      insert(pos - begin_, f, l - f);
    }

    char * erase(char * pos)
    {
      size_t to_move = end_ - pos - 1;
      if (to_move) memmove(pos, pos + 1, to_move);
      --end_;
      return pos;
    }
    char * erase(char * f, char * l)
    {
      if (l >= end_) {
        end_ = f < end_ ? f : end_;
      } else {
        size_t sz = l - f;
        memmove(f, f + sz, end_ - l);
        end_ -= sz;
      }
      return f;
    }
    void erase(size_t pos, size_t s)
    {
      erase(begin_ + pos, begin_ + pos + s);
    }

    //FIXME: Make this more efficient by rewriting the implementation
    //       to work with raw memory rather than using vector<char>
    template <typename Itr>
    void replace(iterator start, iterator stop, Itr rstart, Itr rstop) 
    {
      iterator i = erase(start,stop);
      insert(i, rstart, rstop);
    }

    void replace(size_t pos, size_t n, const char * with, size_t s)
    {
      replace(begin_ + pos, begin_ + pos + n, with, with + s);
    }
    void resize(size_t n)
    {
      reserve(n);
      end_ = begin_ + n;
    }
    void resize(size_t n, char c)
    {
      size_t old_size = size();
      reserve(n);
      end_ = begin_ + n;
      int diff = n - old_size;
      if (diff > 0) memset(begin_ + old_size, c, diff);
    }
    int alloc(int s) {
      int pos = size();
      resize(pos + s);
      return pos;
    }

    bool prefix(ParmStr str, size_t offset = 0) const
    {
      if (str.size() > size() - offset) return false;
      return memcmp(begin_ + offset, str.str(), str.size()) == 0;
    };
    bool suffix(ParmStr str) const
    {
      if (str.size() > size()) return false;
      return memcmp(end_ - str.size(), str.str(), str.size()) == 0;
    }

    // FIXME: Eventually remove
    static const size_t npos = INT_MAX;
    size_t find(char c, size_t pos = 0) const {
      char * res = (char *)memchr(begin_ + pos, c, size() - pos);
      if (res == 0) return npos;
      else return res - begin_;
    }
    size_t rfind(char c) const {
      for (int i = size() - 1; i >= 0; --i) {
        if (begin_[i] == c) return i;
      }
      return npos;
    }
    String substr(size_t pos = 0, size_t n = npos) const
    {
      if (n == npos)
        return String(begin_ + pos, size() - pos);
      else
        return String(begin_ + pos, n);
    }
    // END FIXME

    unsigned short & at16(unsigned int pos) 
      {return reinterpret_cast<unsigned short &>(operator[](pos));}
    unsigned int   & at32(unsigned int pos) 
      {return reinterpret_cast<unsigned int &>(operator[](pos));}

    void write (char c) {append(c);}
    void write (ParmStr str) {operator+=(str);}
    void write (const void * str, unsigned int sz) {append(str,sz);}


    String & operator << (ParmStr str) {
      append(str);
      return *this;
    }

    String & operator << (char c) {
      append(c);
      return *this;
    }
  };

  inline String operator+ (ParmStr lhs, ParmStr rhs)
  {
    String tmp;
    tmp.reserve(lhs.size() + rhs.size());
    tmp += lhs;
    tmp += rhs;
    return tmp;
  }

  inline bool operator== (const String & x, const String & y)
  {
    if (x.size() != y.size()) return false;
    if (x.size() == 0) return true;
    return memcmp(x.data(), y.data(), x.size()) == 0;
  }
  inline bool operator== (const String & x, const char * y)
  {
    return strcmp(x.c_str(), y) == 0;
  }
  inline bool operator== (const char * x, const String & y)
  {
    return strcmp(x, y.c_str()) == 0;
  }
  inline bool operator== (const String & x, ParmStr y)
  {
    if (y == 0) return x.size() == 0;
    return strcmp(x.c_str(), y) == 0;
  }
  inline bool operator== (ParmStr x, const String & y)
  {
    if (x == 0) return y.size() == 0;
    return strcmp(x, y.c_str()) == 0;
  }

  inline bool operator!= (const String & x, const String & y)
  {
    return !(x == y);
  }
  inline bool operator!= (const String & x, const char * y)
  {
    return strcmp(x.c_str(), y) != 0;
  }
  inline bool operator!= (const char * x, const String & y)
  {
    return strcmp(x, y.c_str()) != 0;
  }
  inline bool operator!= (const String & x, ParmStr y)
  {
    return !(x == y);
  }
  inline bool operator!= (ParmStr x, const String & y)
  {
    return !(x == y);
  }

  inline ParmString::ParmString(const String & s) : str_(s.c_str()), size_(s.size()) {}

  class StringIStream : public IStream {
    const char * in_str;
    char         delem;
  public:
    StringIStream(ParmStr s, char d = ';')
      : IStream(d), in_str(s) {}
    bool append_line(String & str, char c);
    bool read(void * data, unsigned int size);
  };

  template <> struct hash<String> : public HashString<String> {};

  inline bool IStream::getline(String & str, char c) 
  {
    str.clear(); 
    return append_line(str,c);
  }

  inline bool IStream::getline(String & str) 
  {
    str.clear(); 
    return append_line(str,delem);
  }

}

namespace std
{
  template<> inline void swap(acommon::String & x, acommon::String & y) {return x.swap(y);}
}

#endif
