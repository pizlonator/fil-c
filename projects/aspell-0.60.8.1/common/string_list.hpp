// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.
#ifndef STRING_LIST_HP_HEADER
#define STRING_LIST_HP_HEADER

#include "string.hpp"
#include "string_enumeration.hpp"
#include "mutable_container.hpp"
#include "posib_err.hpp"
#include <stdio.h>
#include <cstdio>

namespace acommon {

  struct StringListNode {
    // private data structure
    // default copy & destructor unsafe
    String           data;
    StringListNode * next;
    StringListNode(ParmStr str,  StringListNode * n = 0)
      : data(str), next(n) {
    }
  };

  class StringListEnumeration : public StringEnumeration {
    // default copy and destructor safe
  private:
    StringListNode * n_;
  public:
    StringEnumeration * clone() const;
    void assign(const StringEnumeration *);

    StringListEnumeration(StringListNode * n) : n_(n) {}
    const char * next() {
      const char * temp;
      if (n_ == 0) {
	temp = 0;
      } else {
	temp = n_->data.c_str();
	n_ = n_->next;
      }
      return temp;
    }
    bool at_end() const {
      return n_ == 0;
    }
  };


  class StringList : public MutableContainer {
    // copy and destructor provided
  private:
    StringListNode * first;

    StringListNode * * find (ParmStr str);
    void copy(const StringList &);
    void destroy();
  public:
    friend bool operator==(const StringList &, const StringList &);
    StringList() : first(0) {}
    StringList(const StringList & other) 
    {
      copy(other);
    }
    StringList & operator= (const StringList & other)
    {
      destroy();
      copy(other);
      return *this;
    }
    virtual ~StringList() 
    {
      destroy();
    }

    StringList * clone() const;
    void assign(const StringList *);

    PosibErr<bool> add(ParmStr);
    PosibErr<bool> remove(ParmStr);
    PosibErr<void> clear();

    StringEnumeration * elements() const;
    StringListEnumeration elements_obj() const 
    {
      return StringListEnumeration(first);
    }

    bool empty() const { return first == 0; }
    unsigned int size() const { abort(); return 0; }

  };

  StringList * new_string_list();

}
#endif
