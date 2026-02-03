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

#ifndef __aspeller_vector_hash_hh__
#define __aspeller_vector_hash_hh__

//#include <iterator>
//#include <utility>

#include "settings.h"
#undef REL_OPS_POLLUTION  // FIXME

namespace aspeller {

  //
  // This hash table is implemnted as a Open Address Hash Table
  // which uses a Vector like object to store its data.  So
  // it might even be considered an adapter
  //
  
  ////////////////////////////////////////////////////////
  //                                                    //
  //          Vector Hash Table Iterator               //
  //                                                    //
  ////////////////////////////////////////////////////////
  //
  // This is an internal object and should not be created
  // directly.  Instead use VectorHashTable::begin() and end()

  template <class Parms>
  // Parms is expected to have:
  //  typename HashTable;
  //  typename TableIter;
  //  typename Value;
  class VHTIterator 
  {
    template <class T>
    friend bool operator== (VHTIterator<T>, VHTIterator<T>);
#ifndef REL_OPS_POLLUTION
    template <class T>
    friend bool operator!= (VHTIterator<T>, VHTIterator<T>);
#endif
  public: //but don't use
    typedef typename Parms::TableIter       TableIter;
    typedef typename Parms::HashTable       HashTable;
    TableIter   pos;
    HashTable * hash_table; 
  public:
    //typedef std::bidirectional_iterator_tag             iterator_category;
    typedef typename Parms::Value                       value_type;
    // these cause problems for SUNPRO_CC
    //typedef typename std::iterator_traits<TableIter>::difference_type difference_type;
    //typedef typename std::iterator_traits<TableIter>::pointer         pointer;
    //typedef typename std::iterator_traits<TableIter>::reference       reference;

    //VHTIterator vector_iterator() const {return pos;}
  public:
    VHTIterator(TableIter p, HashTable *ht) : pos(p), hash_table(ht) {}
    VHTIterator(TableIter p, HashTable *ht, bool) 
      : pos(p), hash_table(ht) 
    {
      while (pos != hash_table->vector().end()
	     && hash_table->parms().is_nonexistent(*pos) )
	++pos;
    }
    
    value_type & operator * () const  {return *pos;}
    value_type * operator -> () const {return &*pos;}
    
    bool at_end() const {return pos == hash_table->vector().end();}
    
    VHTIterator& operator++ () {
      ++pos;
      for (;;) {
	if (pos == hash_table->vector().end()) break;
	if (!hash_table->parms().is_nonexistent(*pos)) break;
	++pos;
      }
      return *this;
    }
    VHTIterator operator++ (int) {
      VHTIterator temp = *this; 
      operator++();
      return temp;
    }

    VHTIterator& operator-- () {
      --pos;
      for (;;) {
	if (pos < hash_table->vector().begin()) break;
	if (!hash_table->parms().is_nonexistent_func(*pos)) break;
	--pos;
      }
      return *this;
    }
    VHTIterator operator-- (int) {
      VHTIterator temp = *this; 
      operator--();
      return temp;
    }
  };

  template <class Parms>
  inline 
  bool operator== (VHTIterator<Parms> rhs, VHTIterator<Parms> lhs)
  {
    return rhs.pos == lhs.pos;
  }

#ifndef REL_OPS_POLLUTION
  
  template <class Parms>
  inline
  bool operator!= (VHTIterator<Parms> rhs, VHTIterator<Parms> lhs)
  {
    return rhs.pos != lhs.pos;
  }

#endif
  
  ////////////////////////////////////////////////////////
  //                                                    //
  //                Vector Hash Table                   //
  //                                                    //
  ////////////////////////////////////////////////////////

  // Parms is expected to have the following methods
  //   typename Vector
  //   typedef Vector::value_type Value
  //   typedef Vector::size_type  Size
  //   typename Key
  //   bool is_multi;
  //   Size hash(Key)
  //   bool equal(Key, Key)
  //   Key key(Value)
  //   bool is_nonexistent(Value)
  //   void make_nonexistent(Value &)

  template <class Parms>
  class VectorHashTable {
    typedef typename Parms::Vector           Vector;
  public:
    typedef typename Parms::Vector           vector_type;
    typedef typename Vector::value_type      value_type;
    typedef typename Vector::size_type       size_type;
    typedef typename Vector::difference_type difference_type;

    typedef typename Vector::pointer         pointer;
    typedef typename Vector::reference       reference;
    typedef typename Vector::const_reference const_reference;

    typedef typename Parms::Key              key_type;
  public: // but don't use
    typedef VectorHashTable<Parms>           HashTable;
  private:
    Parms parms_;

  public:
    typedef Parms parms_type;
    const parms_type & parms() const {return parms_;}

  public:
    // These public functions are very dangerous and should be used with
    // great care as the modify the internal structure of the object
    Vector & vector()       {return vector_;}
    const Vector & vector() const {return vector_;}
    parms_type & parms() {return parms_;}
    void recalc_size();
    void set_size(size_type s) {size_  = s;} 

  private:
    Vector      vector_;
    size_type   size_;

  public: // but don't use
    typedef typename Vector::iterator       vector_iterator;
    typedef typename Vector::const_iterator const_vector_iterator;
  
  private:
    int hash1(const key_type &d) const {
      return parms_.hash(d) % bucket_count();
    }
    int hash2(const key_type &d) const {
      return 1 + (parms_.hash(d) % (bucket_count() - 2));
    }

    struct NonConstParms {
      typedef VectorHashTable HashTable;
      typedef vector_iterator TableIter;
      typedef value_type      Value;
    };
    struct ConstParms {
      typedef const VectorHashTable HashTable;
      typedef const_vector_iterator TableIter;
      typedef const value_type      Value;
    };
  public:
    typedef VHTIterator<NonConstParms> iterator;
    typedef VHTIterator<ConstParms>    const_iterator;

  private:
    void nonexistent_vector();
  
  public:
    VectorHashTable(size_type i, const Parms & p = Parms());

    VectorHashTable(const Parms & p = Parms())
      : parms_(p), vector_(19), size_(0) {nonexistent_vector();}
 
    iterator begin() {return iterator(vector_.begin(), this, 1);}
    iterator end()   {return iterator(vector_.end(), this);}
    const_iterator begin() const {return const_iterator(vector_.begin(), this, 1);}
    const_iterator end()   const {return const_iterator(vector_.end(), this);}

    std::pair<iterator, bool> insert(const value_type &);
    bool have(const key_type &) const;

    iterator find(const key_type&);
    const_iterator find(const key_type&) const;
  
    size_type erase(const key_type &key);
    void erase(const iterator &p);
  
    size_type size() const {return size_;}
    bool      empty() const {return !size_;}

    void swap(VectorHashTable &);
    void resize(size_type);
    size_type bucket_count() const {return vector_.size();}
    double load_factor() const {return static_cast<double>(size())/bucket_count();}

  private:
    class FindIterator
    {
    public: // but don't use
      const vector_type * vector;
      const Parms       * parms;
      key_type key;
      int i;
      int hash2;
      FindIterator() {}
      FindIterator(const HashTable * ht, const key_type & k);
    public:
      bool at_end() const {return parms->is_nonexistent((*vector)[i]);}
      void adv();
      FindIterator & operator ++() {adv(); return *this;}
    };
    friend class FindIterator;

  public:
    class ConstFindIterator : public FindIterator 
    {
    public:
      ConstFindIterator() {}
      ConstFindIterator(const HashTable * ht, const key_type & k) 
	: FindIterator(ht,k) {}
      const value_type & deref() const {return (*this->vector)[this->i];}
    };

    class MutableFindIterator : public FindIterator 
    {
    public:
      MutableFindIterator() {}
      MutableFindIterator(HashTable * ht, const key_type & k) 
	: FindIterator(ht,k) {}
      value_type & deref() const {
	return (*const_cast<vector_type *>(this->vector))[this->i];
      }
    };
    
    ConstFindIterator multi_find(const key_type & k) const {
      return ConstFindIterator(this, k);
    }
    
    MutableFindIterator multi_find(const key_type & k) {
      return MutableFindIterator(this, k);
    }
    
  };
}

#endif
