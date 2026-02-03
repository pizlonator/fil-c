// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

// Aspell implementation header file.
// Applications that just use the Aspell library should not include 
// these files as they are subject to change.
// Aspell Modules MUST include some of the implementation files and
// spell checkers MAY include some of these files.
// If ANY of the implementation files are included you also link with
// libaspell-impl to protect you from changes in these files.

#ifndef ASPELL_SPELLER__HPP
#define ASPELL_SPELLER__HPP

#include "can_have_error.hpp"
#include "copy_ptr.hpp"
#include "clone_ptr.hpp"
#include "mutable_string.hpp"
#include "posib_err.hpp"
#include "parm_string.hpp"
#include "char_vector.hpp"

namespace acommon {

  typedef void * SpellerLtHandle;

  class Config;
  class WordList;
  class Convert;
  class Tokenizer;
  class Filter;
  class DocumentChecker;

  struct CheckInfo {
    const CheckInfo * next;
    // note: if 'incorrect' then word is a pointer into the string
    // passed into check and is not null terminated;
    // otherwise word is guaranteed to be null termanted
    struct Word {
      const char * str;
      unsigned len;
      ParmString pstr() const {return ParmString(str, len);}
      Word() {}
      Word(const char * str)
        : str(str), len(strlen(str)) {}
      Word(const char * str, unsigned len)
        : str(str), len(len) {}
      Word(ParmStr str)
        : str(str.str()), len(str.size()) {}
    };
    Word word; // generally the root
    short pre_strip_len;
    short pre_add_len;
    const char * pre_add;
    short suf_strip_len;
    short suf_add_len;
    const char * suf_add;
    short pre_flag;
    short suf_flag;
    bool guess;
    bool compound;
    bool incorrect;
  };

  class Speller : public CanHaveError
  {
  private:
    SpellerLtHandle lt_handle_;
    Speller(const Speller &);
    Speller & operator= (const Speller &);
  public:
    String temp_str_0;
    String temp_str_1;
    ClonePtr<Convert> to_internal_;
    ClonePtr<Convert> from_internal_;
  protected:
    CopyPtr<Config> config_;
    Speller(SpellerLtHandle h);
  public:
    SpellerLtHandle lt_handle() const {return lt_handle_;}

    Config * config() {return config_;}
    const Config * config() const {return config_;}

    // utility functions

    virtual char * to_lower(char *) = 0;

    // the setup class will take over for config
    virtual PosibErr<void> setup(Config *) = 0;

    // sets up the tokenizer class
    // should be called only after this class is setup
    virtual void setup_tokenizer(Tokenizer *) = 0;

    ////////////////////////////////////////////////////////////////
    // 
    // Strings from this point on are expected to be in the 
    // encoding specified by encoding()
    //

    virtual PosibErr<bool> check(MutableString) = 0;

    // these functions return information about the last word checked
    virtual const CheckInfo * check_info() = 0;
  
    virtual PosibErr<void> add_to_personal(MutableString) = 0;
    virtual PosibErr<void> add_to_session (MutableString) = 0;
    
    // because the word lists may potently have to convert from one
    // encoding to another the pointer returned by the enumeration is only
    // valid to the next call.

    virtual PosibErr<const WordList *> personal_word_list() const = 0;
    virtual PosibErr<const WordList *> session_word_list () const = 0;
    virtual PosibErr<const WordList *> main_word_list () const = 0;
  
    virtual PosibErr<void> save_all_word_lists() = 0;
  
    virtual PosibErr<void> clear_session() = 0;

    virtual PosibErr<const WordList *> suggest(MutableString) = 0;
    // return null on error
    // the word list returned by suggest is only valid until the next
    // call to suggest
  
    virtual PosibErr<void> store_replacement(MutableString, 
					     MutableString) = 0;

    virtual ~Speller();

  };


  // This function is current a hack to reload the filters in the
  // speller class.  I hope to eventually find a better way.
  PosibErr<void> reload_filters(Speller * m);


  PosibErr<Speller *> new_speller(Config * c);

}

#endif
