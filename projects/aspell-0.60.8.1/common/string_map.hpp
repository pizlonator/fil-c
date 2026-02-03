/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_STRING_MAP__HPP
#define ASPELL_STRING_MAP__HPP

#include "mutable_container.hpp"
#include "parm_string.hpp"
#include "posib_err.hpp"
#include "string_pair.hpp"
#include "hash.hpp"
#include "objstack.hpp"


namespace acommon {

class StringPairEnumeration;

using std::pair;
  
class StringMap : public MutableContainer {
public: // but don't use
  struct Parms {
    typedef StringPair Value;
    typedef const char * Key;
    const char * key(const Value & v) {return v.first;}
    static const bool is_multi = false;
    acommon::hash<const char *>  hash;
    bool equal(const char * x, const char * y) {return strcmp(x,y) == 0;}
  };
  typedef StringPair Value_;
  typedef HashTable<Parms> Lookup;
  typedef Lookup::iterator Iter_;
  typedef Lookup::const_iterator CIter_;
private:
  HashTable<Parms> lookup_;
  ObjStack buffer_;
  /*const */ char empty_str[1];

  void copy(const StringMap & other);
  
  // copy and destructor provided
public:
  PosibErr<void> clear() {lookup_.clear(); buffer_.reset(); return no_err;}
  
  StringMap() {empty_str[0] = '\0';}
  StringMap(const StringMap & other) {empty_str[0] = '\0'; copy(other);}
  StringMap & operator= (const StringMap & o) {clear(); copy(o); return *this;}
  ~StringMap() {}
  
  StringMap * clone() const {
    return new StringMap(*this);
  }
  void assign(const StringMap * other) {
    *this = *(const StringMap *)(other);
  }
  
  StringPairEnumeration * elements() const;
  
  // insert a new element.   Will NOT overwrite an existing entry.
  // returns false if the element already exists.
  bool insert(ParmStr key, ParmStr value) {
    pair<Iter_,bool> res = lookup_.insert(Value_(key,0));
    if (res.second) {
      res.first->first  = buffer_.dup(key);
      res.first->second = buffer_.dup(value);
      return true;
    } else {
      return false;
    }
  }
  PosibErr<bool> add(ParmStr key) {
    pair<Iter_,bool> res = lookup_.insert(Value_(key,0));
    if (res.second) {
      res.first->first  = buffer_.dup(key);
      res.first->second = empty_str;
      return true;
    } else {
      return false;
    }
  }
  // insert a new element. WILL overwrite an existing entry
  // always returns true
  bool replace(ParmStr key, ParmStr value) {
    pair<Iter_,bool> res = lookup_.insert(Value_(key,0));
    if (res.second) {
      res.first->first  = buffer_.dup(key);
      res.first->second = buffer_.dup(value);
    } else {
      res.first->second = buffer_.dup(value);
    }
    return true;
  }
  
  // removes an element.  Returns true if the element existed.
  PosibErr<bool> remove(ParmStr key) {return lookup_.erase(key);}
  
  // looks up an element.  Returns null if the element did not exist.
  // returns an empty string if the element exists but has a null value
  // otherwise returns the value
  const char * lookup(ParmStr key) const 
  {
    CIter_ i = lookup_.find(key);
    if (i == lookup_.end())
      return 0;
    else
      return i->second;
  }  
  
  bool have(ParmStr key) const {return lookup(key) != 0;}
  
  unsigned int size() const {return lookup_.size();}
  bool empty() const {return lookup_.empty();}

};

StringMap * new_string_map();


}

#endif /* ASPELL_STRING_MAP__HPP */
