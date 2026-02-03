// This file is part of The New Aspell
// Copyright (C) 2000-2001,2011 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include <stdlib.h>
#include <typeinfo>

#include "clone_ptr-t.hpp"
#include "config.hpp"
#include "data.hpp"
#include "data_id.hpp"
#include "errors.hpp"
#include "language.hpp"
#include "speller_impl.hpp"
#include "string_list.hpp"
#include "suggest.hpp"
#include "tokenizer.hpp"
#include "convert.hpp"
#include "stack_ptr.hpp"
#include "istream_enumeration.hpp"

//#include "iostream.hpp"

#include "gettext.h"

namespace aspeller {
  //
  // data_access functions
  //

  const char * SpellerImpl::lang_name() const {
    return lang_->name();
  }

  //
  // to lower
  //

  char * SpellerImpl::to_lower(char * str) 
  {
    for (char * i = str; *i; ++i)
      *i = lang_->to_lower(*i);
    return str;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Spell check methods
  //

  PosibErr<void> SpellerImpl::add_to_personal(MutableString word) {
    if (!personal_) return no_err;
    return personal_->add(word);
  }
  
  PosibErr<void> SpellerImpl::add_to_session(MutableString word) {
    if (!session_) return no_err;
    return session_->add(word);
  }

  PosibErr<void> SpellerImpl::clear_session() {
    if (!session_) return no_err;
    return session_->clear();
  }

  PosibErr<void> SpellerImpl::store_replacement(MutableString mis, 
                                                MutableString cor)
  {
    return SpellerImpl::store_replacement(mis,cor,true);
  }

  PosibErr<void> SpellerImpl::store_replacement(const String & mis, 
                                                const String & cor, 
                                                bool memory) 
  {
    if (ignore_repl) return no_err;
    if (!repl_) return no_err;
    String::size_type pos;
    StackPtr<StringEnumeration> sugels(intr_suggest_->suggest(mis.c_str()).elements());
    const char * first_word = sugels->next();
    CheckInfo w1, w2;
    String cor1, cor2;
    String buf;
    bool correct = false;
    pos = cor.find(' '); 
    if (pos == String::npos) {
      cor1 = cor;
      correct = check_affix(cor, w1, 0);
    } else {
      cor1 = (String)cor.substr(0,pos);
      ++pos;
      while (pos < cor.size() && cor[pos] == ' ') ++pos;
      cor2 = (String)cor.substr(pos);
      correct = check_affix(cor1, w1, 0) && check_affix(cor2, w2, 0);
    }
    if (correct) {
      String cor_orignal_casing(cor1);
      if (!cor2.empty()) {
        cor_orignal_casing += cor[pos-1];
        cor_orignal_casing += cor2;
      }
      // Don't try to add the empty string, causes all kinds of
      // problems.  Can happen if the original replacement nothing but
      // whitespace.
      if (cor_orignal_casing.empty()) 
        return no_err;
      if (first_word == 0 || cor != first_word) {
        lang().to_lower(buf, mis.str());
        repl_->add_repl(buf, cor_orignal_casing);
      }
      
      if (memory && prev_cor_repl_ == mis) 
        store_replacement(prev_mis_repl_, cor, false);
      
    } else { //!correct
      
      if (memory) {
         if (prev_cor_repl_ != mis)
          prev_mis_repl_ = mis;
        prev_cor_repl_ = cor;
       }
    }
    return no_err;
  }

  //
  // simple functions
  //

  PosibErr<const WordList *> SpellerImpl::suggest(MutableString word) 
  {
    return &suggest_->suggest(word);
  }

  bool SpellerImpl::check_simple (ParmString w, WordEntry & w0) 
  {
    w0.clear(); // FIXME: is this necessary?
    const char * x = w;
    while (*x != '\0' && (x-w) < static_cast<int>(ignore_count)) ++x;
    if (*x == '\0') {w0.word = w; return true;}
    WS::const_iterator i   = check_ws.begin();
    WS::const_iterator end = check_ws.end();
    do {
      if ((*i)->lookup(w, &s_cmp, w0)) return true;
      ++i;
    } while (i != end);
    return false;
  };

  bool SpellerImpl::check_affix(ParmString word, CheckInfo & ci, GuessInfo * gi)
  {
    WordEntry w;
    bool res = check_simple(word, w);
    if (res) {ci.word = w.word; return true;}
    if (affix_compress) {
      res = lang_->affix()->affix_check(LookupInfo(this, LookupInfo::Word), word, ci, 0);
      if (res) return true;
    }
    if (affix_info && gi) {
      lang_->affix()->affix_check(LookupInfo(this, LookupInfo::Guess), word, ci, gi);
    }
    return false;
  }

  inline bool SpellerImpl::check_single(char * word, /* it WILL modify word */
                                        bool try_uppercase,
                                        CheckInfo & ci, GuessInfo * gi)
  {
    bool res = check_affix(word, ci, gi);
    if (res) return true;
    if (!try_uppercase) return false;
    char t = *word;
    *word = lang_->to_title(t);
    res = check_affix(word, ci, gi);
    *word = t;
    if (res) return true;
    return false;
  }

  CheckInfo * SpellerImpl::check_runtogether(char * word, char * word_end, 
                                             /* it WILL modify word */
                                             bool try_uppercase,
                                             unsigned run_together_limit,
                                             CheckInfo * ci, CheckInfo * ci_end,
                                             GuessInfo * gi)
  {
    if (ci >= ci_end) return NULL;
    clear_check_info(*ci);
    bool res = check_single(word, try_uppercase, *ci, gi);
    if (res) return ci;
    if (run_together_limit <= 1) return NULL;
    enum {Yes, No, Unknown} is_title = try_uppercase ? Yes : Unknown;
    for (char * i = word + run_together_min_; 
         i <= word_end - run_together_min_;
         ++i) 
    {
      char t = *i;
      *i = '\0';
      clear_check_info(*ci);
      res = check_single(word, try_uppercase, *ci, gi);
      if (!res) {*i = t; continue;}
      if (is_title == Unknown)
        is_title = lang_->case_pattern(word) == FirstUpper ? Yes : No;
      *i = t;
      CheckInfo * ci_last = check_runtogether(i, word_end, is_title == Yes, run_together_limit - 1, ci + 1, ci_end, 0);
      if (ci_last) {
        ci->compound = true;
        ci->next = ci + 1;
        return ci_last;
      }
    }
    return NULL;
  }

  PosibErr<bool> SpellerImpl::check(char * word, char * word_end, 
                                    /* it WILL modify word */
                                    bool try_uppercase,
                                    unsigned run_together_limit,
                                    CheckInfo * ci, CheckInfo * ci_end,
                                    GuessInfo * gi, CompoundInfo * cpi)
  {
    clear_check_info(*ci);
    bool res = check_runtogether(word, word_end, try_uppercase, run_together_limit, ci, ci_end, gi);
    if (res) return true;
    
    CompoundWord cw = lang_->split_word(word, word_end - word, camel_case_);
    if (cw.single()) return false;
    bool ok = true;
    CheckInfo * ci_prev = NULL;
    do {
      unsigned len = cw.word_len();
      
      char save = word[len];
      word[len] = '\0';
      CheckInfo * ci_last = check_runtogether(word, word + len, try_uppercase, run_together_limit, ci, ci_end, gi);
      bool found = ci_last;
      word[len] = save;

      if (!found) {
        if (cpi) {
          ci_last = ci;
          ok = false;
          ci->word.str = word;
          ci->word.len = len;
          ci->incorrect = true;
          cpi->incorrect_count++;
          if (!cpi->first_incorrect)
            cpi->first_incorrect = ci;
        } else {
          return false;
        }
      }

      if (cpi)
        cpi->count++;
      
      if (ci_prev) {
        ci_prev->compound = true;
        ci_prev->next = ci;
      }

      ci_prev = ci_last;
      ci = ci_last + 1;
      if (ci >= ci_end) {
        if (cpi) cpi->count = 0;
        return false;
      }
      
      word = word + cw.rest_offset();
      cw = lang_->split_word(cw.rest, cw.rest_len(), camel_case_);
      
    } while (!cw.empty());
    
    return ok;
    
    // for (;;) {
    //   cw = lang_->split_word(cw.rest, cw.rest_len(), camel_case_);
    //   if (cw.empty()) break;
    //   if (ci + 1 >= ci_end) {
    //     if (cpi) cpi->count = 0;
    //     return false;
    //   }
    //   if (cpi) cpi->count++;
    //   len = cw.word_len();
    //   save = word[len];
    //   word[len] = '\0';
    //   ci_last = check_runtogether(word, word + len, try_uppercase, run_together_limit, ci + 1, ci_end, gi);
    //   word[len] = save;
    //   ci->compound = true;
    //   ci->next = ci + 1;
    //   if (ci_last) {
    //     ci = ci_last;
    //   } else if (cpi) {
    //     ok = false;
    //     ci->word.str = word;
    //     ci->word.len = len;
    //     ci->incorrect = true;
    //     cpi->incorrect_count++;
    //   } else {
    //     return false;
    //   }
    //   word = word + cw.rest_offset();
    // }
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Word list managment methods
  //
  
  PosibErr<void> SpellerImpl::save_all_word_lists() {
    SpellerDict * i = dicts_;
    for (; i; i = i->next) {
      if  (i->save_on_saveall)
        RET_ON_ERR(i->dict->synchronize());
    }
    return no_err;
  }
  
  int SpellerImpl::num_wordlists() const {
    return 0; //FIXME
  }

  SpellerImpl::WordLists SpellerImpl::wordlists() const {
    return 0; //FIXME
    //return MakeEnumeration<DataSetCollection::Parms>(wls_->begin(), DataSetCollection::Parms(wls_->end()));
  }

  PosibErr<const WordList *> SpellerImpl::personal_word_list() const {
    const WordList * wl = static_cast<const WordList *>(personal_);
    if (!wl) return make_err(operation_not_supported_error, 
                             _("The personal word list is unavailable."));
    return wl;
  }

  PosibErr<const WordList *> SpellerImpl::session_word_list() const {
    const WordList * wl = static_cast<const WordList *>(session_);
    if (!wl) return make_err(operation_not_supported_error, 
                             _("The session word list is unavailable."));
    return wl;
  }

  PosibErr<const WordList *> SpellerImpl::main_word_list() const {
    const WordList * wl = dynamic_cast<const WordList *>(main_);
    if (!wl) return make_err(operation_not_supported_error, 
                             _("The main word list is unavailable."));
    return wl;
  }

  const SpellerDict * SpellerImpl::locate (const Dict::Id & id) const
  {
    for (const SpellerDict * i = dicts_; i; i = i->next)
      if (i->dict->id() == id) return i;
    return 0;
  }

  PosibErr<void> SpellerImpl::add_dict(SpellerDict * wc)
  {
    Dict * w = wc->dict;
    assert(locate(w->id()) == 0);

    if (!lang_) {
      lang_.copy(w->lang());
      config_->replace("lang", lang_name());
      config_->replace("language-tag", lang_name());
    } else {
      if (strcmp(lang_->name(), w->lang()->name()) != 0)
        return make_err(mismatched_language, lang_->name(), w->lang()->name());
    }

    // add to master list
    wc->next = dicts_;
    dicts_ = wc;

    // check if it has a special_id and act accordingly
    switch (wc->special_id) {
    case main_id:
      assert(main_ == 0);
      main_ = w;
      break;
    case personal_id:
      assert(personal_ == 0);
      personal_ = w;
      break;
    case session_id:
      assert(session_ == 0);
      session_ = w;
      break;
    case personal_repl_id:
      assert(repl_ == 0);
      repl_ = w;
      break;
    case none_id:
      break;
    }

    return no_err;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Config Notifier
  //

  struct UpdateMember {
    const char * name;
    enum Type {String, Int, Bool, Add, Rem, RemAll};
    Type type;
    union Fun {
      typedef PosibErr<void> (*WithStr )(SpellerImpl *, const char *);
      typedef PosibErr<void> (*WithInt )(SpellerImpl *, int);
      typedef PosibErr<void> (*WithBool)(SpellerImpl *, bool);
      WithStr  with_str;
      WithInt  with_int;
      WithBool with_bool;
      Fun() {}
      Fun(WithStr  m) : with_str (m) {}
      Fun(WithInt  m) : with_int (m) {}
      Fun(WithBool m) : with_bool(m) {}
      PosibErr<void> call(SpellerImpl * m, const char * val) const 
        {return (*with_str) (m,val);}
      PosibErr<void> call(SpellerImpl * m, int val)          const 
        {return (*with_int) (m,val);}
      PosibErr<void> call(SpellerImpl * m, bool val)         const 
        {return (*with_bool)(m,val);}
    } fun;
    typedef SpellerImpl::ConfigNotifier CN;
  };

  template <typename T>
  PosibErr<void> callback(SpellerImpl * m, const KeyInfo * ki, T value, 
                          UpdateMember::Type t);
  
  class SpellerImpl::ConfigNotifier : public Notifier {
  private:
    SpellerImpl * speller_;
  public:
    ConfigNotifier(SpellerImpl * m) 
      : speller_(m) 
    {}

    PosibErr<void> item_updated(const KeyInfo * ki, int value) {
      return callback(speller_, ki, value, UpdateMember::Int);
    }
    PosibErr<void> item_updated(const KeyInfo * ki, bool value) {
      return callback(speller_, ki, value, UpdateMember::Bool);
    }
    PosibErr<void> item_updated(const KeyInfo * ki, ParmStr value) {
      return callback(speller_, ki, value, UpdateMember::String);
    }

    static PosibErr<void> ignore(SpellerImpl * m, int value) {
      m->ignore_count = value;
      return no_err;
    }
    static PosibErr<void> ignore_accents(SpellerImpl * m, bool value) {
      return no_err;
    }
    static PosibErr<void> ignore_case(SpellerImpl * m, bool value) {
      m->s_cmp.case_insensitive = value;
      m->s_cmp_begin.case_insensitive = value;
      m->s_cmp_middle.case_insensitive = value;
      m->s_cmp_end.case_insensitive = value;
      return no_err;
    }
    static PosibErr<void> ignore_repl(SpellerImpl * m, bool value) {
      
      m->ignore_repl = value;
      return no_err;
    }
    static PosibErr<void> save_repl(SpellerImpl * m, bool value) {
      // FIXME
      // m->save_on_saveall(DataSet::Id(&m->personal_repl()), value);
      abort(); return no_err;
    }
    static PosibErr<void> sug_mode(SpellerImpl * m, const char * mode) {
      RET_ON_ERR(m->suggest_->set_mode(mode));
      RET_ON_ERR(m->intr_suggest_->set_mode(mode));
      return no_err;
    }
    static PosibErr<void> run_together(SpellerImpl * m, bool value) {
      m->unconditional_run_together_ = value;
      m->run_together = m->unconditional_run_together_;
      return no_err;
    }
    static PosibErr<void> run_together_limit(SpellerImpl * m, int value) {
      if (value > 8) {
        m->config()->replace("run-together-limit", "8");
        // will loop back
      } else {
        m->run_together_limit_ = value;
      }
      return no_err;
    }
    static PosibErr<void> run_together_min(SpellerImpl * m, int value) {
      m->run_together_min_ = value;
      return no_err;
    }
    static PosibErr<void> camel_case(SpellerImpl * m, bool value) {
      m->camel_case_ = value;
      return no_err;
    }
  };

  static UpdateMember update_members[] = 
  {
    {"ignore",         UpdateMember::Int,     UpdateMember::CN::ignore}
    ,{"ignore-accents",UpdateMember::Bool,    UpdateMember::CN::ignore_accents}
    ,{"ignore-case",   UpdateMember::Bool,    UpdateMember::CN::ignore_case}
    ,{"ignore-repl",   UpdateMember::Bool,    UpdateMember::CN::ignore_repl}
    //,{"save-repl",     UpdateMember::Bool,    UpdateMember::CN::save_repl}
    ,{"sug-mode",      UpdateMember::String,  UpdateMember::CN::sug_mode}
    ,{"run-together",  
        UpdateMember::Bool,    
        UpdateMember::CN::run_together}
    ,{"run-together-limit",  
        UpdateMember::Int,    
        UpdateMember::CN::run_together_limit}
    ,{"run-together-min",  
        UpdateMember::Int,    
        UpdateMember::CN::run_together_min}
    ,{"camel-case",
        UpdateMember::Bool,    
        UpdateMember::CN::camel_case}
  };

  template <typename T>
  PosibErr<void> callback(SpellerImpl * m, const KeyInfo * ki, T value, 
                          UpdateMember::Type t) 
  {
    const UpdateMember * i
      = update_members;
    const UpdateMember * end   
      = i + sizeof(update_members)/sizeof(UpdateMember);
    while (i != end) {
      if (strcmp(ki->name, i->name) == 0) {
        if (i->type == t) {
          RET_ON_ERR(i->fun.call(m, value));
          break;
        }
      }
      ++i;
    }
    return no_err;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // SpellerImpl inititization members
  //

  SpellerImpl::SpellerImpl() 
    : Speller(0) /* FIXME */, ignore_repl(true), 
      dicts_(0), personal_(0), session_(0), repl_(0), main_(0)
  {}

  inline PosibErr<void> add_dicts(SpellerImpl * sp, DictList & d)
  {
    for (;!d.empty(); d.pop())
    {
      if (!sp->locate(d.last()->id())) {
        RET_ON_ERR(sp->add_dict(new SpellerDict(d.last())));
      }
    }
    return no_err;
  }

  PosibErr<void> SpellerImpl::setup(Config * c) {
    assert (config_ == 0);
    config_.reset(c);

    ignore_repl = config_->retrieve_bool("ignore-repl");
    ignore_count = config_->retrieve_int("ignore");

    DictList to_add;
    RET_ON_ERR(add_data_set(config_->retrieve("master-path"), *config_, &to_add, this));
    RET_ON_ERR(add_dicts(this, to_add));

    s_cmp.lang = lang_;
    s_cmp.case_insensitive = config_->retrieve_bool("ignore-case");

    s_cmp_begin.lang = lang_; 
    s_cmp_begin.case_insensitive = s_cmp.case_insensitive;
    s_cmp_begin.end = false;

    s_cmp_middle.lang = lang_;
    s_cmp_middle.case_insensitive = s_cmp.case_insensitive;
    s_cmp_middle.begin = false;
    s_cmp_middle.end   = false;

    s_cmp_end.lang = lang_;
    s_cmp_end.case_insensitive = s_cmp.case_insensitive;
    s_cmp_end.begin = false;

    StringList extra_dicts;
    config_->retrieve_list("extra-dicts", &extra_dicts);
    StringListEnumeration els = extra_dicts.elements_obj();
    const char * dict_name;
    while ( (dict_name = els.next()) != 0) {
      RET_ON_ERR(add_data_set(dict_name,*config_, &to_add, this));
      RET_ON_ERR(add_dicts(this, to_add));
    }

    bool use_other_dicts = config_->retrieve_bool("use-other-dicts");

    if (use_other_dicts && !personal_)
    {
      Dictionary * temp;
      temp = new_default_writable_dict(*config_);
      PosibErrBase pe = temp->load(config_->retrieve("personal-path"),*config_);
      if (pe.has_err(cant_read_file))
        temp->set_check_lang(lang_name(), *config_);
      else if (pe.has_err())
        return pe;
      RET_ON_ERR(add_dict(new SpellerDict(temp, *config_, personal_id)));
    }
    
    if (use_other_dicts && !session_)
    {
      Dictionary * temp;
      temp = new_default_writable_dict(*config_);
      temp->set_check_lang(lang_name(), *config_);
      RET_ON_ERR(add_dict(new SpellerDict(temp, *config_, session_id)));
    }
     
    if (use_other_dicts && !repl_)
    {
      ReplacementDict * temp = new_default_replacement_dict(*config_);
      PosibErrBase pe = temp->load(config_->retrieve("repl-path"),*config_);
      if (pe.has_err(cant_read_file))
        temp->set_check_lang(lang_name(), *config_);
      else if (pe.has_err())
        return pe;
      RET_ON_ERR(add_dict(new SpellerDict(temp, *config_, personal_repl_id)));
    }

    StringList wordlist_files;
    config_->retrieve_list("wordlists", &wordlist_files);
    if (!wordlist_files.empty()) {
      Dictionary * dict = session_;
      if (!dict) {
        dict = new_default_writable_dict(*config_);
        dict->set_check_lang(lang_name(), *config_);
        RET_ON_ERR(add_dict(new SpellerDict(dict, *config_)));
      }
      const char * fn;
      StringListEnumeration els = wordlist_files.elements_obj();
      while ( (fn = els.next()) != 0) {
        FStream f;
        RET_ON_ERR(f.open(fn, "r"));
        IstreamEnumeration els(f);
        WordListIterator wl_itr(&els, lang_, 0);
        wl_itr.init_plain(*config_);
        for (;;) {
          PosibErr<bool> pe = wl_itr.adv();
          if (pe.has_err())
            return pe.with_file(fn);
          if (!pe.data) break;
          PosibErr<void> pev = dict->add(wl_itr->word);
          if (pev.has_err())
            return pev.with_file(fn);
        }
      }
    }

    const char * sys_enc = lang_->charmap();
    ConfigConvKey user_enc = config_->retrieve_value("encoding");
    if (user_enc.val == "none") {
      config_->replace("encoding", sys_enc);
      user_enc = sys_enc;
    }

    PosibErr<Convert *> conv;
    conv = new_convert(*c, user_enc, sys_enc, NormFrom);
    if (conv.has_err()) return conv;
    to_internal_.reset(conv);
    conv = new_convert(*c, sys_enc, user_enc, NormTo);
    if (conv.has_err()) return conv;
    from_internal_.reset(conv);

    unconditional_run_together_ = config_->retrieve_bool("run-together");
    run_together = unconditional_run_together_;
    
    run_together_limit_  = config_->retrieve_int("run-together-limit");
    if (run_together_limit_ > 8) {
      config_->replace("run-together-limit", "8");
      run_together_limit_ = 8;
    }
    run_together_min_    = config_->retrieve_int("run-together-min");

    camel_case_ = config_->retrieve_bool("camel-case");

    config_->add_notifier(new ConfigNotifier(this));

    config_->set_attached(true);

    affix_info = lang_->affix();

    //
    // setup word set lists
    //

    typedef Vector<SpellerDict *> AllWS; AllWS all_ws;
    for (SpellerDict * i = dicts_; i; i = i->next) {
      if (i->dict->basic_type == Dict::basic_dict ||
          i->dict->basic_type == Dict::replacement_dict) {
        all_ws.push_back(i);
      }
    }
    
    const std::type_info * ti = 0;
    while (!all_ws.empty())
    {
      AllWS::iterator i0 = all_ws.end();
      int max = -2;
      AllWS::iterator i = all_ws.begin();
      for (; i != all_ws.end(); ++i)
      {
        const Dictionary * ws = (*i)->dict;
        if (ti && *ti != typeid(*ws)) continue;
        if ((int)ws->size() > max) {max = ws->size(); i0 = i;}
      }

      if (i0 == all_ws.end()) {ti = 0; continue;}

      SpellerDict * cur = *i0;

      all_ws.erase(i0);

      ti = &typeid(*cur->dict);

      if (cur->use_to_check) {
        check_ws.push_back(cur->dict);
        if (cur->dict->affix_compressed) affix_ws.push_back(cur->dict);
      }
      if (cur->use_to_suggest) {
        suggest_ws.push_back(cur->dict);
        if (cur->dict->affix_compressed) suggest_affix_ws.push_back(cur->dict);
      }
    }
    fast_scan   = suggest_ws.front()->fast_scan;
    fast_lookup = suggest_ws.front()->fast_lookup;
    have_soundslike = lang_->have_soundslike();
    have_repl = lang_->have_repl();
    invisible_soundslike = suggest_ws.front()->invisible_soundslike;
    soundslike_root_only = suggest_ws.front()->soundslike_root_only;
    affix_compress = !affix_ws.empty();

    //
    // Setup suggest
    //

    PosibErr<Suggest *> pe;
    pe = new_default_suggest(this);
    if (pe.has_err()) return pe;
    suggest_.reset(pe.data);
    pe = new_default_suggest(this);
    if (pe.has_err()) return pe;
    intr_suggest_.reset(pe.data);

    return no_err;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // SpellerImpl destrution members
  //

  SpellerImpl::~SpellerImpl() {
    while (dicts_) {
      SpellerDict * next = dicts_->next;
      delete dicts_;
      dicts_ = next;
    }
  }

  //////////////////////////////////////////////////////////////////////
  //
  // SpellerImple setup tokenizer method
  //

  void SpellerImpl::setup_tokenizer(Tokenizer * tok)
  {
    for (int i = 0; i != 256; ++i) 
    {
      tok->char_type_[i].word   = lang_->is_alpha(i);
      tok->char_type_[i].begin  = lang_->special(i).begin;
      tok->char_type_[i].middle = lang_->special(i).middle;
      tok->char_type_[i].end    = lang_->special(i).end;
    }
    tok->conv_ = to_internal_;
  }


  //////////////////////////////////////////////////////////////////////
  //
  //
  //

  SpellerDict::SpellerDict(Dict * d) 
    : dict(d), special_id(none_id), next(0) 
  {
    switch (dict->basic_type) {
    case Dict::basic_dict:
      use_to_check = true;
      use_to_suggest = true;
      break;
    case Dict::replacement_dict:
      use_to_check = false;
      use_to_suggest = true;
    case Dict::multi_dict:
      break;
    default:
      abort();
    }
    save_on_saveall = false;
  }

  SpellerDict::SpellerDict(Dict * w, const Config & c, SpecialId id)
    : next(0) 
  {
    dict = w;
    special_id = id;
    switch (id) {
    case main_id:
      if (dict->basic_type == Dict::basic_dict) {

        use_to_check    = true;
        use_to_suggest  = true;
        save_on_saveall = false;

      } else if (dict->basic_type == Dict::replacement_dict) {
        
        use_to_check    = false;
        use_to_suggest  = false;
        save_on_saveall = false;
        
      } else {
        
        abort();
        
      }
      break;
    case personal_id:
      use_to_check = true;
      use_to_suggest = true;
      save_on_saveall = true;
      break;
    case session_id:
      use_to_check = true;
      use_to_suggest = true;
      save_on_saveall = false;
      break;
    case personal_repl_id:
      use_to_check = false;
      use_to_suggest = true;
      save_on_saveall = c.retrieve_bool("save-repl");
      break;
    case none_id:
      break;
    }
  }

  extern "C"
  Speller * libaspell_speller_default_LTX_new_speller_class(SpellerLtHandle)
  {
    return new SpellerImpl();
  }
}

