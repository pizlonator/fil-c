// Copyright 2004 by Kevin Atkinson under the terms of the LGPL

#ifndef ASPELLER_LANGUAGE__HPP
#define ASPELLER_LANGUAGE__HPP

#include "affix.hpp"
#include "cache.hpp"
#include "config.hpp"
#include "convert.hpp"
#include "phonetic.hpp"
#include "posib_err.hpp"
#include "stack_ptr.hpp"
#include "string.hpp"
#include "objstack.hpp"
#include "string_enumeration.hpp"

#include "iostream.hpp"

using namespace acommon;

namespace acommon {
  struct CheckInfo;

  struct ConfigConvKey : public ConvKey {
    Config::Value config_val;
    template <typename T>
    ConfigConvKey(const T & v) : config_val(v) {
      val = config_val.val;
      allow_ucs = config_val.secure;
    }
    ConfigConvKey & operator=(const ConfigConvKey & other) {
      config_val = other.config_val; 
      val = config_val.val;
      allow_ucs = config_val.secure;
      return *this;
    }
    void fix_encoding_str() {
      String buf;
      ::fix_encoding_str(val, buf);
      config_val.val.swap(buf);
      val = config_val.val;
    }
  private:
  };
}

namespace aspeller {

  struct SuggestRepl {
    const char * substr;
    const char * repl;
  };
  
  class SuggestReplEnumeration
  {
    const SuggestRepl * i_;
    const SuggestRepl * end_;
  public:
    SuggestReplEnumeration(const SuggestRepl * b, const SuggestRepl * e)
      : i_(b), end_(e) {}
    bool at_end() const {return i_ == end_;}
    const SuggestRepl * next() {
      if (i_ == end_) return 0;
      return i_++;
    }
  };

  // CharInfo

  typedef unsigned int CharInfo; // 6 bits

  static const CharInfo LOWER  = (1 << 0);
  static const CharInfo UPPER  = (1 << 1);
  static const CharInfo TITLE  = (1 << 2);
  static const CharInfo PLAIN  = (1 << 3);
  static const CharInfo LETTER = (1 << 4);
  static const CharInfo CLEAN  = (1 << 5);

  static const CharInfo CHAR_INFO_ALL = 0x3F;

  //

  //

  struct CompoundWord {
    const char * word;
    const char * sep;
    const char * rest;
    const char * end;
    bool empty() const {return word == end;}
    bool single() const {return rest == end;}
    unsigned word_len() const {return sep - word;}
    unsigned rest_offset() const {return rest - word;}
    unsigned rest_len() const {return end - rest;}
    CompoundWord()
      : word(), sep(), rest(), end() {}
    CompoundWord(const char * a, const char * b)
      : word(a), sep(b), rest(b), end(b) {}
    CompoundWord(const char * a, const char * b, const char * c)
      : word(a), sep(b), rest(b), end(c) {}
    CompoundWord(const char * a, const char * b, const char * c, const char * d)
      : word(a), sep(b), rest(c), end(d) {}
  };

  enum StoreAs {Stripped, Lower};

  class Language : public Cacheable {
  public:
    typedef const Config CacheConfig;
    typedef String       CacheKey;

    enum CharType {Unknown, WhiteSpace, Hyphen, Digit, 
                   NonLetter, Modifier, Letter};
    
    struct SpecialChar {
      bool begin;
      bool middle;
      bool end;
      bool any;
      SpecialChar() : begin(false), middle(false), end(false), any(false) {}
      SpecialChar(bool b, bool m, bool e) : begin(b), middle(m), end(e),
                                            any(b || m || e) {}
    };

  private:
    String   dir_;
    String   name_;
    String   charset_;
    String   charmap_;
    String   data_encoding_;

    ConvObj  mesg_conv_;
    ConvObj  to_utf8_;
    ConvObj  from_utf8_;

    unsigned char to_uchar(char c) const {return static_cast<unsigned char>(c);}

    SpecialChar special_[256];
    CharInfo      char_info_[256];
    char          to_lower_[256];
    char          to_upper_[256];
    char          to_title_[256];
    char          to_stripped_[256];
    char          to_plain_[256];
    int           to_uni_[256];
    CharType      char_type_[256];
    char          to_clean_[256];
    char          de_accent_[256];

    StoreAs       store_as_;

    String      soundslike_chars_;
    String      clean_chars_;

    bool have_soundslike_;
    bool have_repl_;

    StackPtr<Soundslike> soundslike_;
    StackPtr<AffixMgr>   affix_;
    StackPtr<Config>     lang_config_;

    StringBuffer buf_;
    Vector<SuggestRepl> repls_;

    Language(const Language &);
    void operator=(const Language &);

  public: // but don't use

    char          sl_first_[256];
    char          sl_rest_[256];

  public:

    Language() {}
    PosibErr<void> setup(const String & lang, const Config * config);
    PosibErr<void> set_lang_defaults(Config & config) const;

    const char * data_dir() const {return dir_.c_str();}
    const char * name() const {return name_.c_str();}
    const char * charmap() const {return charmap_.c_str();}
    const char * data_encoding() const {return data_encoding_.c_str();}

    const Convert * mesg_conv() const {return mesg_conv_.ptr;}
    const Convert * to_utf8() const {return to_utf8_.ptr;}
    const Convert * from_utf8() const {return from_utf8_.ptr;}

    int to_uni(char c) const {return to_uni_[to_uchar(c)];}

    //
    // case conversion
    //

    char to_upper(char c) const {return to_upper_[to_uchar(c)];}
    bool is_upper(char c) const {return to_upper(c) == c;}

    char to_lower(char c) const {return to_lower_[to_uchar(c)];}
    bool is_lower(char c) const {return to_lower(c) == c;}

    char to_title(char c) const {return to_title_[to_uchar(c)];}
    bool is_title(char c) const {return to_title(c) == c;}

    char * to_lower(char * res, const char * str) const {
      while (*str) *res++ = to_lower(*str++); *res = '\0'; return res;}
    char * to_upper(char * res, const char * str) const {
      while (*str) *res++ = to_upper(*str++); *res = '\0'; return res;}

    void to_lower(String & res, const char * str) const {
      res.clear(); while (*str) res += to_lower(*str++);}
    void to_upper(String & res, const char * str) const {
      res.clear(); while (*str) res += to_upper(*str++);}

    bool is_lower(const char * str) const {
      while (*str) {if (!is_lower(*str++)) return false;} return true;}
    bool is_upper(const char * str) const {
      while (*str) {if (!is_upper(*str++)) return false;} return true;}

    //
    //
    //

    char to_plain(char c) const {return to_plain_[to_uchar(c)];}

    char de_accent(char c) const {return de_accent_[to_uchar(c)];}
    
    SpecialChar special(char c) const {return special_[to_uchar(c)];}
  
    CharType char_type(char c) const {return char_type_[to_uchar(c)];}
    bool is_alpha(char c) const {return char_type(c) >  NonLetter;}

    CharInfo char_info(char c) const {return char_info_[to_uchar(c)];}

    //
    // stripped
    //

    char to_stripped(char c) const {return to_stripped_[to_uchar(c)];}

    // return a pointer to the END of the string
    char * to_stripped(char * res, const char * str) const {
      for (; *str; ++str) {
        char c = to_stripped(*str);
        if (c) *res++ = c;
      }
      *res = '\0';
      return res;
    }
    void to_stripped(String & res, const char * str) const {
      res.clear();
      for (; *str; ++str) {
        char c = to_stripped(*str);
        if (c) res += c;
      }
    }

    bool is_stripped(char c) const {return to_stripped(c) == c;}

    bool is_stripped(const char * str) const {
      while (*str) {if (!is_stripped(*str++)) return false;} return true;}

    //
    // Clean
    //
    // The "clean" form is how words are indixed in the dictionary.
    // It will at very least convert the word to lower case.  It may
    // also strip accents and non-letters.
    //

    char to_clean(char c) const {return to_clean_[to_uchar(c)];}

    char * to_clean(char * res, const char * str) const {
      for (; *str; ++str) {
        char c = to_clean(*str);
        if (c) *res++ = c;
      }
      *res = '\0';
      return res;
    }
    void to_clean(String & res, const char * str) const {
      res.clear();
      for (; *str; ++str) {
        char c = to_clean(*str);
        if (c) res += c;
      }
    }

    bool is_clean(char c) const {return to_clean(c) == c;}

    bool is_clean(const char * str) const {
      while (*str) {if (!is_clean(*str++)) return false;} return true;}

    bool is_clean_wi(WordInfo wi) const {
      return false;
      //return wi & CASE_PATTEN == AllLower && 
    }


    const char * clean_chars() const {return clean_chars_.c_str();}

    //
    // Soundslike
    // 
  
    bool have_soundslike() const {return have_soundslike_;}
    
    const char * soundslike_name() const {return soundslike_->name();}
    const char * soundslike_version() const {return soundslike_->version();}

    void to_soundslike(String & res, ParmStr word) const {
      res.resize(word.size());
      char * e = soundslike_->to_soundslike(res.data(), word.str(), word.size());
      res.resize(e - res.data());
    }
    
    // returns a pointer to the END of the string
    char * to_soundslike(char * res, const char * str, int len = -1) const { 
      return soundslike_->to_soundslike(res,str,len);
    }

    char * to_soundslike(char * res, const char * str, int len, WordInfo wi) const {
      if (!have_soundslike_ && (wi & ALL_CLEAN)) return 0;
      else return soundslike_->to_soundslike(res,str,len);
    }

    const char * soundslike_chars() const {return soundslike_chars_.c_str();}

    //
    // Affix compression methods
    //

    const AffixMgr * affix() const {return affix_;}

    bool have_affix() const {return affix_;}

    void munch(ParmStr word, GuessInfo * cl, bool cross = true) const {
      if (affix_)
        affix_->munch(word, cl, cross);
    }
      
    WordAff * expand(ParmStr word, ParmStr aff, 
                     ObjStack & buf, int limit = INT_MAX) const {
      if (affix_)
        return affix_->expand(word, aff, buf, limit);
      else
        return fake_expand(word, aff, buf);
    }
    WordAff * fake_expand(ParmStr word, ParmStr aff, ObjStack & buf) const;

    //
    // Repl
    //

    bool have_repl() const {return have_repl_;}

    SuggestReplEnumeration * repl() const {
      return new SuggestReplEnumeration(repls_.pbegin(), repls_.pend());}
    
    //
    //
    //

    WordInfo get_word_info(ParmStr str) const;
    
    //
    // fix_case
    //

    CasePattern case_pattern(ParmStr str) const;

    CasePattern case_pattern(const char * str, unsigned size) const;

    void fix_case(CasePattern case_pattern, char * str)
    {
      if (!str[0]) return;
      if (case_pattern == AllUpper) to_upper(str,str);
      else if (case_pattern == FirstUpper) *str = to_title(*str);
    }
    void fix_case(CasePattern case_pattern, 
                  char * res, const char * str) const;
    const char * fix_case(CasePattern case_pattern, 
                          const char * str, String & buf) const;

    //
    //
    //

    CompoundWord split_word(const char * str, unsigned size, bool camel_case) const;

    //
    // for cache
    //

    static inline PosibErr<Language *> get_new(const String & lang, const Config * config) {
      StackPtr<Language> l(new Language());
      RET_ON_ERR(l->setup(lang, config));
      return l.release();
    }

    bool cache_key_eq(const String & l) const  {return name_ == l;}
  };

  typedef Language LangImpl;

  struct MsgConv : public ConvP
  {
    MsgConv(const Language * l) : ConvP(l->mesg_conv()) {}
    MsgConv(const Language & l) : ConvP(l.mesg_conv()) {}
  };

  struct InsensitiveCompare {
    // compares to strings without regards to casing or special characters
    const Language * lang;
    InsensitiveCompare(const Language * l = 0) : lang(l) {}
    operator bool () const {return lang;}
    int operator() (const char * a, const char * b) const
    { 
      char x, y;
      for (;;)
      {
        while (x = lang->to_clean(*a++), !x);
        while (y = lang->to_clean(*b++), !y);
        if (x == 0x10 || y == 0x10 || x != y) break;
      }
      return static_cast<unsigned char>(x) - static_cast<unsigned char>(y);
    }
  };

  struct InsensitiveEqual {
    InsensitiveCompare cmp;
    InsensitiveEqual(const Language * l = 0) : cmp(l) {}
    bool operator() (const char * a, const char * b) const
    {
      return cmp(a,b) == 0;
    }
  };
  
  template <typename HASH_INT = size_t>
  struct InsensitiveHash {
    // hashes a string without regards to casing or special begin
    // or end characters
    const Language * lang;
    InsensitiveHash() {}
    InsensitiveHash(const Language * l)
	: lang(l) {}
    HASH_INT operator() (const char * s) const
    {
      HASH_INT h = 0;
      for (;;) {
	if (*s == 0) break;
        unsigned char c = lang->to_clean(*s++);
	if (c) h=5*h + c;
      }
      return h;
    }
  };

  struct SensitiveCompare {
    const Language * lang;
    bool case_insensitive;
    bool ignore_accents; // unused
    bool begin; // if not begin we are checking the end of the word
    bool end;   // if not end we are checking the beginning of the word
                // if both false we are checking the middle of a word
    SensitiveCompare(const Language * l = 0) 
      : lang(l), case_insensitive(false), ignore_accents(false),
        begin(true), end(true) {}
    bool operator() (const char * word, const char * inlist) const;
  };

  struct CleanAffix {
    const Language * lang;
    OStream * log;
    MsgConv msgconv1;
    MsgConv msgconv2;
    CleanAffix(const Language * lang0, OStream * log0);
    char * operator() (ParmStr word, char * aff);
  };

  class WordListIterator
  {
  public:
    struct Value {
      SimpleString word;
      SimpleString aff;
    };
    WordListIterator(StringEnumeration * in,
                     const Language * lang,
                     OStream * log);
    // init may set "norm-strict" to true which is why it is not const
    PosibErr<void> init (Config & config);
    // init_plain initialized the iterator to read in a plain word
    // list without any affix flags, for simplicity it will expect the
    // input to be utf-8.  It will also assume clean the words unless
    // the `clean-words` option is explicitly specified.  Like init it
    // may set "norm-strict" to true which is why it is not const
    PosibErr<void> init_plain (Config & config);
    const Value & operator*() const {return val;}
    const Value * operator-> () const {return &val;}
    PosibErr<bool> adv();
  private:
    bool have_affix;
    bool validate_words;
    bool validate_affixes;
    bool clean_words;
    bool skip_invalid_words;
    bool clean_affixes;
    StringEnumeration * in;
    const Language * lang;
    ConvEC iconv;
    OStream * log;
    Value val;
    String data;
    const char * orig;
    char * str;
    char * str_end;
    CleanAffix clean_affix;
  };

  String get_stripped_chars(const Language & l);

  String get_clean_chars(const Language & l);
  
  PosibErr<void> check_if_sane(const Language & l, ParmStr word);
  PosibErr<void> check_if_valid(const Language & l, ParmStr word);
  PosibErr<void> validate_affix(const Language & l, ParmStr word, ParmStr aff);

  bool find_language(Config & c);

  PosibErr<Language *> new_language(const Config &, ParmStr lang = 0);

  PosibErr<void> open_affix_file(const Config &, FStream & o);
}


#endif
