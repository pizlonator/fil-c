// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef ASPELLER_SUGGEST__HPP
#define ASPELLER_SUGGEST__HPP

#include "word_list.hpp"
#include "enumeration.hpp"
#include "parm_string.hpp"
#include "suggestions.hpp"

using namespace acommon;

namespace aspeller {

  class SpellerImpl;

  class SuggestionList : public WordList {
  public:
    typedef StringEnumeration            VirEmul;
    typedef Enumeration<VirEmul>         Emul;
    typedef const char *               Value;
    typedef unsigned int               Size;

    //virtual SuggestionList * clone() const = 0;
    //virtual void assign(const SuggestionList *) = 0;
    
    virtual bool empty() const = 0;
    virtual Size size() const = 0;
    virtual VirEmul * elements() const = 0;
    virtual ~SuggestionList() {}
  };

  class Suggest {
  public:
    virtual PosibErr<void> set_mode(ParmString) = 0;
    virtual SuggestionList & suggest(const char * word) = 0;
    virtual SuggestionsData & suggestions(const char * word) = 0;
    virtual ~Suggest() {}
  };

  PosibErr<Suggest *> new_default_suggest(SpellerImpl *);

}


#endif
