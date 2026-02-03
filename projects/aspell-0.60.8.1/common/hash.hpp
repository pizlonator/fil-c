// Copyright (c) 2001,2011
// Kevin Atkinson
//
// Permission to use, copy, modify, distribute and sell this software
// and its documentation for any purpose is hereby granted without
// fee, provided that the above copyright notice appear in all copies
// and that both that copyright notice and this permission notice
// appear in supporting documentation.  No representations about the
// suitability of this software for any purpose.  It is provided "as
// is" without express or implied warranty.

#ifndef autil__hash_hh
#define autil__hash_hh

#include <utility>
#include <functional>

#include "settings.h"

#include "hash_fun.hpp"
#include "block_slist.hpp"

// This file provided implementation for hash_set, hash_multiset, hash_map
//   and hash_multimap which are very similar to SGI STL's implementation
//   with a few notable exceptions.  The main one is that while 
//   const_iterator is never invalided until the actual element is removed
//   iterator is invalided my most all non-const member functions.  This
//   is to simply the implementation slightly and allow the removal 
//   of an element in guaranteed constant time when a non-const iterator
//   is provided rather that normally constant time.

// All of the hash_* implementations are derived from the HashTable class

namespace acommon {

  // Parms is expected to have the following methods
  //   typename Value
  //   typename Key
  //   bool is_multi;
  //   Size hash(Key)
  //   bool equal(Key, Key)
  //   Key key(Value)

  template <class Value>
  class HT_ConstIterator;

  template <class Value>
  class HT_Iterator {
  public: // but don't use
    typedef typename BlockSList<Value>::Node Node;
    Node * * t;
    Node * * n;
    void adv() {while (*t == 0) ++t; n = t;}
    void inc() {n = &(*n)->next; if (*n == 0) {++t; adv();}}
    HT_Iterator(Node * * t0) : t(t0) {adv();}
    HT_Iterator(Node * * t0, Node * * n0) : t(t0), n(n0) {}
  public:
    HT_Iterator() : t(0), n(0) {}
    explicit HT_Iterator(const HT_ConstIterator<Value> & other);
    Value & operator*() const {return (*n)->data;}
    Value * operator->() const {return &(*n)->data;}
    HT_Iterator & operator++() {inc(); return *this;}
    HT_Iterator operator++(int) {HT_Iterator tmp(*this); inc(); return tmp;}
  };

  template <class Value>
  class HT_ConstIterator
  {
  public: // but don't use
    typedef typename BlockSList<Value>::Node Node;
    Node * * t;
    Node * n;
    void adv() {while (*t == 0) ++t; n = *t;}
    void inc() {n = n->next; if (n == 0) {++t; adv();}}
    HT_ConstIterator(Node * * t0) : t(t0) {adv();}
    HT_ConstIterator(Node * * t0, Node * n0) : t(t0), n(n0) {}
  public:
    HT_ConstIterator() : t(0), n(0) {}
    HT_ConstIterator(const HT_Iterator<Value> & other) : t(other.t), n(*other.n) {}
    Value & operator*() const {return n->data;}
    Value * operator->() const {return &n->data;}
    HT_ConstIterator & operator++() {inc(); return *this;}
    HT_ConstIterator operator++(int) {HT_ConstIterator tmp(*this); inc(); return tmp;}
  };
  
  template <typename P>
  class HashTable 
  {
  public:
    typedef P                     parms_type;
    typedef parms_type            Parms;

    typedef typename Parms::Value value_type;
    typedef value_type            Value;

    typedef typename Parms::Key  key_type;
    typedef key_type              Key;

    typedef unsigned int          size_type;
    typedef size_type             Size;

  public: // but don't use
    typedef BlockSList<Value>       NodePool;
    typedef typename NodePool::Node Node;

  private:
    typedef unsigned int      PrimeIndex;

    Size              size_;
    Node * *          table_;     // always one larger than table_size_;
    Node * *          table_end_; // always at true table_end - 1
    Size              table_size_;
    PrimeIndex        prime_index_;
    NodePool          node_pool_;
    Parms             parms_;

  public:

    typedef HT_Iterator<Value>      iterator;
    typedef HT_ConstIterator<Value> const_iterator;

  private:
    void del();
    void init(PrimeIndex);
    void copy(const HashTable & other);
    PrimeIndex next_largest(Size);
    void resize_i(PrimeIndex);
    void create_table(PrimeIndex);
    iterator find_i(const Key &, bool & have);
    std::pair<iterator, iterator> equal_range_i(const Key & to_find, int & c);
    
  public:
    
    HashTable() {init(0);}
    HashTable(const Parms & p) : parms_(p) {init(0);}
    HashTable(int size) : prime_index_(0) {init(next_largest(size));}
    HashTable(int size, const Parms & p) 
      : prime_index_(0), parms_(p) {init(next_largest(size));}

    HashTable(const HashTable & other) {copy(other);}
    HashTable& operator=(const HashTable & other) {del(); copy(other); return *this;}
    ~HashTable() {del();}
    iterator begin() {return iterator(table_);}
    iterator end()   {return iterator(table_end_, table_end_);}
    const_iterator begin() const {return const_iterator(table_);}
    const_iterator end()   const {return const_iterator(table_end_,*table_end_);}
    size_type size() const  {return size_;}
    bool      empty() const {return size_ + 1;}
    std::pair<iterator,bool> insert(const value_type &); 
    void erase(iterator);
    size_type erase(const key_type &);
    void clear() {del(), init(0);}
    iterator find(const key_type & to_find) {
      bool h; 
      iterator i = find_i(to_find,h);
      return h ? i : end();
    }
    bool have(const key_type & to_find) const {
      bool h; 
      const_cast<HashTable *>(this)->find_i(to_find,h);
      return h;
    }
    const_iterator find(const key_type & to_find) const {
      return const_cast<HashTable *>(this)->find(to_find);
    }

    std::pair<iterator,iterator> equal_range(const key_type & to_find) 
    {
      int irrelevant;
      return equal_range_i(to_find, irrelevant);
    }

    std::pair<const_iterator,const_iterator> 
    equal_range(const key_type & to_find) const
    {
      int irrelevant;
      std::pair<iterator,iterator> range 
	= const_cast<HashTable *>(this)->equal_range_i(to_find, irrelevant);
      return std::pair<const_iterator,const_iterator>
	(range.first,range.second);
    }
        
    void resize(Size s) {resize_i(next_largest(s));}

    //other niceties: swap, copy, equal

  };

  template <class V>
  inline HT_Iterator<V>::HT_Iterator(const HT_ConstIterator<V> & other)
    : t(other.t), n(other.t)
  {
    while (*n != other.n) n = &(*n)->next;
  }
  
  template <class V>
  inline bool operator== (HT_Iterator<V> rhs,
			  HT_Iterator<V> lhs) 
  {
    return rhs.n == lhs.n;
  }

  template <class V>
  inline bool operator== (HT_ConstIterator<V> rhs,
			  HT_Iterator<V> lhs) 
  {
    return rhs.n == *lhs.n;
  }

  template <class V>
  inline bool operator== (HT_Iterator<V> rhs,
			  HT_ConstIterator<V> lhs) 
  {
    return *rhs.n == lhs.n;
  }

  template <class V>
  inline bool operator== (HT_ConstIterator<V> rhs, 
			  HT_ConstIterator<V> lhs) 
  {
    return rhs.n == lhs.n;
  }

#ifndef REL_OPS_POLLUTION

  template <class V>
  inline bool operator!= (HT_Iterator<V> rhs,
			  HT_Iterator<V> lhs) 
  {
    return rhs.n != lhs.n;
  }

  template <class V>
  inline bool operator!= (HT_ConstIterator<V> rhs,
			  HT_Iterator<V> lhs) 
  {
    return rhs.n != *lhs.n;
  }

  template <class V>
  inline bool operator!= (HT_Iterator<V> rhs,
			  HT_ConstIterator<V> lhs) 
  {
    return *rhs.n != lhs.n;
  }

  template <class V>
  inline bool operator!= (HT_ConstIterator<V> rhs, 
			  HT_ConstIterator<V> lhs) 
  {
    return rhs.n != lhs.n;
  }

#endif

  template <typename K, typename HF, typename E, bool m>
  struct HashSetParms 
  {
    typedef K Value;
    typedef const K Key;
    static const bool is_multi = m;
    HF hash;
    E  equal;
    const K & key(const K & v) {return v;}
    HashSetParms(const HF & h = HF(), const E & e = E()) : hash(h), equal(e) {}
  };

  template <typename K, typename HF = hash<K>, typename E = std::equal_to<K> >
  class hash_set : public HashTable<HashSetParms<K,HF,E,false> >
  {
  public:
    typedef HashTable<HashSetParms<K,HF,E,false> > Base;
    typedef typename Base::size_type               size_type;
    typedef typename Base::Parms                   Parms;
    hash_set(size_type s = 0, const HF & h = HF(), const E & e = E()) 
      : Base(s, Parms(h,e)) {}
  };

  template <typename K, typename HF = hash<K>, typename E = std::equal_to<K> >
  class hash_multiset : public HashTable<HashSetParms<K,HF,E,true> >
  {
  public:
    typedef HashTable<HashSetParms<K,HF,E,true> > Base;
    typedef typename Base::size_type              size_type;
    typedef typename Base::Parms                  Parms;
    hash_multiset(size_type s = 0, const HF & h = HF(), const E & e = E()) 
      : Base(s, Parms(h,e)) {}
  };

  template <typename K, typename V, typename HF, typename E, bool m>
  struct HashMapParms 
  {
    typedef std::pair<const K,V> Value;
    typedef const K         Key;
    static const bool is_multi = m;
    HF hash;
    E  equal;
    const K & key(const Value & v) {return v.first;}
    HashMapParms() {}
    HashMapParms(const HF & h) : hash(h) {}
    HashMapParms(const HF & h, const E & e) : hash(h), equal(e) {}
  };

  template <typename K, typename V, typename HF = hash<K>, typename E = std::equal_to<K> >
  class hash_map : public HashTable<HashMapParms<K,V,HF,E,false> >
  {
  public:
    typedef V         data_type;
    typedef data_type Data;
    
    typedef HashTable<HashMapParms<K,V,HF,E,false> > Base;
    typedef typename Base::size_type                 size_type;
    typedef typename Base::key_type                  key_type;
    typedef typename Base::value_type                value_type;
    typedef typename Base::Parms                     Parms;

    hash_map(size_type s = 0, const HF & h = HF(), const E & e = E()) 
      : Base(s, Parms(h,e)) {}
    data_type & operator[](const key_type & k) 
    {
      return (*((this->insert(value_type(k, data_type()))).first)).second;
    }
  };

  template <typename K, typename V, typename HF = hash<K>, typename E = std::equal_to<K> >
  class hash_multimap : public HashTable<HashMapParms<K,V,HF,E,true> >
  {
  public:
    typedef HashTable<HashMapParms<K,V,HF,E,true> > Base;
    typedef typename Base::size_type                size_type;
    typedef typename Base::Parms                    Parms;
    hash_multimap(size_type s = 0, const HF & h = HF(), const E & e = E()) 
      : Base(s, Parms(h,e)) {}
  };
}

#endif
