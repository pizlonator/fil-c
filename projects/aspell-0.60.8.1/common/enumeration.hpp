// Copyright (c) 2001
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

#ifndef __autil_enumeration__
#define __autil_enumeration__

#include "clone_ptr-t.hpp"

// An enumeration is an efficient way to iterate through elements much
// like a forward iterator.  The at_end method is a convince method
// as enumerations will return a null pointer or some other sort of
// special end state when they are at the end.
// Unlike an iterator iterating through x elements on a list can be 
// done in x function calls while an iterator will require 3*x.
// function calls.
// Example of emulator usage
//   const char * word;
//   while ( (word = elements->next()) != 0) { // one function call
//     cout << word << endl;
//   }
// And an iterator
//   iterator i = container->begin();
//   iterator end = container->end();
//   while (i != end) { // comparison, one function call
//     cout << *i << endl; // deref, total of two function calls
//     ++i;                // increment, total of three function calls.
//   }
// Normally all three calls are inline so it doesn't really matter
// but when the container type is not visible there are not inline
// and probably even virtual.
// If you really love iterators you can very easily wrap an enumeration 
// in a forward iterator.  

namespace acommon {

  template <typename Val>
  class Enumeration {
  public:
    typedef Val Value;
    typedef Enumeration Base;
    virtual Enumeration * clone() const = 0;
    virtual void assign(const Enumeration *) = 0;
    virtual Value next() = 0;
    virtual bool at_end() const = 0;
    virtual ~Enumeration() {}
  };

  template <class Parms, class Enum = Enumeration<typename Parms::Value> > 
  // Parms is expected to have the following members:
  //   typename Value
  //   typename Iterator;
  //   bool endf(Iterator)  
  //   Value end_state()
  //   Value deref(Iterator)
  class MakeEnumeration : public Enum {
  public:
    typedef typename Parms::Iterator Iterator;
    typedef typename Parms::Value    Value;
  private:
    Iterator  i_;
    Parms     parms_;
  public:

    MakeEnumeration(const Iterator i, const Parms & p = Parms()) 
      : i_(i), parms_(p) {}

    Enum * clone() const {
      return new MakeEnumeration(*this);
    }

    void assign (const Enum * other) {
      *this = *static_cast<const MakeEnumeration *>(other);
    }

    Value next() {
      if (parms_.endf(i_))
	return parms_.end_state();
      Value temp = parms_.deref(i_);
      ++i_;
      return temp;
    }

    bool at_end() const {
      return parms_.endf(i_);
    }
  };

  template <class Value>
  struct MakeAlwaysEndEnumerationParms {
    Value end_state() const {return Value();}
  };

  template <class Value>
  struct MakeAlwaysEndEnumerationParms<Value *> {
    Value * end_state() const {return 0;}
  };
  
  template <class Value> 
  class MakeAlwaysEndEnumeration : public Enumeration<Value> {
    MakeAlwaysEndEnumerationParms<Value> parms_;
  public:
    MakeAlwaysEndEnumeration * clone() const {
      return new MakeAlwaysEndEnumeration(*this);
    }
    void assign(const Enumeration<Value> * that) {
      *this = *static_cast<const MakeAlwaysEndEnumeration *>(that);
    }
    Value next() {return parms_.end_state();}
    bool at_end() const {return true;}
  };
}

#endif

