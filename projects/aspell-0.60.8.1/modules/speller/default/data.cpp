// This file is part of The New Aspell
// Copyright (C) 2000-2001,2011 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include "config.hpp"
#include "convert.hpp"
#include "data.hpp"
#include "data_id.hpp"
#include "errors.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "language.hpp"
#include "speller_impl.hpp"
#include "cache-t.hpp"
#include "vararray.hpp"

#include "gettext.h"

namespace aspeller {

  GlobalCache<Dictionary> dict_cache("dictionary");

  //
  // Dict impl
  //

  Dictionary::Id::Id(Dict * p, const FileName & fn)
    : ptr(p)
  {
    file_name = fn.name();
#ifdef USE_FILE_INO
    struct stat s;
    // the file ,i
    if (file_name[0] != '\0' && stat(fn.path().c_str(), &s) == 0) {
      ino = s.st_ino;
      dev = s.st_dev;
    } else {
      ino = 0;
      dev = 0;
    }
#endif
  }

  bool operator==(const Dictionary::Id & rhs, const Dictionary::Id & lhs)
  {
    if (rhs.ptr == 0 || lhs.ptr == 0) {
      if (rhs.file_name == 0 || lhs.file_name == 0)
	return false;
#ifdef USE_FILE_INO
      return rhs.ino == lhs.ino && rhs.dev == lhs.dev;
#else
      return strcmp(rhs.file_name, lhs.file_name) == 0;
#endif
    } else {
      return rhs.ptr == lhs.ptr;
    }
  }

  PosibErr<void> Dictionary::attach(const Language &l) {
    if (lang_ && strcmp(l.name(),lang_->name()) != 0)
      return make_err(mismatched_language, lang_->name(), l.name());
    if (!lang_) lang_.copy(&l);
    copy();
    return no_err;
  }

  Dictionary::Dictionary(BasicType t, const char * n)
    : Cacheable(&dict_cache), lang_(), id_(), 
      basic_type(t), class_name(n),
      validate_words(true),
      affix_compressed(false), 
      invisible_soundslike(false), soundslike_root_only(false),
      fast_scan(false), fast_lookup(false)
  {
    id_.reset(new Id(this));
  }

  Dictionary::~Dictionary() 
  {
  }

  const char *  Dictionary::lang_name() const {
    return lang_->name();
  }

  PosibErr<void>  Dictionary::check_lang(ParmString l) {
    if (l != lang_->name())
      return make_err(mismatched_language, lang_->name(), l);
    return no_err;
  }
 
  PosibErr<void> Dictionary::set_check_lang (ParmString l, Config & config)
  {
    if (lang_ == 0) {
      PosibErr<Language *> res = new_language(config, l);
      if (res.has_err()) return res;
      lang_.reset(res.data);
      RET_ON_ERR(lang_->set_lang_defaults(config));
      set_lang_hook(config);
    } else {
      if (l != lang_->name())
	return make_err(mismatched_language, l, lang_->name());
    }
    return no_err;
  }

  void Dictionary::FileName::copy(const FileName & other) 
  {
    path_ = other.path_;
    name_ = path_.c_str() + (other.name_ - other.path_.c_str());
  }

  void Dictionary::FileName::clear()
  {
    path_  = "";
    name_ = path_.c_str();
  }

  void Dictionary::FileName::set(ParmString str) 
  {
    path_ = str;
    int i = path_.size() - 1;
    while (i >= 0) {
      if (path_[i] == '/' || path_[i] == '\\') {
          ++i;
          break;
      }
      --i;
    }
    if (i < 0) i = 0;
    name_ = path_.c_str() + i;
  }

  PosibErr<void> Dictionary::set_file_name(ParmString fn) 
  {
    file_name_.set(fn);
    *id_ = Id(this, file_name_);
    return no_err;
  }

  PosibErr<void> Dictionary::update_file_info(FStream & f) 
  {
#ifdef USE_FILE_INO
    struct stat s;
    int ok = fstat(f.file_no(), &s);
    assert(ok == 0);
    id_->ino = s.st_ino;
    id_->dev = s.st_dev;
#endif
    return no_err;
  }

  //
  // BasicDict
  //

  class DictStringEnumeration : public StringEnumeration 
  {
    ClonePtr<Dict::Enum> real_;
  public:
    DictStringEnumeration(Dict::Enum * r) : real_(r) {}
    
    bool at_end() const {
      return real_->at_end();
    }
    const char * next() {
      // FIXME: It's not this simple when affixes are involved
      WordEntry * w =  real_->next(); 
      if (!w) return 0;
      return w->word;
    }
    StringEnumeration * clone() const {
      return new DictStringEnumeration(*this);
    }
    void assign(const StringEnumeration * other) {
      *this = *static_cast<const DictStringEnumeration *>(other);
    }
  };

  PosibErr<void> Dictionary::add_repl(ParmString mis, ParmString cor) 
  {
    if (!invisible_soundslike) {
      VARARRAY(char, sl, mis.size() + 1);
      lang()->LangImpl::to_soundslike(sl, mis.str(), mis.size());
      return add_repl(mis, cor, sl);
    } else {
      return add_repl(mis, cor, "");
    }
  }

  PosibErr<void> Dictionary::add(ParmString w) 
  {
    if (!invisible_soundslike) {
      VARARRAY(char, sl, w.size() + 1);
      lang()->LangImpl::to_soundslike(sl, w.str(), w.size());
      return add(w, sl);
    } else {
      return add(w, "");
    }
  }

  //
  // Default implementation;
  //

  PosibErr<void> Dictionary::load(ParmString, Config &, 
                                  DictList *, SpellerImpl *) 
  {
    return make_err(unimplemented_method, "load", class_name);
  }
  
  PosibErr<void> Dictionary::merge(ParmString)
  {
    return make_err(unimplemented_method, "load", class_name);
  }
  
  PosibErr<void> Dictionary::synchronize() 
  {
    return make_err(unimplemented_method, "synchronize", class_name);
  }
  
  PosibErr<void> Dictionary::save_noupdate()
  {
    return make_err(unimplemented_method, "save_noupdate", class_name);
  }
  
  PosibErr<void> Dictionary::save_as(ParmString)
  {
    return make_err(unimplemented_method, "save_as", class_name);
  }
  
  PosibErr<void> Dictionary::clear() 
  {
    return make_err(unimplemented_method, "clear", class_name);
  }

  StringEnumeration * Dictionary::elements() const
  {
    Enum * e = detailed_elements();
    if (!e) return 0;
    return new DictStringEnumeration(e);
  }
  
  Dict::Enum * Dictionary::detailed_elements() const
  {
    return 0;
  }
  
  Dict::Size   Dictionary::size()     const
  {
    if (empty()) return 0;
    else         return 1;
  }
  
  bool Dictionary::empty()    const 
  {
    return false;
  }
  
  bool Dictionary::lookup (ParmString word, const SensitiveCompare *,
                           WordEntry &) const
  {
    return false;
  }
  
  bool Dictionary::clean_lookup(ParmString, WordEntry &) const 
  {
    return false;
  }
  
  bool Dictionary::soundslike_lookup(const WordEntry &, WordEntry &) const
  {
    return false;
  }
  
  bool Dictionary::soundslike_lookup(ParmString, WordEntry &) const
  {
    return false;
  }
  
  SoundslikeEnumeration * Dictionary::soundslike_elements() const
  {
    return 0;
  }
  
  PosibErr<void> Dictionary::add(ParmString w, ParmString s) 
  {
    return make_err(unimplemented_method, "add", class_name);
  }
  
  PosibErr<void> Dictionary::remove(ParmString w) 
  {
    return make_err(unimplemented_method, "remove", class_name);
  }
  
  bool Dictionary::repl_lookup(const WordEntry &, WordEntry &) const 
  {
    return false;
  }

  bool Dictionary::repl_lookup(ParmString, WordEntry &) const 
  {
    return false;
  }

  PosibErr<void> Dictionary::add_repl(ParmString mis, ParmString cor, ParmString s) 
  {
    return make_err(unimplemented_method, "add_repl", class_name);
  }
  
  PosibErr<void> Dictionary::remove_repl(ParmString mis, ParmString cor) 
  {
    return make_err(unimplemented_method, "remove_repl", class_name);
  }
  
  DictsEnumeration * Dictionary::dictionaries() const 
  {
    return 0;
  }

#define write_conv(s) do { \
    if (!c) {o << s;} \
    else {ParmString ss(s); buf.clear(); c->convert(ss.str(), ss.size(), buf); o.write(buf.data(), buf.size());} \
  } while (false)

  OStream & WordEntry::write (OStream & o,
                              const Language & l,
                              Convert * c) const
  {
    CharVector buf;
    write_conv(word);
    if (aff && *aff) {
      o << '/';
      write_conv(aff);
    }
    return o;
  }

  PosibErr<Dict *> add_data_set(ParmString fn,
                                Config & config,
                                DictList * new_dicts,
                                SpellerImpl * speller,
                                ParmString dir,
                                DataType allowed)
  {
    Dict * res = 0;
    static const char * suffix_list[] = {"", ".multi", ".alias", 
					 ".spcl", ".special",
					 ".pws", ".prepl"};
    FStream in;
    const char * * suffix;
    const char * * suffix_end 
      = suffix_list + sizeof(suffix_list)/sizeof(const char *);
    String dict_dir = config.retrieve("dict-dir");
    String true_file_name;
    Dict::FileName file_name(fn);
    const char * d = dir;
    do {
      if (d == 0) d = dict_dir.c_str();
      suffix = suffix_list;
      do {
	true_file_name = add_possible_dir(d, ParmString(file_name.path())
					  + ParmString(*suffix));
	in.open(true_file_name, "r").ignore_err();
	++suffix;
      } while (!in && suffix != suffix_end);
      if (d == dict_dir.c_str()) break;
      d = 0;
    } while (!in);
    if (!in) {
      true_file_name = add_possible_dir(dir ? dir.str() : d, file_name.path());
      return make_err(cant_read_file, true_file_name);
    }
    DataType actual_type;
    if ((true_file_name.size() > 5
	 && true_file_name.substr(true_file_name.size() - 6, 6) == ".spcl") 
	||
	(true_file_name.size() > 6 
	 && (true_file_name.substr(true_file_name.size() - 6, 6) == ".multi" 
	     || true_file_name.substr(true_file_name.size() - 6, 6) == ".alias")) 
	||
	(true_file_name.size() > 8
	 && true_file_name.substr(true_file_name.size() - 6, 6) == ".special")) 
    {

      actual_type = DT_Multi;

    } else {
      
      char head[32] = {0};
      in.read(head, 32);
      if      (strncmp(head, "aspell default speller rowl", 27) ==0)
	actual_type = DT_ReadOnly;
      else if (strncmp(head, "personal_repl", 13) == 0)
	actual_type = DT_WritableRepl;
      else if (strncmp(head, "personal_ws", 11) == 0)
	actual_type = DT_Writable;
      else
	return make_err(bad_file_format, true_file_name);
    }
    
    if (actual_type & ~allowed)
      return make_err(bad_file_format, true_file_name
		      , _("is not one of the allowed types"));

    Dict::FileName id_fn(true_file_name);
    Dict::Id id(0,id_fn);

    if (speller != 0) {
      const SpellerDict * d = speller->locate(id);
      if (d != 0) {
        return d->dict;
      }
    }

    res = 0;

    Lock dict_cache_lock(NULL);

    if (actual_type == DT_ReadOnly) { // try to get it from the cache
      dict_cache_lock.set(&dict_cache.lock); 
      res = dict_cache.find(id);
    }

    if (!res) {

      StackPtr<Dict> w;
      switch (actual_type) {
      case DT_ReadOnly: 
        w = new_default_readonly_dict();
        break;
      case DT_Multi:
        w = new_default_multi_dict();
        break;
      case DT_Writable: 
        w = new_default_writable_dict(config);
        break;
      case DT_WritableRepl:
        w = new_default_replacement_dict(config);
        break;
      default:
        abort();
      }

      RET_ON_ERR(w->load(true_file_name, config, new_dicts, speller));

      if (actual_type == DT_ReadOnly)
        dict_cache.add(w);
      
      res = w.release();

    } else { // actual_type == DT_ReadOnly implied, and hence the lock
             // is already acquired

      res->copy_no_lock();
      
    }

    dict_cache_lock.release();

    if (new_dicts)
      new_dicts->add(res);
    
    return res;
  }

}

