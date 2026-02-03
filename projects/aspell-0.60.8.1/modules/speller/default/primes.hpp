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

#include <vector>

namespace aspeller {

  class Primes {
  private:
    typedef std::vector<bool> Data;
    Data data;
  public:
    typedef Data::size_type size_type;
    typedef Data::size_type value_type;
  
    Primes() {}
    Primes(size_type s) {resize(s);}
    size_type size() const {return data.size();}
    void resize(size_type s);
    bool operator[] (size_type pos) const {return data[pos];}
    bool is_prime(size_type n) const;
    size_type max_num() const {return (size()-1)*(size()-1);}
    
    //
    // Iterators
    //

    class const_reverse_iterator;
    class const_iterator {
      friend class Primes;
      friend class const_reverse_iterator;
    protected:
      size_type     pos;
      const Primes* data;
      const_iterator(const Primes *d, size_type p) {data = d; pos = p;}
    public:
      const_iterator() {}
      size_type operator* () {return pos;}
      const_iterator& operator++ () {
	size_type size = data->size();
	if (pos != size)
	  do {++pos;} while (pos != size && !(*data)[pos]);
	return *this;
      }
      const_iterator operator++ (int) {
	const_iterator temp = *this;
	operator++();
	return temp;
      }
      const_iterator& operator-- () {
	if (pos != 0)
	  do {--pos;} while (pos != 0 && !(*data)[pos]);
	return *this;
      }
      const_iterator operator-- (int) {
	const_iterator temp = *this;
	operator--();
	return temp;
      }
      const_iterator& jump(size_type p) {
	pos = p;
	if (!(*data)[pos])
	  operator++();
	return *this;
      }

      inline friend bool operator == (const const_iterator &rhs,
				      const const_iterator &lhs) {
	return rhs.data == lhs.data && rhs.pos == lhs.pos;
      }
    };
    typedef const_iterator iterator;

    class const_reverse_iterator : private const_iterator {
      friend class Primes;
    protected:
      const_reverse_iterator(const Primes *d, size_type p) {data = d; pos = p;}
    public:
      const_reverse_iterator() {}
      const_reverse_iterator(const const_iterator &other) :
	const_iterator(other) {}
      size_type operator* () const {return pos;}
      const_reverse_iterator& operator++ () {const_iterator::operator--(); return *this;}
      const_reverse_iterator operator++ (int){return const_iterator::operator--(1);}
      const_reverse_iterator& operator-- () {const_iterator::operator++(); return *this;}
      const_reverse_iterator operator-- (int) {return const_iterator::operator++(1); return *this;}
    
      const_reverse_iterator& jump(size_type p) {
	pos = p;
	if (!(*data)[pos])
	  operator++();
	return *this;
      }

      inline friend bool operator == (const const_reverse_iterator &rhs,
				      const const_reverse_iterator &lhs) {
	return rhs.data == lhs.data && rhs.pos == lhs.pos;
      }
    };    
    typedef const_reverse_iterator reverse_iterator;

    typedef Data::const_iterator const_ra_iterator;
    typedef const_ra_iterator                ra_iterator;
    typedef Data::const_reverse_iterator const_reverse_ra_iterator;
    typedef const_reverse_ra_iterator                reverse_ra_iterator;

    const_iterator begin() const {return const_iterator(this, 2);}
    const_iterator end() const {return const_iterator(this, size());}
    const_iterator jump(size_type p) {return const_iterator(this,0).jump(p);}
    const_reverse_iterator rbegin() const {return ++const_reverse_iterator(this, size());}
    const_reverse_iterator rend() const {return const_reverse_iterator(this, 0);}
    const_reverse_iterator rjump(size_type p) {return const_reverse_iterator(this,0).jump(p);}

    const_ra_iterator ra_begin() const {return data.begin();}
    const_ra_iterator ra_end() const {return data.end();}
    const_reverse_ra_iterator r_ra_begin() const {return data.rbegin();}
    const_reverse_ra_iterator r_ra_end() const {return data.rend();}
  };

}
