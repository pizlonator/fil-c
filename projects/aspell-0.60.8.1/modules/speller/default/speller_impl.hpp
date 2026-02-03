// Aspell main C++ include file
// Copyright 1998-2000 by Kevin Atkinson under the terms of the LGPL.

#ifndef __aspeller_speller__
#define __aspeller_speller__

#include <vector>

#include "clone_ptr.hpp"
#include "copy_ptr.hpp"
#include "data.hpp"
#include "enumeration.hpp"
#include "speller.hpp"
#include "check_list.hpp"

using namespace acommon;

namespace acommon {
  class StringMap;
  class Config;
  class WordList;
}
// The speller class is responsible for keeping track of the
// dictionaries coming up with suggestions and the like. Its methods
// are NOT meant to be used my multiple threads and/or documents.

namespace aspeller {

  class Language;
  struct SensitiveCompare;
  class Suggest;

  enum SpecialId {main_id, personal_id, session_id, 
                  personal_repl_id, none_id};

  struct SpellerDict
  {
    Dict *            dict;
    bool              use_to_check;
    bool              use_to_suggest;
    bool              save_on_saveall;
    SpecialId         special_id;
    SpellerDict     * next;
    SpellerDict(Dict *);
    SpellerDict(Dict *, const Config &, SpecialId id = none_id);
    ~SpellerDict() {if (dict) dict->release();}
  };

  class SpellerImpl : public Speller
  {
  public:
    SpellerImpl(); // does not set anything up. 
    ~SpellerImpl();

    PosibErr<void> setup(Config *);

    void setup_tokenizer(Tokenizer *);

    //
    // Low level Word List Management methods
    //

  public:

    typedef Enumeration<Dict *> * WordLists;

    WordLists wordlists() const;
    int num_wordlists() const;

    const SpellerDict * locate (const Dict::Id &) const;

    //
    // Add a single dictionary that has not been previously added
    //
    PosibErr<void> add_dict(SpellerDict *);

    PosibErr<const WordList *> personal_word_list  () const;
    PosibErr<const WordList *> session_word_list   () const;
    PosibErr<const WordList *> main_word_list      () const;

    //
    // Language methods
    //
    
    char * to_lower(char *);

    const char * lang_name() const;

    const Language & lang() const {return *lang_;}

    //
    // Spelling methods
    //

    struct CompoundInfo {
      short count;
      short incorrect_count;
      CheckInfo * first_incorrect;      
      CompoundInfo() : count(0), incorrect_count(0), first_incorrect() {}
    };
  
    PosibErr<bool> check(char * word, char * word_end, /* it WILL modify word */
                         bool try_uppercase,
			 unsigned run_together_limit,
			 CheckInfo *, CheckInfo *, GuessInfo *, CompoundInfo * = NULL);

    PosibErr<bool> check(MutableString word) {
      guess_info.reset();
      return check(word.begin(), word.end(), false,
		   unconditional_run_together_ ? run_together_limit_ : 0,
		   check_inf, check_inf + 8, &guess_info);
    }
    PosibErr<bool> check(ParmString word)
    {
      size_t sz = word.size();
      std::vector<char> w(sz+1);
      memcpy(&*w.begin(), word.str(), sz+1);
      return check(MutableString(&w.front(), sz));
    }
    PosibErr<bool> check(const char * word) {return check(ParmString(word));}
    PosibErr<bool> check(const char * word, size_t sz)
    {
      std::vector<char> w(sz+1);
      memcpy(&*w.begin(), word, sz);
      w[sz] = '\0';
      return check(MutableString(&w.front(), sz));
    }

    CheckInfo * check_runtogether(char * word, char * word_end, /* it WILL modify word */
                                  bool try_uppercase,
                                  unsigned run_together_limit,
                                  CheckInfo *, CheckInfo *,
                                  GuessInfo *);
    
    bool check_single(char * word, /* it WILL modify word */
                      bool try_uppercase,
                      CheckInfo & ci, GuessInfo * gi);

    bool check_affix(ParmString word, CheckInfo & ci, GuessInfo * gi);

    bool check_simple(ParmString, WordEntry &);

    const CheckInfo * check_info() {
      if (check_inf[0].word.str)
        return check_inf;
      else if (guess_info.head)
        return guess_info.head;
      else
        return 0;
    }
    
    //
    // High level Word List management methods
    //

    PosibErr<void> add_to_personal(MutableString word);
    PosibErr<void> add_to_session(MutableString word);

    PosibErr<void> save_all_word_lists();

    PosibErr<void> clear_session();

    PosibErr<const WordList *> suggest(MutableString word);
    // the suggestion list and the elements in it are only 
    // valid until the next call to suggest.

    PosibErr<void> store_replacement(MutableString mis, 
				     MutableString cor);

    PosibErr<void> store_replacement(const String & mis, const String & cor,
				     bool memory);

    //
    // Private Stuff (from here to the end of the class)
    //

    class DictCollection;
    class ConfigNotifier;

  private:
    friend class ConfigNotifier;

    CachePtr<const Language>   lang_;
    CopyPtr<SensitiveCompare>  sensitive_compare_;
    //CopyPtr<DictCollection> wls_;
    ClonePtr<Suggest>       suggest_;
    ClonePtr<Suggest>       intr_suggest_;
    unsigned int            ignore_count;
    bool                    ignore_repl;
    String                  prev_mis_repl_;
    String                  prev_cor_repl_;

    void operator= (const SpellerImpl &other);
    SpellerImpl(const SpellerImpl &other);

    SpellerDict * dicts_;
    
    Dictionary       * personal_;
    Dictionary       * session_;
    ReplacementDict  * repl_;
    Dictionary       * main_;

  public:
    // these are public so that other classes and functions can use them, 
    // DO NOT USE

    const SensitiveCompare & sensitive_compare() const {return *sensitive_compare_;}

    //const DictCollection & data_set_collection() const {return *wls_;}

    PosibErr<void> set_check_lang(ParmString lang, ParmString lang_dir);
  
    double distance (const char *, const char *, 
		     const char *, const char *) const;

    CheckInfo check_inf[8];
    GuessInfo guess_info;

    SensitiveCompare s_cmp;
    SensitiveCompare s_cmp_begin;  // These (s_cmp_begin,middle,end)
    SensitiveCompare s_cmp_middle; // are used by the affix code.
    SensitiveCompare s_cmp_end;

    typedef Vector<const Dict *> WS;
    WS check_ws, affix_ws, suggest_ws, suggest_affix_ws;

    bool                    unconditional_run_together_;
    unsigned int            run_together_limit_;
    unsigned int            run_together_min_;

    unsigned run_together_limit() const {
      return unconditional_run_together_ ? run_together_limit_ : 0;
    }

    bool camel_case_;

    bool affix_info, affix_compress;

    bool have_repl;

    bool have_soundslike;

    bool invisible_soundslike, soundslike_root_only;

    bool fast_scan, fast_lookup;

    bool run_together;

  };

  struct LookupInfo {
    SpellerImpl * sp;
    enum Mode {Word, Guess, Clean, Soundslike, AlwaysTrue} mode;
    SpellerImpl::WS::const_iterator begin;
    SpellerImpl::WS::const_iterator end;
    inline LookupInfo(SpellerImpl * s, Mode m);
    // returns 0 if nothing found
    // 1 if a match is found
    // -1 if a word is found but affix doesn't match and "gi"
    int lookup (ParmString word, const SensitiveCompare * c, char aff, 
                WordEntry & o, GuessInfo * gi) const;
  };

  inline LookupInfo::LookupInfo(SpellerImpl * s, Mode m) 
    : sp(s), mode(m) 
  {
    switch (m) { 
    case Word: 
      begin = sp->affix_ws.begin(); 
      end = sp->affix_ws.end();
      return;
    case Guess:
      begin = sp->check_ws.begin(); 
      end = sp->check_ws.end(); 
      mode = Word; 
      return;
    case Clean:
    case Soundslike: 
      begin = sp->suggest_affix_ws.begin(); 
      end = sp->suggest_affix_ws.end(); 
      return;
    case AlwaysTrue: 
      return; 
    }
  }
}

#endif
