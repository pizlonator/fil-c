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

#ifndef __autil_hash_t_hh__
#define __autil_hash_t_hh__

#include <math.h>

#include "vector_hash.hpp"
#include "primes.hpp"

namespace aspeller {

  template <class Parms>
  VectorHashTable<Parms>::FindIterator
  ::FindIterator(const HashTable * ht, const key_type & k)
    : vector(&ht->vector())
    , parms(&ht->parms())
    , key(k)
    , i(ht->hash1(k))
    , hash2(ht->hash2(k))
  {
    if (!parms->is_nonexistent((*vector)[i])
	&& !parms->equal(parms->key((*vector)[i]), key))
      adv();
  }

  template <class Parms>
  void VectorHashTable<Parms>::FindIterator::adv() {
    do {
      i = (i + hash2) % vector->size();
    } while (!parms->is_nonexistent((*vector)[i])
	     && !parms->equal(parms->key((*vector)[i]), key));
  }

  template<class Parms>
  void VectorHashTable<Parms>::nonexistent_vector() {
    vector_iterator vector_end = vector_.end();
    for (vector_iterator i = vector_.begin(); i != vector_end; ++i) {
      parms_.make_nonexistent(*i);
    }
  }
  
  template<class Parms>
  std::pair<typename VectorHashTable<Parms>::iterator, bool> 
  VectorHashTable<Parms>::insert(const value_type & d) 
  {
    MutableFindIterator j(this, parms_.key(d));
    vector_iterator i = vector_.begin() + j.i;
    if (!parms_.is_multi && !j.at_end())
      return std::pair<iterator,bool>(iterator(i,this),false);
    if (load_factor() > .92) {
      resize(bucket_count()*2);
      return insert(d);
    } 
    if (parms_.is_multi) 
      while(!j.at_end()) j.adv();
    *(vector_.begin() + j.i) = d;
    ++size_;
    return std::pair<iterator,bool>(iterator(i,this),true);
  }

  template<class Parms>
  bool VectorHashTable<Parms>::have(const key_type & key) const 
  {
    return !ConstFindIterator(this,key).at_end();
  }

  template<class Parms>
  typename VectorHashTable<Parms>::iterator 
  VectorHashTable<Parms>::find(const key_type & key) 
  {
    MutableFindIterator i(this, key);
    if (!i.at_end()) 
      return iterator(vector_.begin() + i.i, this);
    else
      return end();
  }

  template<class Parms>
  typename VectorHashTable<Parms>::const_iterator 
  VectorHashTable<Parms>::find(const key_type & key) const {
    ConstFindIterator i(this, key);
    if (!i.at_end()) 
      return const_iterator(vector_.begin() + i.i, this);
    else
      return end();
  }

#if 0 // it currently doesn't work needs fixing

  template<class Parms>
  VectorHashTable<Parms>::size_type 
  VectorHashTable<Parms>::erase(const key_type & key) {
    MutableFindIterator i(this, key);
    if (!i.at_end()) {
      erase(iterator(vector_.begin() + i.i, this));
      return 1;
    } else {
      return 0;
    }
  }

  template<class Parms>
  void VectorHashTable<Parms>::erase(const iterator & p) {
    vector_iterator pos = p.vector_iterator();
    parms_.make_nonexistent(*pos);
    vector_iterator vector_end = vector_.end();

    vector_iterator e = pos;
    do {
      ++e;
      if (e == vector_end) e = vector_.begin();
    } while (!parms_.is_nonexistent(*e));

    vector_iterator should_be;
    while (pos != e) {
      if (parms_.is_nonexistent(*pos)) {
	should_be = find_item_v(*pos);
	if (parms_.is_nonexistent(*should_be)) {
	  *should_be = *pos;
	  parms_.make_nonexistent(*pos);
	}
      }
      ++pos;
      if (pos == vector_end) pos = vector_.begin();
    }
    --size_;
  }
#endif

  template<class Parms>
  void VectorHashTable<Parms>::swap(VectorHashTable<Parms> &other) {
    vector_.swap(other.vector_);
    size_type temp = size_;
    size_ = other.size_;
    other.size_ = temp;
  }

  template<class Parms>
  VectorHashTable<Parms>::VectorHashTable(size_type i, const Parms & p)
    : parms_(p), size_(0) 
  {
    if (i <= 19) {
      i = 19;
    } else {
      size_type j =  ((i - 3)/4)*4 + 3;
      if (j == i) 
	i = j;
      else
	i = j + 4;
      Primes p(static_cast<size_type>(sqrt(static_cast<double>(i))+2));
      for (;;) {
	if (i > p.max_num())
	  p.resize(static_cast<size_type>(sqrt(static_cast<double>(i))+2));
	if (p.is_prime(i) && p.is_prime(i-2))
	  break;
	i += 4;
      }
    }
    vector_.resize(i);
    nonexistent_vector();
  }

  template<class Parms>
  void VectorHashTable<Parms>::resize(size_type i) {
    VectorHashTable temp(i,parms_);
    iterator e = end();
    for (iterator i = begin(); i != e; ++i)
      temp.insert(*i);
    swap(temp);
  }

  template<class Parms>
  void VectorHashTable<Parms>::recalc_size() {
    size_ = 0;
    for (iterator i = begin(); i != this->e; ++i, ++this->_size);
  }

}

#endif
