#include <string.h>
#include <assert.h>

#include "parm_string.hpp"
#include "string_map.hpp"
#include "string_pair.hpp"
#include "string_pair_enumeration.hpp"

#include "hash-t.hpp"

// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

namespace acommon {

  // needed for darwin
  template HashTable<StringMap::Parms>::iterator 
           HashTable<StringMap::Parms>::find_i(char const* const&, bool&);
  template std::pair<HashTable<StringMap::Parms>::iterator,bool>
           HashTable<StringMap::Parms>::insert(const StringPair &);
  template void HashTable<StringMap::Parms>::init(unsigned int);
  template void HashTable<StringMap::Parms>::del(void);
  template HashTable<StringMap::Parms>::size_type
           HashTable<StringMap::Parms>::erase(char const* const&);
  template void BlockSList<StringPair>::clear(void);

  void StringMap::copy(const StringMap & other)
  {
    lookup_ = other.lookup_;
    for (Iter_ i = lookup_.begin(); 
         !(i == lookup_.end());  // i != lookup_.end() causes problems
                                 // with gcc-2.95
         ++i)
    {
      i->first = buffer_.dup(i->first);
      i->second = buffer_.dup(i->second);
    }
  }
  

  class StringMapEnumeration : public StringPairEnumeration {
    StringMap::CIter_ i;
    StringMap::CIter_ end;
  public:
    StringMapEnumeration(StringMap::CIter_ i0, StringMap::CIter_ e0)
      : i(i0), end(e0) {}
    StringPairEnumeration * clone() const;
    void assign(const StringPairEnumeration *);
    bool at_end() const;
    StringPair next();
  };

  StringPairEnumeration * StringMapEnumeration::clone() const {
    return new StringMapEnumeration(*this);
  }

  void 
  StringMapEnumeration::assign
  (const StringPairEnumeration * other)
  {
    *this = *(const StringMapEnumeration *)(other);
  }

  bool StringMapEnumeration::at_end() const {
    return i == end;
  }

  StringPair StringMapEnumeration::next() {
    StringPair temp;
    if (i == end)
      return temp;
    temp = *i;
    ++i;
    return temp;
  }

  StringPairEnumeration * StringMap::elements() const {
    return new StringMapEnumeration(lookup_.begin(), lookup_.end());
  }

  StringMap * new_string_map() 
  {
    return new StringMap();
  }
}

