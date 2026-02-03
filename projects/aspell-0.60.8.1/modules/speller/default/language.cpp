// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include "settings.h"

#include <vector>
#include <assert.h>

#include <iostream.hpp>

#include "asc_ctype.hpp"
#include "clone_ptr-t.hpp"
#include "config.hpp"
#include "enumeration.hpp"
#include "errors.hpp"
#include "file_data_util.hpp"
#include "fstream.hpp"
#include "language.hpp"
#include "string.hpp"
#include "cache-t.hpp"
#include "getdata.hpp"
#include "file_util.hpp"

#ifdef ENABLE_NLS
#  include <langinfo.h>
#endif

#include "gettext.h"

namespace aspeller {

  static const char TO_CHAR_TYPE[256] = {
    // 1  2  3  4  5  6  7  8  9  A  B  C  D  E  F 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 2
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 3
    0, 4, 0, 0, 3, 0, 0, 0, 2, 0, 0, 0, 6, 5, 0, 0, // 4
    0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, // 5
    0, 4, 0, 0, 3, 0, 0, 0, 2, 0, 0, 0, 6, 5, 0, 0, // 6
    0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, // 7
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // A
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // B
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // C
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // D
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // E
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // F
  };

  static const int FOR_CONFIG = 1;

  static const KeyInfo lang_config_keys[] = {
    {"charset",             KeyInfoString, "iso-8859-1", ""}
    , {"data-encoding",       KeyInfoString, "<charset>", ""}
    , {"name",                KeyInfoString, "", ""}
    , {"run-together",        KeyInfoBool,   "", "", 0, FOR_CONFIG}
    , {"run-together-limit",  KeyInfoInt,    "", "", 0, FOR_CONFIG}
    , {"run-together-min",    KeyInfoInt,    "", "", 0, FOR_CONFIG}
    , {"soundslike",          KeyInfoString, "none", ""}
    , {"special",             KeyInfoString, "", ""}
    , {"ignore-accents" ,     KeyInfoBool, "", "", 0, FOR_CONFIG}
    , {"invisible-soundslike",KeyInfoBool, "", "", 0, FOR_CONFIG}
    , {"keyboard",            KeyInfoString, "standard", "", 0, FOR_CONFIG} 
    , {"affix",               KeyInfoString, "none", ""}
    , {"affix-compress",      KeyInfoBool, "false", "", 0, FOR_CONFIG}
    , {"partially-expand",    KeyInfoBool, "false", "", 0, FOR_CONFIG}
    , {"affix-char",          KeyInfoString, "/", "", 0, FOR_CONFIG}
    , {"flag-char",           KeyInfoString, ":", "", 0, FOR_CONFIG}
    , {"repl-table",          KeyInfoString, "none", ""}
    , {"sug-split-char",      KeyInfoList, "", "", 0, FOR_CONFIG}
    , {"store-as",            KeyInfoString, "", ""}
    , {"try",                 KeyInfoString, "", ""}
    , {"normalize",           KeyInfoBool, "false", "", 0, FOR_CONFIG}
    , {"norm-required",       KeyInfoBool, "false", "", 0, FOR_CONFIG}
    , {"norm-form",           KeyInfoString, "nfc", "", 0, FOR_CONFIG}
  };

  static GlobalCache<Language> language_cache("language");

  PosibErr<void> Language::setup(const String & lang, const Config * config)
  {
    //
    // get_lang_info
    //

    String dir1,dir2,path;

    fill_data_dir(config, dir1, dir2);
    dir_ = find_file(path,dir1,dir2,lang,".dat");

    lang_config_ = 
      new Config("speller-lang",
                 lang_config_keys, 
                 lang_config_keys + sizeof(lang_config_keys)/sizeof(KeyInfo));
    Config & data = *lang_config_;

    {
      PosibErrBase pe = data.read_in_file(path);
      if (pe.has_err(cant_read_file)) {
	String mesg = pe.get_err()->mesg;
	mesg[0] = asc_tolower(mesg[0]);
	mesg = _("This is probably because: ") + mesg;
	return make_err(unknown_language, lang, mesg);
      } else if (pe.has_err())
	return pe;
    }

    if (!data.have("name"))
      return make_err(bad_file_format, path, _("The required field \"name\" is missing."));

    String buf;
    name_          = data.retrieve("name");
    charset_       = fix_encoding_str(data.retrieve("charset"), buf);
    charmap_       = charset_;

    ConfigConvKey d_enc = data.retrieve_value("data-encoding");
    d_enc.fix_encoding_str();
    data_encoding_ = d_enc.val;

    DataPair d;

    //
    // read header of cset data file
    //
  
    FStream char_data;
    String char_data_name;
    find_file(char_data_name,dir1,dir2,charset_,".cset");
    RET_ON_ERR(char_data.open(char_data_name, "r"));
    
    String temp;
    char * p;
    do {
      p = get_nb_line(char_data, temp);
      if (*p == '=') {
        ++p;
        while (asc_isspace(*p)) ++p;
        charmap_ = p;
      }
    } while (*p != '/');

    //
    // fill in tables
    //

    for (unsigned int i = 0; i != 256; ++i) {
      p = get_nb_line(char_data, temp);
      if (!p || strtoul(p, &p, 16) != i) 
        return make_err(bad_file_format, char_data_name);
      to_uni_[i] = strtol(p, &p, 16);
      while (asc_isspace(*p)) ++p;
      char_type_[i] = static_cast<CharType>(TO_CHAR_TYPE[to_uchar(*p++)]);
      while (asc_isspace(*p)) ++p;
      ++p; // display, ignored for now
      CharInfo inf = char_type_[i] >= Letter ? LETTER : 0;
      to_upper_[i] = static_cast<char>(strtol(p, &p, 16));
      inf |= to_uchar(to_upper_[i]) == i ? UPPER : 0;
      to_lower_[i] = static_cast<char>(strtol(p, &p, 16));
      inf |= to_uchar(to_lower_[i]) == i ? LOWER : 0;
      to_title_[i] = static_cast<char>(strtol(p, &p, 16));
      inf |= to_uchar(to_title_[i]) == i ? TITLE : 0;
      to_plain_[i] = static_cast<char>(strtol(p, &p, 16));
      inf |= to_uchar(to_plain_[i]) == i ? PLAIN : 0;
      inf |= to_uchar(to_plain_[i]) == 0 ? PLAIN : 0;
      sl_first_[i] = static_cast<char>(strtol(p, &p, 16));
      sl_rest_[i]  = static_cast<char>(strtol(p, &p, 16));
      char_info_[i] = inf;
    }

    for (unsigned int i = 0; i != 256; ++i) {
      de_accent_[i] = to_plain_[i] == 0 ? to_uchar(i) : to_plain_[i];
    }

    to_plain_[0] = 0x10; // to make things slightly easier
    to_plain_[1] = 0x10;

    for (unsigned int i = 0; i != 256; ++i) {
      to_stripped_[i] = to_plain_[(unsigned char)to_lower_[i]];
    }
    
    char_data.close();

    if (data.have("store-as"))
      buf = data.retrieve("store-as");
    else if (data.retrieve_bool("affix-compress"))
      buf = "lower";
    else
      buf = "stripped";
    char * clean_is;
    if (buf == "stripped") {
      store_as_ = Stripped;
      clean_is = to_stripped_;
    } else {
      store_as_ = Lower;
      clean_is = to_lower_;
    }

    for (unsigned i = 0; i != 256; ++i) {
      to_clean_[i] = char_type_[i] > NonLetter ? clean_is[i] : 0;
      if ((unsigned char)to_clean_[i] == i) char_info_[i] |= CLEAN;
    }

    to_clean_[0x00] = 0x10; // to make things slightly easier
    to_clean_[0x10] = 0x10;

    clean_chars_   = get_clean_chars(*this);

    //
    // determine which mapping to use
    //

    if (charmap_ != charset_) {
      if (file_exists(dir1 + charset_ + ".cmap") || 
          file_exists(dir2 + charset_ + ".cmap"))
      {
        charmap_ = charset_;
      } else if (data_encoding_ == charset_) {
        data_encoding_ = charmap_;
      }
    }
      
    //
    // set up conversions
    //
    {
#ifdef ENABLE_NLS
      const char * tmp = 0;
      tmp = bind_textdomain_codeset("aspell", 0);
#ifdef HAVE_LANGINFO_CODESET
      if (!tmp) tmp = nl_langinfo(CODESET);
#endif
      if (ascii_encoding(*config, tmp)) tmp = 0;
      if (tmp)
        RET_ON_ERR(mesg_conv_.setup(*config, charmap_, fix_encoding_str(tmp, buf), NormTo));
      else 
#endif
        RET_ON_ERR(mesg_conv_.setup(*config, charmap_, data_encoding_, NormTo));
      // no need to check for errors here since we know charmap_ is a
      // supported encoding
      RET_ON_ERR(to_utf8_.setup(*config, charmap_, "utf-8", NormTo));
      RET_ON_ERR(from_utf8_.setup(*config, "utf-8", charmap_, NormFrom));
    }
    
    Conv iconv;
    RET_ON_ERR(iconv.setup(*config, data_encoding_, charmap_, NormFrom));

    //
    // set up special
    //

    init(data.retrieve("special"), d, buf);
    while (split(d)) {
      char c = iconv(d.key)[0];
      split(d);
      special_[to_uchar(c)] = 
        SpecialChar (d.key[0] == '*',d.key[1] == '*', d.key[2] == '*');
    }

    //
    // prep phonetic code
    //

    {
      PosibErr<Soundslike *> pe = new_soundslike(data.retrieve("soundslike"),
                                                 iconv,
                                               this);
      if (pe.has_err()) return pe;
      soundslike_.reset(pe.data);
    }
    soundslike_chars_ = soundslike_->soundslike_chars();

    have_soundslike_ = strcmp(soundslike_->name(), "none") != 0;

    //
    // prep affix code
    //
    {
      PosibErr<AffixMgr *> pe = new_affix_mgr(data.retrieve("affix"), iconv, this);
      if (pe.has_err()) return pe;
      affix_.reset(pe.data);
    }

    //
    // fill repl tables (if any)
    //

    String repl = data.retrieve("repl-table");
    have_repl_ = false;
    if (repl != "none") {

      String repl_file;
      FStream REPL;
      find_file(repl_file, dir1, dir2, repl, "_repl", ".dat");
      RET_ON_ERR(REPL.open(repl_file, "r"));
      
      size_t num_repl = 0;
      while (getdata_pair(REPL, d, buf)) {
        ::to_lower(d.key);
        if (d.key == "rep") {
          num_repl = atoi(d.value); // FIXME make this more robust
          break;
        }
      }

      if (num_repl > 0)
        have_repl_ = true;

      for (size_t i = 0; i != num_repl; ++i) {
        bool res = getdata_pair(REPL, d, buf);
        assert(res); // FIXME
        ::to_lower(d.key);
        assert(d.key == "rep"); // FIXME
        split(d);
        SuggestRepl rep;
        rep.substr = buf_.dup(iconv(d.key));
        if (check_if_valid(*this, rep.substr).get_err()) 
          continue; // FIXME: This should probably be an error, but
                    // this may cause problems with compatibility with
                    // Myspell as these entries may make sense for
                    // Myspell (but obviously not for Aspell)
        to_clean((char *)rep.substr, rep.substr);
        rep.repl   = buf_.dup(iconv(d.value));
        if (check_if_valid(*this, rep.repl).get_err()) 
          continue; // FIXME: Ditto
        to_clean((char *)rep.repl, rep.repl);
        if (strcmp(rep.substr, rep.repl) == 0 || rep.substr[0] == '\0')
          continue; // FIXME: Ditto
        repls_.push_back(rep);
      }

    }
    return no_err;
  }

  PosibErr<void> Language::set_lang_defaults(Config & config) const
  {
    config.replace_internal("actual-lang", name());
    RET_ON_ERR(config.lang_config_merge(*lang_config_, FOR_CONFIG, data_encoding_));
    return no_err;
  }

  WordInfo Language::get_word_info(ParmStr str) const
  {
    CharInfo first = CHAR_INFO_ALL, all = CHAR_INFO_ALL;
    const char * p = str;
    while (*p && (first = char_info(*p++), all &= first, !(first & LETTER)));
    while (*p) all &= char_info(*p++);
    WordInfo res;
    if      (all & LOWER)   res = AllLower;
    else if (all & UPPER)   res = AllUpper;
    else if (first & TITLE) res = FirstUpper;
    else                    res = Other;
    if (all & PLAIN)  res |= ALL_PLAIN;
    if (all & CLEAN)  res |= ALL_CLEAN;
    return res;
  }
  
  CasePattern Language::case_pattern(ParmStr str) const  
  {
    CharInfo first = CHAR_INFO_ALL, all = CHAR_INFO_ALL;
    const char * p = str;
    while (*p && (first = char_info(*p++), all &= first, !(first & LETTER)));
    while (*p) all &= char_info(*p++);
    if      (all & LOWER)   return AllLower;
    else if (all & UPPER)   return AllUpper;
    else if (first & TITLE) return FirstUpper;
    else                    return Other;
  }

  CasePattern Language::case_pattern(const char * str, unsigned size) const  
  {
    CharInfo first = CHAR_INFO_ALL, all = CHAR_INFO_ALL;
    const char * p = str;
    const char * end = p + size;
    while (p < end && (first = char_info(*p++), all &= first, !(first & LETTER)));
    while (p < end) all &= char_info(*p++);
    if      (all & LOWER)   return AllLower;
    else if (all & UPPER)   return AllUpper;
    else if (first & TITLE) return FirstUpper;
    else                    return Other;
  }
  
  void Language::fix_case(CasePattern case_pattern,
                          char * res, const char * str) const 
  {
    if (!str[0]) return;
    if (case_pattern == AllUpper) {
      to_upper(res,str);
    } if (case_pattern == FirstUpper && is_lower(str[0])) {
      *res = to_title(str[0]);
      if (res == str) return;
      res++;
      str++;
      while (*str) *res++ = *str++;
      *res = '\0';
    } else {
      if (res == str) return;
      while (*str) *res++ = *str++;
      *res = '\0';
    }
  }

  const char * Language::fix_case(CasePattern case_pattern, const char * str,
                                  String & buf) const 
  {
    if (!str[0]) return str;
    if (case_pattern == AllUpper) {
      to_upper(buf,str);
      return buf.str();
    } if (case_pattern == FirstUpper && is_lower(str[0])) {
      buf.clear();
      buf += to_title(str[0]);
      str++;
      while (*str) buf += *str++;
      return buf.str();
    } else {
      return str;
    }
  }

  WordAff * Language::fake_expand(ParmStr word, ParmStr aff, 
                                  ObjStack & buf) const 
  {
    WordAff * cur = (WordAff *)buf.alloc_bottom(sizeof(WordAff));
    cur->word = buf.dup(word);
    cur->aff = (unsigned char *)buf.dup("");
    cur->next = 0;
    return cur;
  }

  CompoundWord Language::split_word(const char * word, unsigned len,
                                    bool camel_case) const
  {
    if (!camel_case || len <= 1)
      return CompoundWord(word, word + len);
    // len >= 2
    if (is_upper(word[0])) {
      if (is_lower(word[1])) {
        unsigned i = 2;
        while (i < len && is_lower(word[i]))
          ++i;
        return CompoundWord(word, word + i, word + len);
      }
      if (is_upper(word[1])) {
        unsigned i = 2;
        while (i < len && is_upper(word[i]))
          ++i;
        if (i == len)
          return CompoundWord(word, word + len);
        // The first upper case letter is assumed to be part of the next word
        return CompoundWord(word, word + i - 1, word + len);
      }
    } else if (is_lower(word[0])) {
      unsigned i = 1;
      while (i < len && is_lower(word[i]))
        ++i;
      return CompoundWord(word, word + i, word + len);
    }
    // this should't happen but just in case...
    return CompoundWord(word, word + len);
  }
  
  bool SensitiveCompare::operator() (const char * word0, 
				     const char * inlist0) const
  {
    assert(*word0 && *inlist0);
  try_again:
    const char * word = word0;
    const char * inlist = inlist0;

    if (!case_insensitive) {
      
      if (begin) {
        if (*word == *inlist || *word == lang->to_title(*inlist)) ++word, ++inlist;
        else                                                      goto try_upper;
      }
      while (*word && *inlist && *word == *inlist) ++word, ++inlist;
      if (*inlist) goto try_upper;
      if (end && lang->special(*word).end) ++word;
      if (*word) goto try_upper;
      return true;
    try_upper:
      word = word0;
      inlist = inlist0;
      while (*word && *inlist && *word == lang->to_upper(*inlist)) ++word, ++inlist;
      if (*inlist) goto fail;
      if (end && lang->special(*word).end) ++word;
      if (*word) goto fail;
      
    } else { // case_insensitive
      
      while (*word && *inlist && 
             lang->to_upper(*word) == lang->to_upper(*inlist)) ++word, ++inlist;
      if (*inlist) goto fail;
      if (end && lang->special(*word).end) ++word;
      if (*word) goto fail;
      
    }
    return true;

  fail:
    if (begin && lang->special(*word0).begin) {++word0; goto try_again;}
    return false;
  }

  static PosibErrBase invalid_word_e(const Language & l,
                                     ParmStr word,
                                     const char * msg,
                                     char chr = 0)
  {
    char m[200];
    if (chr) {
      // the "char *" cast is needed due to an incorrect "snprintf"
      //   declaration on some platforms.
      snprintf(m, 200, (char *)msg, MsgConv(l)(chr), l.to_uni(chr));
      msg = m;
    }
    return make_err(invalid_word, MsgConv(l)(word), msg);
  }

  PosibErr<void> check_if_sane(const Language & l, ParmStr word) {
    if (*word == '\0') 
      return invalid_word_e(l, word, _("Empty string."));
    return no_err;
  }

  PosibErr<void> check_if_valid(const Language & l, ParmStr word) {
    RET_ON_ERR(check_if_sane(l, word));
    const char * i = word;
    if (!l.is_alpha(*i)) {
      if (!l.special(*i).begin)
        return invalid_word_e(l, word, _("The character '%s' (U+%02X) may not appear at the beginning of a word."), *i);
      else if (!l.is_alpha(*(i+1)))
        return invalid_word_e(l, word, _("The character '%s' (U+%02X) must be followed by an alphabetic character."), *i);
      else if (!*(i+1))
        return invalid_word_e(l, word, _("Does not contain any alphabetic characters."));
    }
    for (;*(i+1) != '\0'; ++i) { 
      if (!l.is_alpha(*i)) {
        if (!l.special(*i).middle)
          return invalid_word_e(l, word, _("The character '%s' (U+%02X) may not appear in the middle of a word."), *i);
        else if (!l.is_alpha(*(i+1)))
          return invalid_word_e(l, word, _("The character '%s' (U+%02X) must be followed by an alphabetic character."), *i);
      }
    }
    if (!l.is_alpha(*i)) {
      if (*i == '\r')
        return invalid_word_e(l, word, _("The character '\\r' (U+0D) may not appear at the end of a word. " 
                                         "This probably means means that the file is using MS-DOS EOL instead of Unix EOL."), *i);
      if (!l.special(*i).end)
        return invalid_word_e(l, word, _("The character '%s' (U+%02X) may not appear at the end of a word."), *i);
    }
    return no_err;
  }

  PosibErr<void> validate_affix(const Language & l, ParmStr word, ParmStr aff)
  {
    for (const char * a = aff; *a; ++a) {
      CheckAffixRes res = l.affix()->check_affix(word, *a);
      if (res == InvalidAffix)
        return make_err(invalid_affix, MsgConv(l)(*a), MsgConv(l)(word));
      else if (res == InapplicableAffix)
        return make_err(inapplicable_affix, MsgConv(l)(*a), MsgConv(l)(word));
    }
    return no_err;
  }

  CleanAffix::CleanAffix(const Language * lang0, OStream * log0)
    : lang(lang0), log(log0), msgconv1(lang0), msgconv2(lang0)
  {
  }
  
  char * CleanAffix::operator()(ParmStr word, char * aff)
  {
    char * r = aff;
    for (const char * a = aff; *a; ++a) {
      CheckAffixRes res = lang->affix()->check_affix(word, *a);
      if (res == ValidAffix) {
        *r = *a;
        ++r;
      } else if (log) {
        const char * msg = res == InvalidAffix 
          ? _("Warning: Removing invalid affix '%s' from word %s.\n")
          : _("Warning: Removing inapplicable affix '%s' from word %s.\n");
        log->printf(msg, msgconv1(*a), msgconv2(word));
      }
    }
    *r = '\0';
    return r;
  }

  String get_stripped_chars(const Language & lang) {
    bool chars_set[256] = {0};
    String     chars_list;
    for (int i = 0; i != 256; ++i) 
    {
      char c = static_cast<char>(i);
	if (lang.is_alpha(c) || lang.special(c).any)
	  chars_set[static_cast<unsigned char>(lang.to_stripped(c))] = true;
    }
    for (int i = 1; i != 256; ++i) 
    {
      if (chars_set[i]) 
	chars_list += static_cast<char>(i);
    }
    return chars_list;
  }

  String get_clean_chars(const Language & lang) {
    bool chars_set[256] = {0};
    String     chars_list;
    for (int i = 0; i != 256; ++i) 
    {
      char c = static_cast<char>(i);
      if (lang.is_alpha(c) || lang.special(c).any) 
        chars_set[static_cast<unsigned char>(lang.to_clean(c))] = true;
    }
    for (int i = 1; i != 256; ++i) 
    {
      if (chars_set[i]) {
	chars_list += static_cast<char>(i);
      }
    }
    return chars_list;
  }

  PosibErr<Language *> new_language(const Config & config, ParmStr lang)
  {
    if (!lang)
      return get_cache_data(&language_cache, &config, config.retrieve("lang"));
    else
      return get_cache_data(&language_cache, &config, lang);
  }

  PosibErr<void> open_affix_file(const Config & c, FStream & f)
  {
    String lang = c.retrieve("lang");

    String dir1,dir2,path;
    fill_data_dir(&c, dir1, dir2);
    String dir = find_file(path,dir1,dir2,lang,".dat");

    String file;
    file += dir;
    file += '/';
    file += lang;
    file += "_affix.dat";
    
    RET_ON_ERR(f.open(file,"r"));

    return no_err;
  }

  bool find_language(Config & c)
  {
    String l_data = c.retrieve("lang");
    char * l = l_data.mstr();

    String dir1,dir2,path;
    fill_data_dir(&c, dir1, dir2);

    char * s = l + strlen(l);

    while (s > l) {
      find_file(path,dir1,dir2,l,".dat");
      if (file_exists(path)) {
        c.replace_internal("actual-lang", l);
        return true;
      }
      while (s > l && !(*s == '-' || *s == '_')) --s;
      *s = '\0';
    }
    return false;
  }

  WordListIterator::WordListIterator(StringEnumeration * in0,
                                   const Language * lang0,
                                   OStream * log0)
    : in(in0), lang(lang0), log(log0), val(), str(0), str_end(0),
      clean_affix(lang0, log0) {}

  PosibErr<void>  WordListIterator::init(Config & config)
  {
    if (!config.have("norm-strict"))
      config.replace("norm-strict", "true");
    have_affix = lang->have_affix();
    validate_words = config.retrieve_bool("validate-words");
    validate_affixes = config.retrieve_bool("validate-affixes");
    clean_words = config.retrieve_bool("clean-words");
    skip_invalid_words = config.retrieve_bool("skip-invalid-words");
    clean_affixes = config.retrieve_bool("clean-affixes");
    if (config.have("encoding")) {
      ConfigConvKey enc = config.retrieve_value("encoding");
      RET_ON_ERR(iconv.setup(config, enc, lang->charmap(),NormFrom));
    } else {
      RET_ON_ERR(iconv.setup(config, lang->data_encoding(), lang->charmap(), NormFrom));
    }
    return no_err;
  }

  PosibErr<void> WordListIterator::init_plain(Config & config)
  {
    if (!config.have("norm-strict"))
      config.replace("norm-strict", "true");
    have_affix = false;
    validate_words = config.retrieve_bool("validate-words");
    clean_words = true;
    if (config.have("clean-words"))
      clean_words = config.retrieve_bool("clean-words");
    skip_invalid_words = true;
    RET_ON_ERR(iconv.setup(config, "utf-8", lang->charmap(),NormFrom));
    return no_err;
  }
 
  PosibErr<bool> WordListIterator::adv() 
  {
  loop:
    if (!str) {
      orig = in->next();
      if (!orig) return false;
      if (!*orig) goto loop;
      PosibErr<const char *> pe = iconv(orig);
      if (pe.has_err()) {
        if (!skip_invalid_words) return pe;
        if (log) log->printf(_("Warning: %s Skipping string.\n"), pe.get_err()->mesg);
        else pe.ignore_err();
        goto loop;
      }
      if (pe.data == orig) {
        data = orig;
        data.ensure_null_end();
        str = data.pbegin();
        str_end = data.pend();
      } else {
        str = iconv.buf.pbegin();
        str_end = iconv.buf.pend();
      }
      char * aff = str_end;
      char * aff_end = str_end;
      if (have_affix) {
        aff = strchr(str, '/');
        if (aff == 0) {
          aff = str_end;
        } else {
          *aff = '\0';
          str_end = aff;
          ++aff;
        }
        if (validate_affixes) {
          if (clean_affixes)
            aff_end = clean_affix(str, aff);
          else
            RET_ON_ERR(validate_affix(*lang, str, aff));
        }
      }
      val.aff.str = aff;
      val.aff.size = aff_end - aff;
      if (!*aff && validate_words && clean_words) {
        char * s = str;
        while (s < str_end && !lang->is_alpha(*s) && !lang->special(*s).begin)
          *s++ = '\0';
        char * s2 = str_end - 1;
        while (s2 >= str && *s2 && !lang->is_alpha(*s2) && !lang->special(*s2).end)
          *s2-- = '\0';
      }
    }
    while (str < str_end) 
    {
      if (!*str) {++str; continue;}

      PosibErrBase pe2 = validate_words ? check_if_valid(*lang, str) : no_err;

      val.word.str = str;
      val.word.size = strlen(str);
      str += val.word.size + 1;

      if (!pe2.has_err() && val.word.size + (*val.aff ? val.aff.size + 1 : 0) > 240)
        pe2 = make_err(invalid_word, MsgConv(lang)(val.word),
                       _("The total length is larger than 240 characters."));

      if (!pe2.has_err()) return true;
      if (!skip_invalid_words) return pe2;
      if (log) log->printf(_("Warning: %s Skipping word.\n"), pe2.get_err()->mesg);
      else pe2.ignore_err();
    } 
    str = 0;
    goto loop;
  }
}
