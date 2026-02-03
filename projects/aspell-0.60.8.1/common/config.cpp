// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

//#include <stdio.h>
//#define DEBUG {fprintf(stderr,"File: %s(%i)\n",__FILE__,__LINE__);}
#include <string.h>
#include <stdlib.h>
#include "ndebug.hpp"
#include <assert.h>

#include "dirs.h"
#include "settings.h"

#ifdef USE_LOCALE
# include <locale.h>
#endif

#ifdef HAVE_LANGINFO_CODESET
# include <langinfo.h>
#endif

#include "cache.hpp"
#include "asc_ctype.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "itemize.hpp"
#include "mutable_container.hpp"
#include "posib_err.hpp"
#include "string_map.hpp"
#include "stack_ptr.hpp"
#include "char_vector.hpp"
#include "convert.hpp"
#include "vararray.hpp"
#include "string_list.hpp"

#include "gettext.h"

#include "iostream.hpp"

#define DEFAULT_LANG "en_US"

// NOTE: All filter options are now stored with he "f-" prefix.  However
//   during lookup, the non prefix version is also recognized.

// The "place_holder" field in Entry and the "Vector<int>" parameter of
// commit_all are there to deal with the fact that with spacing
// options on the command line such as "--key what" it cannot be
// determined if "what" should be a value of "key" or if it should
// be treated as an independent arg.  This is because "key" may
// be a filter option.  Filter options KeyInfo are not loaded until
// after a commit which is not done at the time the options are being
// read in from the command line.  (If the command line arguments are
// read in after the other settings are read in and committed, then any
// options setting any of the config files will be ignored.  Thus, the
// command line must be parsed and options must be added in an
// uncommitted state).  So the solution is to assume it is an
// independent arg until told otherwise, the position in the arg array
// is stored along with the value in the "place_holder" field.  When
// the config class is finally committed and it is determined that
// "what" is really a value for key the stored arg position is pushed
// onto the Vector<int> so it can be removed from the arg array.  In
// the case of a "lset-*" this will happen in multiple config
// "Entry"s, so care is taken to only add the arg position once.

namespace acommon {

  const char * const keyinfo_type_name[4] = {
    N_("string"), N_("integer"), N_("boolean"), N_("list")
  };

  const int Config::num_parms_[9] = {1, 1, 0, 0, 0,
                                     1, 1, 1, 0};
  
  typedef Notifier * NotifierPtr;
  
  Config::Config(ParmStr name,
		 const KeyInfo * mainbegin, 
		 const KeyInfo * mainend)
    : name_(name)
    , first_(0), insert_point_(&first_)
    , committed_(true), attached_(false)
    , md_info_list_index(-1)
    , settings_read_in_(false)
    , load_filter_hook(0)
    , filter_mode_notifier(0)
  {
    keyinfo_begin = mainbegin;
    keyinfo_end   = mainend;
    extra_begin = 0;
    extra_end   = 0;
  }

  Config::~Config() {
    del();
  }

  Config::Config(const Config & other) 
  {
    copy(other);
  }
  
  Config & Config::operator= (const Config & other)
  {
    del();
    copy(other);
    return *this;
  }
  
  Config * Config::clone() const {
    return new Config(*this);
  }

  void Config::assign(const Config * other) {
    *this = *(const Config *)(other);
  }

  void Config::copy(const Config & other)
  {
    name_ = other.name_;

    committed_ = other.committed_;
    attached_ = other.attached_;
    settings_read_in_ = other.settings_read_in_;

    keyinfo_begin = other.keyinfo_begin;
    keyinfo_end   = other.keyinfo_end;
    extra_begin   = other.extra_begin;
    extra_end     = other.extra_end;
    filter_modules = other.filter_modules;

#ifdef HAVE_LIBDL
    filter_modules_ptrs = other.filter_modules_ptrs;
    for (Vector<Cacheable *>::iterator i = filter_modules_ptrs.begin();
         i != filter_modules_ptrs.end();
         ++i)
      (*i)->copy();
#endif

    md_info_list_index = other.md_info_list_index;

    insert_point_ = 0;
    Entry * const * src  = &other.first_;
    Entry * * dest = &first_;
    while (*src) 
    {
      *dest = new Entry(**src);
      if (src == other.insert_point_)
        insert_point_ = dest;
      src  = &((*src)->next);
      dest = &((*dest)->next);
    }
    if (insert_point_ == 0)
      insert_point_ = dest;
    *dest = 0;

    Vector<Notifier *>::const_iterator i   = other.notifier_list.begin();
    Vector<Notifier *>::const_iterator end = other.notifier_list.end();

    for(; i != end; ++i) {
      Notifier * tmp = (*i)->clone(this);
      if (tmp != 0)
	notifier_list.push_back(tmp);
    }
  }

  void Config::del()
  {
    while (first_) {
      Entry * tmp = first_->next;
      delete first_;
      first_ = tmp;
    }

    Vector<Notifier *>::iterator i   = notifier_list.begin();
    Vector<Notifier *>::iterator end = notifier_list.end();

    for(; i != end; ++i) {
      delete (*i);
      *i = 0;
    }
    
    notifier_list.clear();

#ifdef HAVE_LIBDL
    filter_modules.clear();
    for (Vector<Cacheable *>::iterator i = filter_modules_ptrs.begin();
         i != filter_modules_ptrs.end();
         ++i)
      (*i)->release();
    filter_modules_ptrs.clear();
#endif
  }

  void Config::set_filter_modules(const ConfigModule * modbegin, 
				  const ConfigModule * modend)
  {
    assert(filter_modules_ptrs.empty());
    filter_modules.clear();
    filter_modules.assign(modbegin, modend);
  }

  void Config::set_extra(const KeyInfo * begin, 
			       const KeyInfo * end) 
  {
    extra_begin = begin;
    extra_end   = end;
  }

  //
  //
  //


  //
  // Notifier methods
  //

  NotifierEnumeration * Config::notifiers() const 
  {
    return new NotifierEnumeration(notifier_list);
  }

  bool Config::add_notifier(Notifier * n) 
  {
    Vector<Notifier *>::iterator i   = notifier_list.begin();
    Vector<Notifier *>::iterator end = notifier_list.end();

    while (i != end && *i != n)
      ++i;

    if (i != end) {
    
      return false;
    
    } else {

      notifier_list.push_back(n);
      return true;

    }
  }

  bool Config::remove_notifier(const Notifier * n) 
  {
    Vector<Notifier *>::iterator i   = notifier_list.begin();
    Vector<Notifier *>::iterator end = notifier_list.end();

    while (i != end && *i != n)
      ++i;

    if (i == end) {
    
      return false;
    
    } else {

      delete *i;
      notifier_list.erase(i);
      return true;

    }
  }

  bool Config::replace_notifier(const Notifier * o, 
				      Notifier * n) 
  {
    Vector<Notifier *>::iterator i   = notifier_list.begin();
    Vector<Notifier *>::iterator end = notifier_list.end();

    while (i != end && *i != o)
      ++i;

    if (i == end) {
    
      return false;
    
    } else {

      delete *i;
      *i = n;
      return true;

    }
  }

  //
  // retrieve methods
  //

  const Config::Entry * Config::lookup(const char * key) const
  {
    const Entry * res = 0;
    const Entry * cur = first_;

    while (cur) {
      if (cur->key == key && cur->action != NoOp)  res = cur;
      cur = cur->next;
    }

    if (!res || res->action == Reset) return 0;
    return res;
  }

  bool Config::have(ParmStr key) const 
  {
    PosibErr<const KeyInfo *> pe = keyinfo(key);
    if (pe.has_err()) {pe.ignore_err(); return false;}
    return lookup(pe.data->name);
  }

  PosibErr<String> Config::retrieve(ParmStr key) const
  {
    RET_ON_ERR_SET(keyinfo(key), const KeyInfo *, ki);
    if (ki->type == KeyInfoList) return make_err(key_not_string, ki->name);

    const Entry * cur = lookup(ki->name);

    return cur ? cur->value : get_default(ki);
  }

  PosibErr<Config::Value> Config::retrieve_value(ParmStr key) const
  {
    RET_ON_ERR_SET(keyinfo(key), const KeyInfo *, ki);
    if (ki->type == KeyInfoList) return make_err(key_not_string, ki->name);

    const Entry * cur = lookup(ki->name);

    return cur ? Value(cur->value,cur->secure) : Value(get_default(ki), true);
  }
  
  PosibErr<String> Config::retrieve_any(ParmStr key) const
  {
    RET_ON_ERR_SET(keyinfo(key), const KeyInfo *, ki);

    if (ki->type != KeyInfoList) {
      const Entry * cur = lookup(ki->name);
      return cur ? cur->value : get_default(ki);
    } else {
      StringList sl;
      RET_ON_ERR(retrieve_list(key, &sl));
      StringListEnumeration els = sl.elements_obj();
      const char * s;
      String val;
      while ( (s = els.next()) != 0 ) {
        val += s;
        val += '\n';
      }
      val.pop_back();
      return val;
    }
  }

  PosibErr<bool> Config::retrieve_bool(ParmStr key) const
  {
    RET_ON_ERR_SET(keyinfo(key), const KeyInfo *, ki);
    if (ki->type != KeyInfoBool) return make_err(key_not_bool, ki->name);

    const Entry * cur = lookup(ki->name);

    String value(cur ? cur->value : get_default(ki));

    if (value == "false") return false;
    else                  return true;
  }
  
  PosibErr<int> Config::retrieve_int(ParmStr key) const
  {
    assert(committed_); // otherwise the value may not be an integer
                        // as it has not been verified.

    RET_ON_ERR_SET(keyinfo(key), const KeyInfo *, ki);
    if (ki->type != KeyInfoInt) return make_err(key_not_int, ki->name);

    const Entry * cur = lookup(ki->name);

    String value(cur ? cur->value : get_default(ki));

    return atoi(value.str());
  }

#define RET_ON_ERR_WRAP(prefix, key, cmd)                                \
  do{PosibErrBase pe(cmd);if(pe.has_err())return pe.with_key(prefix, strncmp(key, "f-", 2) == 0 ? key + 2 : key);}while(false)

  PosibErr<void> Config::lookup_list(const KeyInfo * ki,
                                     MutableContainer & m,
                                     bool include_default) const
  {
    const Entry * cur = first_;
    const Entry * first_to_use = 0;

    while (cur) {
      if (cur->key == ki->name && 
          (first_to_use == 0 || 
           cur->action == Reset || cur->action == Set 
           || cur->action == ListClear)) 
        first_to_use = cur;
      cur = cur->next;
    }

    cur = first_to_use;

    if (include_default && 
        (!cur || 
         !(cur->action == Set || cur->action == ListClear)))
    {
      String def = get_default(ki);
      separate_list(def, m, true);
    }

    if (cur && cur->action == Reset) {
      cur = cur->next;
    }

    if (cur && cur->action == Set) {
      if (!include_default) m.clear();
      RET_ON_ERR_WRAP("", ki->name, m.add(cur->value));
      cur = cur->next;
    }

    if (cur && cur->action == ListClear) {
      if (!include_default) m.clear();
      cur = cur->next;
    }

    while (cur) {
      if (cur->key == ki->name) {
        if (cur->action == ListAdd)
          RET_ON_ERR_WRAP("add-", ki->name, m.add(cur->value));
        else if (cur->action == ListRemove)
          RET_ON_ERR_WRAP("remove-", ki->name, m.remove(cur->value));
      }
      cur = cur->next;
    }
    return no_err;
  }

#undef RET_ON_ERR_WRAP

  PosibErr<void> Config::retrieve_list(ParmStr key, 
				       MutableContainer * m) const
  {
    RET_ON_ERR_SET(keyinfo(key), const KeyInfo *, ki);
    if (ki->type != KeyInfoList) return make_err(key_not_list, ki->name);

    RET_ON_ERR(lookup_list(ki, *m, true));

    return no_err;
  }

  static const KeyInfo * find(ParmStr key, 
			      const KeyInfo * i, 
			      const KeyInfo * end) 
  {
    while (i != end) {
      if (strcmp(key, i->name) == 0)
	return i;
      ++i;
    }
    return i;
  }

  static const ConfigModule * find(ParmStr key, 
				   const ConfigModule * i, 
				   const ConfigModule * end) 
  {
    while (i != end) {
      if (strcmp(key, i->name) == 0)
	return i;
      ++i;
    }
    return i;
  }

  PosibErr<const KeyInfo *> Config::keyinfo(ParmStr key) const
  {
    typedef PosibErr<const KeyInfo *> Ret;
    {
      const KeyInfo * i;
      i = acommon::find(key, keyinfo_begin, keyinfo_end);
      if (i != keyinfo_end) return Ret(i);
      
      i = acommon::find(key, extra_begin, extra_end);
      if (i != extra_end) return Ret(i);
      
      const char * s = strncmp(key, "f-", 2) == 0 ? key + 2 : key.str();
      const char * h = strchr(s, '-');
      if (h == 0) goto err;

      String k(s, h - s);
      const ConfigModule * j = acommon::find(k,
					     filter_modules.pbegin(),
					     filter_modules.pend());
      
      if (j == filter_modules.pend() && load_filter_hook && committed_) {
        // FIXME: This isn't quite right
        PosibErrBase pe = load_filter_hook(const_cast<Config *>(this), k);
        pe.ignore_err();
        j = acommon::find(k,
                          filter_modules.pbegin(),
                          filter_modules.pend());
      }

      if (j == filter_modules.pend()) goto err;

      i = acommon::find(key, j->begin, j->end);
      if (i != j->end) return Ret(i);
      
      if (strncmp(key, "f-", 2) != 0) k = "f-";
      else                            k = "";
      k += key;
      i = acommon::find(k, j->begin, j->end);
      if (i != j->end) return Ret(i);
    }
  err:  
    return Ret().prim_err(unknown_key, key);
  }

  static bool proc_locale_str(ParmStr lang, String & final_str)
  {
    if (lang == 0) return false;
    const char * i = lang;
    if (!(asc_islower(i[0]) && asc_islower(i[1]))) return false;
    final_str.assign(i, 2);
    i += 2;
    if (! (i[0] == '_' || i[0] == '-')) return true;
    i += 1;
    if (!(asc_isupper(i[0]) && asc_isupper(i[1]))) return true;
    final_str += '_';
    final_str.append(i, 2);
    return true;
  }

  static void get_lang_env(String & str) 
  {
    if (proc_locale_str(getenv("LC_MESSAGES"), str)) return;
    if (proc_locale_str(getenv("LANG"), str)) return;
    if (proc_locale_str(getenv("LANGUAGE"), str)) return;
    str = DEFAULT_LANG;
  }

#ifdef USE_LOCALE

  static void get_lang(String & final_str) 
  {
    // FIXME: THIS IS NOT THREAD SAFE
    String locale = setlocale (LC_ALL, NULL);
    if (locale == "C")
      setlocale (LC_ALL, "");
    const char * lang = setlocale (LC_MESSAGES, NULL);
    bool res = proc_locale_str(lang, final_str);
    if (locale == "C")
      setlocale(LC_MESSAGES, locale.c_str());
    if (!res)
      get_lang_env(final_str);
  }

#else

  static inline void get_lang(String & str) 
  {
    get_lang_env(str);
  }

#endif

#if defined USE_LOCALE && defined HAVE_LANGINFO_CODESET

  static inline void get_encoding(const Config & c, String & final_str)
  {
    const char * codeset = nl_langinfo(CODESET);
    if (ascii_encoding(c, codeset)) codeset = "none";
    final_str = codeset;
  }

#else

  static inline void get_encoding(const Config &, String & final_str)
  {
    final_str = "none";
  }

#endif

  String Config::get_default(const KeyInfo * ki) const
  {
    bool   in_replace = false;
    String final_str;
    String replace;
    const char * i = ki->def;
    if (*i == '!') { // special cases
      ++i;
    
      if (strcmp(i, "lang") == 0) {
        
        const Entry * entry;
        if (entry = lookup("actual-lang"), entry) {
          return entry->value;
        } else if (have("master")) {
	  final_str = "<unknown>";
	} else {
	  get_lang(final_str);
	}
	
      } else if (strcmp(i, "encoding") == 0) {

        get_encoding(*this, final_str);

      } else if (strcmp(i, "special") == 0) {

	// do nothing

      } else {
      
	abort(); // this should not happen
      
      }
    
    } else for(; *i; ++i) {
    
      if (!in_replace) {

	if (*i == '<') {
	  in_replace = true;
	} else {
	  final_str += *i;
	}

      } else { // in_replace
      
	if (*i == '/' || *i == ':' || *i == '|' || *i == '#' || *i == '^') {
	  char sep = *i;
	  String second;
	  ++i;
	  while (*i != '\0' && *i != '>') second += *i++;
	  if (sep == '/') {
	    String s1 = retrieve(replace);
	    String s2 = retrieve(second);
	    final_str += add_possible_dir(s1, s2);
	  } else if (sep == ':') {
	    String s1 = retrieve(replace);
	    final_str += add_possible_dir(s1, second);
	  } else if (sep == '#') {
	    String s1 = retrieve(replace);
	    assert(second.size() == 1);
	    unsigned int s = 0;
	    while (s != s1.size() && s1[s] != second[0]) ++s;
	    final_str.append(s1, s);
	  } else if (sep == '^') {
	    String s1 = retrieve(replace);
	    String s2 = retrieve(second);
	    final_str += figure_out_dir(s1, s2);
	  } else { // sep == '|'
	    assert(replace[0] == '$');
	    const char * env = getenv(replace.c_str()+1);
	    final_str += env ? env : second;
	  }
	  replace = "";
	  in_replace = false;

	} else if (*i == '>') {

	  final_str += retrieve(replace).data;
	  replace = "";
	  in_replace = false;

	} else {

	  replace += *i;

	}

      }
      
    }
    return final_str;
  }

  PosibErr<String> Config::get_default(ParmStr key) const
  {
    RET_ON_ERR_SET(keyinfo(key), const KeyInfo *, ki);
    return get_default(ki);
  }



#define TEST(v,l,a)                         \
  do {                                      \
    if (len == l && memcmp(s, v, l) == 0) { \
      if (action) *action = a;              \
      return c + 1;                         \
    }                                       \
  } while (false)

  const char * Config::base_name(const char * s, Action * action)
  {
    if (action) *action = Set;
    const char * c = strchr(s, '-');
    if (!c) return s;
    unsigned len = c - s;
    TEST("reset",   5, Reset);
    TEST("enable",  6, Enable);
    TEST("dont",    4, Disable);
    TEST("disable", 7, Disable);
    TEST("lset",    4, ListSet);
    TEST("rem",     3, ListRemove);
    TEST("remove",  6, ListRemove);
    TEST("add",     3, ListAdd);
    TEST("clear",   5, ListClear);
    return s;
  }

#undef TEST

  void separate_list(ParmStr value, AddableContainer & out, bool do_unescape)
  {
    unsigned len = value.size();
    
    VARARRAY(char, buf, len + 1);
    memcpy(buf, value, len + 1);
    
    len = strlen(buf);
    char * s = buf;
    char * end = buf + len;
      
    while (s < end)
    {
      if (do_unescape) while (*s == ' ' || *s == '\t') ++s;
      char * b = s;
      char * e = s;
      while (*s != '\0') {
        if (do_unescape && *s == '\\') {
          ++s;
          if (*s == '\0') break;
          e = s;
          ++s;
        } else {
          if (*s == ':') break;
          if (!do_unescape || (*s != ' ' && *s != '\t')) e = s;
          ++s;
        }
      }
      if (s != b) {
        ++e;
        *e = '\0';
        if (do_unescape) unescape(b);
      
        out.add(b);
      }
      ++s;
    }
  }

  void combine_list(String & res, const StringList & in)
  {
    res.clear();
    StringListEnumeration els = in.elements_obj();
    const char * s = 0;
    while ( (s = els.next()) != 0) 
    {
      for (; *s; ++s) {
        if (*s == ':')
          res.append('\\');
        res.append(*s);
      }
      res.append(':');
    }
    if (!res.empty() && res.back() == ':') res.pop_back();
  }

  struct ListAddHelper : public AddableContainer 
  {
    Config * config;
    Config::Entry * orig_entry;
    PosibErr<bool> add(ParmStr val);
  };

  PosibErr<bool> ListAddHelper::add(ParmStr val)
  {
    Config::Entry * entry = new Config::Entry(*orig_entry);
    entry->value = val;
    entry->action = Config::ListAdd;
    config->set(entry);
    return true;
  }

  void Config::replace_internal(ParmStr key, ParmStr value)
  {
    Entry * entry = new Entry;
    entry->key = key;
    entry->value = value;
    entry->action = Set;
    entry->next = *insert_point_;
    *insert_point_ = entry;
    insert_point_ = &entry->next;
  }

  PosibErr<void> Config::replace(ParmStr key, ParmStr value)
  {
    Entry * entry = new Entry;
    entry->key = key;
    entry->value = value;
    entry->secure = true;
    return set(entry);
  }
  
  PosibErr<void> Config::remove(ParmStr key)
  {
    Entry * entry = new Entry;
    entry->key = key;
    entry->action = Reset;
    return set(entry);
  }

  PosibErr<void> Config::set(Entry * entry0, bool do_unescape)
  {
    StackPtr<Entry> entry(entry0);

    if (entry->action == NoOp)
      entry->key = base_name(entry->key.str(), &entry->action);

    if (num_parms(entry->action) == 0 && !entry->value.empty()) 
    {
      if (entry->place_holder == -1) {
        switch (entry->action) {
        case Reset:
          return make_err(no_value_reset, entry->key);
        case Enable:
          return make_err(no_value_enable, entry->key);
        case Disable:
          return make_err(no_value_disable, entry->key);
        case ListClear:
          return make_err(no_value_clear, entry->key);
        default:
          abort(); // this shouldn't happen
        }
      } else {
        entry->place_holder = -1;
      }
    }

    if (entry->action != ListSet) {

      switch (entry->action) {
      case Enable:
        entry->value = "true";
        entry->action = Set;
        break;
      case Disable:
        entry->value = "false";
        entry->action = Set;
        break;
      default:
        ;
      }
      if (do_unescape) unescape(entry->value.mstr());

      entry->next = *insert_point_;
      *insert_point_ = entry;
      insert_point_ = &entry->next;
      entry.release();
      if (committed_) RET_ON_ERR(commit(entry0)); // entry0 == entry
      
    } else { // action == ListSet

      Entry * ent = new Entry;
      ent->key = entry->key;
      ent->action = ListClear;
      RET_ON_ERR(set(ent));

      ListAddHelper helper;
      helper.config = this;
      helper.orig_entry = entry;

      separate_list(entry->value.str(), helper, do_unescape);
    }
    return no_err;
  }

  PosibErr<void> Config::merge(const Config & other)
  {
    for (const Entry * src  = other.first_; src; src = src->next)
    {
      if (src->action == NoOp) continue;
      Entry * entry = new Entry(*src);
      entry->next = *insert_point_;
      *insert_point_ = entry;
      insert_point_ = &entry->next;
      if (committed_) RET_ON_ERR(commit(entry));
    }
    return no_err;
  }

  PosibErr<void> Config::lang_config_merge(const Config & other,
                                           int which, ParmStr data_encoding)
  {
    Conv to_utf8;
    RET_ON_ERR(to_utf8.setup(*this, data_encoding, "utf-8", NormTo));
    const Entry * src  = other.first_;
    Entry * * ip = &first_;
    while (src)
    {
      const KeyInfo * l_ki = other.keyinfo(src->key);
      if (l_ki->other_data == which) {
        const KeyInfo * c_ki = keyinfo(src->key);
        Entry * entry = new Entry(*src);
        if (c_ki->flags & KEYINFO_UTF8)
          entry->value = to_utf8(entry->value);
        entry->next = *ip;
        *ip = entry;
        ip = &entry->next;
      }
      src = src->next;
    }
    return no_err;
  }


#define NOTIFY_ALL(fun)                                       \
  do {                                                        \
    Vector<Notifier *>::iterator   i = notifier_list.begin(); \
    Vector<Notifier *>::iterator end = notifier_list.end();   \
    while (i != end) {                                        \
      RET_ON_ERR((*i)->fun);                                  \
      ++i;                                                    \
    }                                                         \
  } while (false)

  PosibErr<int> Config::commit(Entry * entry, Conv * conv) 
  {
    PosibErr<const KeyInfo *> pe = keyinfo(entry->key);
    {
      if (pe.has_err()) goto error;
      
      const KeyInfo * ki = pe;

      entry->key = ki->name;
      
      // FIXME: This is the correct thing to do but it causes problems
      //        with changing a filter mode in "pipe" mode and probably
      //        elsewhere.
      //if (attached_ && !(ki->flags & KEYINFO_MAY_CHANGE)) {
      //  pe = make_err(cant_change_value, entry->key);
      //  goto error;
      //}

      int place_holder = entry->place_holder;
      
      if (conv && ki->flags & KEYINFO_UTF8)
        entry->value = (*conv)(entry->value);

      if (ki->type != KeyInfoList && list_action(entry->action)) {
        pe = make_err(key_not_list, entry->key);
        goto error;
      }

      if (!ki->def) // if null this key should never have values
                    // directly added to it
        return make_err(aerror_cant_change_value, entry->key);

      String value(entry->action == Reset ? get_default(ki) : entry->value);
      
      switch (ki->type) {
        
      case KeyInfoBool: {

        bool val;
      
        if  (value.empty() || entry->place_holder != -1) {
          // if entry->place_holder != -1 than IGNORE the value no
          // matter what it is
          entry->value = "true";
          val = true;
          place_holder = -1;
        } else if (value == "true") {
          val = true;
        } else if (value == "false") {
          val = false;
        } else {
          pe = make_err(bad_value, entry->key, value,
                        /* TRANSLATORS: "true" and "false" are literal
                         * values and should not be translated.*/
                        _("either \"true\" or \"false\""));
          goto error;
        }

        NOTIFY_ALL(item_updated(ki, val));
        break;
        
      } case KeyInfoString:
        
        NOTIFY_ALL(item_updated(ki, value));
        break;
        
      case KeyInfoInt: 
      {
        int num;
        
        if (sscanf(value.str(), "%i", &num) == 1 && num >= 0) {
          NOTIFY_ALL(item_updated(ki, num));
        } else {
          pe = make_err(bad_value, entry->key, value, _("a positive integer"));
          goto error;
        }
        
        break;
      }
      case KeyInfoList:
        
        NOTIFY_ALL(list_updated(ki));
        break;
        
      }
      return place_holder;
    }
  error:
    entry->action = NoOp;
    if (!entry->file.empty())
      return pe.with_file(entry->file, entry->line_num);
    else
      return (PosibErrBase &)pe;
  }

#undef NOTIFY_ALL


  /////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////

  class PossibleElementsEmul : public KeyInfoEnumeration
  {
  private:
    bool include_extra;
    bool include_modules;
    bool module_changed;
    const Config * cd;
    const KeyInfo * i;
    const ConfigModule * m;
  public:
    PossibleElementsEmul(const Config * d, bool ic, bool im)
      : include_extra(ic), include_modules(im), 
        module_changed(false), cd(d), i(d->keyinfo_begin), m(0) {}

    KeyInfoEnumeration * clone() const {
      return new PossibleElementsEmul(*this);
    }

    void assign(const KeyInfoEnumeration * other) {
      *this = *(const PossibleElementsEmul *)(other);
    }

    virtual bool active_filter_module_changed(void) {
      return module_changed;
    }

    const char * active_filter_module_name(void){
      if (m != 0)
        return m->name;
      return "";
    }

    virtual const char * active_filter_module_desc(void) {
      if (m != 0)
        return m->desc;
      return "";
    }

    const KeyInfo * next() {
      if (i == cd->keyinfo_end) {
	if (include_extra)
	  i = cd->extra_begin;
	else
	  i = cd->extra_end;
      }
      
      module_changed = false;
      if (i == cd->extra_end) {
	m = cd->filter_modules.pbegin();
	if (!include_modules || m == cd->filter_modules.pend()) return 0;
	else {
          i = m->begin;
          module_changed = true;
        }
      }

      if (m == 0){
	return i++;
      }

      if (m == cd->filter_modules.pend()){
	return 0;
      }

      while (i == m->end) {
	++m;
	if (m == cd->filter_modules.pend()) return 0;
	else {
          i = m->begin;
          module_changed = true;
        }
      }

      return i++;
    }

    bool at_end() const {
      return (m == cd->filter_modules.pend());
    }
  };

  KeyInfoEnumeration *
  Config::possible_elements(bool include_extra, bool include_modules) const
  {
    return new PossibleElementsEmul(this, include_extra, include_modules);
  }

  struct ListDefaultDump : public AddableContainer 
  {
    OStream & out;
    bool first;
    const char * first_prefix;
    unsigned num_blanks;
    ListDefaultDump(OStream & o);
    PosibErr<bool> add(ParmStr d);
  };
  
  ListDefaultDump::ListDefaultDump(OStream & o) 
    : out(o), first(false)
  {
    first_prefix = _("# default: ");
    num_blanks = strlen(first_prefix) - 1;
  }

  PosibErr<bool> ListDefaultDump::add(ParmStr d) 
  {
    if (first) {
      out.write(first_prefix);
    } else {
      out.put('#');
      for (unsigned i = 0; i != num_blanks; ++i)
        out.put(' ');
    }
    VARARRAY(char, buf, d.size() * 2 + 1);
    escape(buf, d);
    out.printl(buf);
    first = false;
    return true;
  }

  class ListDump : public MutableContainer 
  {
    OStream & out;
    const char * name;
  public:
    ListDump(OStream & o, ParmStr n) 
      : out(o), name(n) {}
    PosibErr<bool> add(ParmStr d);
    PosibErr<bool> remove(ParmStr d);
    PosibErr<void> clear();
  };

  PosibErr<bool> ListDump::add(ParmStr d) {
    VARARRAY(char, buf, d.size() * 2 + 1);
    escape(buf, d);
    out.printf("add-%s %s\n", name, buf);
    return true;
  }
  PosibErr<bool> ListDump::remove(ParmStr d) {
    VARARRAY(char, buf, d.size() * 2 + 1);
    escape(buf, d);
    out.printf("remove-%s %s\n", name, buf);
    return true;
  }
  PosibErr<void> ListDump::clear() {
    out.printf("clear-%s\n", name);
    return no_err;
  }

  void Config::write_to_stream(OStream & out, 
			       bool include_extra) 
  {
    KeyInfoEnumeration * els = possible_elements(include_extra);
    const KeyInfo * i;
    String buf;
    String obuf;
    String def;
    bool have_value;

    while ((i = els->next()) != 0) {
      if (i->desc == 0) continue;

      if (els->active_filter_module_changed()) {
        out.printf(_("\n"
                     "#######################################################################\n"
                     "#\n"
                     "# Filter: %s\n"
                     "#   %s\n"
                     "#\n"
                     "# configured as follows:\n"
                     "\n"),
                   els->active_filter_module_name(),
                   _(els->active_filter_module_desc()));
      }

      obuf.clear();
      have_value = false;

      obuf.printf("# %s (%s)\n#   %s\n",
                  i->name, _(keyinfo_type_name[i->type]), _(i->desc));
      if (i->def != 0) {
	if (i->type != KeyInfoList) {
          buf.resize(strlen(i->def) * 2 + 1);
          escape(buf.data(), i->def);
          obuf.printf("# default: %s", buf.data());
          def = get_default(i);
          if (def != i->def) {
            buf.resize(def.size() * 2 + 1);
            escape(buf.data(), def.str());
            obuf.printf(" = %s", buf.data());
          }
          obuf << '\n';
          const Entry * entry = lookup(i->name);
	  if (entry) {
            have_value = true;
            buf.resize(entry->value.size() * 2 + 1);
            escape(buf.data(), entry->value.str());
	    obuf.printf("%s %s\n", i->name, buf.data());
          }
	} else {
          unsigned s = obuf.size();
          ListDump ld(obuf, i->name);
          lookup_list(i, ld, false);
          have_value = s != obuf.size();
	}
      }
      obuf << '\n';
      if (!(i->flags & KEYINFO_HIDDEN) || have_value)
        out.write(obuf);
    }
    delete els;
  }

  PosibErr<void> Config::read_in(IStream & in, ParmStr id) 
  {
    String buf;
    DataPair dp;
    while (getdata_pair(in, dp, buf)) {
      to_lower(dp.key);
      Entry * entry = new Entry;
      entry->key = dp.key;
      entry->value = dp.value;
      entry->file = id;
      entry->line_num = dp.line_num;
      RET_ON_ERR(set(entry, true));
    }
    return no_err;
  }

  PosibErr<void> Config::read_in_file(ParmStr file) {
    FStream in;
    RET_ON_ERR(in.open(file, "r"));
    return read_in(in, file);
  }

  PosibErr<void> Config::read_in_string(ParmStr str, const char * what) {
    StringIStream in(str);
    return read_in(in, what);
  }


  PosibErr<bool> Config::read_in_settings(const Config * other)
  {
    if (settings_read_in_) return false;

    bool was_committed = committed_;
    set_committed_state(false);

    if (other && other->settings_read_in_) {

      assert(empty());
      del(); // to clean up any notifiers and similar stuff
      copy(*other);

    } else {

      if (other) merge(*other);

      const char * env = getenv("ASPELL_CONF");
      if (env != 0) { 
        insert_point_ = &first_;
        RET_ON_ERR(read_in_string(env, _("ASPELL_CONF env var")));
      }
      
      {
        insert_point_ = &first_;
        PosibErrBase pe = read_in_file(retrieve("per-conf-path"));
        if (pe.has_err() && !pe.has_err(cant_read_file)) return pe;
      }
      
      {
        insert_point_ = &first_;
        PosibErrBase pe = read_in_file(retrieve("conf-path"));
        if (pe.has_err() && !pe.has_err(cant_read_file)) return pe;
      }

      if (was_committed)
        RET_ON_ERR(commit_all());

      settings_read_in_ = true;
    }

    return true;
  }

  PosibErr<void> Config::commit_all(Vector<int> * phs, const char * codeset)
  {
    committed_ = true;
    Entry * uncommitted = first_;
    first_ = 0;
    insert_point_ = &first_;
    Conv to_utf8;
    if (codeset)
      RET_ON_ERR(to_utf8.setup(*this, codeset, "utf-8", NormTo));
    PosibErr<void> ret;
    while (uncommitted) {
      Entry * cur = uncommitted;
      uncommitted = cur->next;
      cur->next = 0;
      *insert_point_ = cur;
      insert_point_ = &((*insert_point_)->next);
      PosibErr<int> pe = commit(cur, codeset ? &to_utf8 : 0);
      if (pe.has_err()) {
        if (ret.has_err())
          pe.ignore_err();
        else
          ret = pe;
        continue;
      }
      int place_holder = pe.data;
      if (phs && place_holder != -1 && (phs->empty() || phs->back() != place_holder))
        phs->push_back(place_holder);
    }
    return ret;
  }

  PosibErr<void> Config::set_committed_state(bool val) {
    if (val && !committed_) {
      RET_ON_ERR(commit_all());
    } else if (!val && committed_) {
      assert(empty());
      committed_ = false;
    }
    return no_err;
  }


#ifdef ENABLE_WIN32_RELOCATABLE
#  define HOME_DIR "<prefix>"
#  define PERSONAL "<lang>.pws"
#  define REPL     "<lang>.prepl"
#else
#  define HOME_DIR "<$HOME|./>"
#  define PERSONAL ".aspell.<lang>.pws"
#  define REPL     ".aspell.<lang>.prepl"
#endif

  static const KeyInfo config_keys[] = {
    // the description should be under 50 chars
    {"actual-dict-dir", KeyInfoString, "<dict-dir^master>", 0}
    , {"actual-lang",     KeyInfoString, "", 0} 
    , {"conf",     KeyInfoString, "aspell.conf",
       /* TRANSLATORS: The remaining strings in config.cpp should be kept
          under 50 characters, begin with a lower case character and not
          include any trailing punctuation marks. */
       N_("main configuration file")}
    , {"conf-dir", KeyInfoString, CONF_DIR,
       N_("location of main configuration file")}
    , {"conf-path",     KeyInfoString, "<conf-dir/conf>", 0}
    , {"data-dir", KeyInfoString, DATA_DIR,
       N_("location of language data files")}
    , {"dict-alias", KeyInfoList, "",
       N_("create dictionary aliases")}
    , {"dict-dir", KeyInfoString, DICT_DIR,
       N_("location of the main word list")}
    , {"encoding",   KeyInfoString, "!encoding",
       N_("encoding to expect data to be in"), KEYINFO_COMMON}
    , {"filter",   KeyInfoList  , "url",
       N_("add or removes a filter"), KEYINFO_MAY_CHANGE}
    , {"filter-path", KeyInfoList, DICT_DIR,
       N_("path(s) aspell looks for filters")}
    //, {"option-path", KeyInfoList, DATA_DIR,
    //   N_("path(s) aspell looks for options descriptions")}
    , {"mode",     KeyInfoString, "url",
       N_("filter mode"), KEYINFO_COMMON}
    , {"extra-dicts", KeyInfoList, "",
       N_("extra dictionaries to use")}
    , {"wordlists", KeyInfoList, "",
       N_("files with list of extra words to accept")}
    , {"home-dir", KeyInfoString, HOME_DIR,
       N_("location for personal files")}
    , {"ignore",   KeyInfoInt   , "1",
       N_("ignore words <= n chars"), KEYINFO_MAY_CHANGE}
    , {"ignore-accents" , KeyInfoBool, "false",
       /* TRANSLATORS: It is OK if this is longer than 50 chars */
       N_("ignore accents when checking words -- CURRENTLY IGNORED"), KEYINFO_MAY_CHANGE | KEYINFO_HIDDEN}
    , {"ignore-case", KeyInfoBool  , "false",
       N_("ignore case when checking words"), KEYINFO_MAY_CHANGE}
    , {"ignore-repl", KeyInfoBool  , "false",
       N_("ignore commands to store replacement pairs"), KEYINFO_MAY_CHANGE}
    , {"jargon",     KeyInfoString, "",
       N_("extra information for the word list"), KEYINFO_HIDDEN}
    , {"keyboard", KeyInfoString, "standard",
       N_("keyboard definition to use for typo analysis")}
    , {"lang", KeyInfoString, "<language-tag>",
       N_("language code"), KEYINFO_COMMON}
    , {"language-tag", KeyInfoString, "!lang",
       N_("deprecated, use lang instead"), KEYINFO_HIDDEN}
    , {"local-data-dir", KeyInfoString, "<actual-dict-dir>",
       N_("location of local language data files")     }
    , {"master",        KeyInfoString, "<lang>",
       N_("base name of the main dictionary to use"), KEYINFO_COMMON}
    , {"master-flags",  KeyInfoString, "", 0}
    , {"master-path",   KeyInfoString, "<dict-dir/master>",   0}
    , {"module",        KeyInfoString, "default",
       N_("set module name"), KEYINFO_HIDDEN}
    , {"module-search-order", KeyInfoList, "",
       N_("search order for modules"), KEYINFO_HIDDEN}
    , {"normalize", KeyInfoBool, "true",
       N_("enable Unicode normalization")}
    , {"norm-required", KeyInfoBool, "false",
       N_("Unicode normalization required for current lang")}
    , {"norm-form", KeyInfoString, "nfc",
       /* TRANSLATORS: the values after the ':' are literal
          values and should not be translated. */
       N_("Unicode normalization form: none, nfd, nfc, comp")}
    , {"norm-strict", KeyInfoBool, "false",
       N_("avoid lossy conversions when normalization")}
    , {"per-conf", KeyInfoString, ".aspell.conf",
       N_("personal configuration file")}
    , {"per-conf-path", KeyInfoString, "<home-dir/per-conf>", 0}
    , {"personal", KeyInfoString, PERSONAL,
       N_("personal dictionary file name")}
    , {"personal-path", KeyInfoString, "<home-dir/personal>", 0}
    , {"prefix",   KeyInfoString, PREFIX,
       N_("prefix directory")}
    , {"repl",     KeyInfoString, REPL,
       N_("replacements list file name") }
    , {"repl-path",     KeyInfoString, "<home-dir/repl>",     0}
    , {"run-together",        KeyInfoBool,  "false",
       N_("consider run-together words legal"), KEYINFO_MAY_CHANGE}
    , {"run-together-limit",  KeyInfoInt,   "2",
       N_("maximum number that can be strung together"), KEYINFO_MAY_CHANGE}
    , {"run-together-min",    KeyInfoInt,   "3",
       N_("minimal length of interior words"), KEYINFO_MAY_CHANGE}
    , {"camel-case", KeyInfoBool,  "false",
       N_("consider camel case words legal"), KEYINFO_MAY_CHANGE}
    , {"save-repl", KeyInfoBool  , "true",
       N_("save replacement pairs on save all")}
    , {"set-prefix", KeyInfoBool, "true",
       N_("set the prefix based on executable location")}
    , {"size",          KeyInfoString, "+60",
       N_("size of the word list")}
    , {"spelling",   KeyInfoString, "",
       N_("no longer used"), KEYINFO_HIDDEN}
    , {"sug-mode",   KeyInfoString, "normal",
       N_("suggestion mode"), KEYINFO_MAY_CHANGE | KEYINFO_COMMON}
    , {"sug-typo-analysis", KeyInfoBool, "true",
       /* TRANSLATORS: "sug-mode" is a literal value and should not be
          translated. */
       N_("use typo analysis, override sug-mode default")}
    , {"sug-repl-table", KeyInfoBool, "true",
       N_("use replacement tables, override sug-mode default")}
    , {"sug-split-char", KeyInfoList, "\\ :-",
       N_("characters to insert when a word is split"), KEYINFO_UTF8}
    , {"use-other-dicts", KeyInfoBool, "true",
       N_("use personal, replacement & session dictionaries")}
    , {"variety", KeyInfoList, "",
       N_("extra information for the word list")}
    , {"word-list-path", KeyInfoList, DATA_DIR,
       N_("search path for word list information files"), KEYINFO_HIDDEN}
    , {"warn", KeyInfoBool, "true",
       N_("enable warnings")}
    
    
    //
    // These options are generally used when creating dictionaries
    // and may also be specified in the language data file
    //

    , {"affix-char",          KeyInfoString, "/", // FIXME: Implement
       /* TRANSLATORS: It is OK if this is longer than 50 chars */
       N_("indicator for affix flags in word lists -- CURRENTLY IGNORED"), KEYINFO_UTF8 | KEYINFO_HIDDEN}
    , {"affix-compress", KeyInfoBool, "false",
       N_("use affix compression when creating dictionaries")}
    , {"clean-affixes", KeyInfoBool, "true",
       N_("remove invalid affix flags")}
    , {"clean-words", KeyInfoBool, "false",
       N_("attempts to clean words so that they are valid")}
    , {"invisible-soundslike", KeyInfoBool, "false",
       N_("compute soundslike on demand rather than storing")} 
    , {"partially-expand",  KeyInfoBool, "false",
       N_("partially expand affixes for better suggestions")}
    , {"skip-invalid-words",  KeyInfoBool, "true",
       N_("skip invalid words")}
    , {"validate-affixes", KeyInfoBool, "true",
       N_("check if affix flags are valid")}
    , {"validate-words", KeyInfoBool, "true",
       N_("check if words are valid")}
    
    //
    // These options are specific to the "aspell" utility.  They are
    // here so that they can be specified in configuration files.
    //
    , {"backup",  KeyInfoBool, "true",
       N_("create a backup file by appending \".bak\"")}
    , {"byte-offsets", KeyInfoBool, "false",
       N_("use byte offsets instead of character offsets")}
    , {"guess", KeyInfoBool, "false",
       N_("create missing root/affix combinations"), KEYINFO_MAY_CHANGE}
    , {"keymapping", KeyInfoString, "aspell",
       N_("keymapping for check mode: \"aspell\" or \"ispell\"")}
    , {"reverse", KeyInfoBool, "false",
       N_("reverse the order of the suggest list")}
    , {"suggest", KeyInfoBool, "true",
       N_("suggest possible replacements"), KEYINFO_MAY_CHANGE}
    , {"time"   , KeyInfoBool, "false",
       N_("time load time and suggest time in pipe mode"), KEYINFO_MAY_CHANGE}
    };

  const KeyInfo * config_impl_keys_begin = config_keys;
  const KeyInfo * config_impl_keys_end   
  = config_keys + sizeof(config_keys)/sizeof(KeyInfo);

  Config * new_basic_config() { 
    aspell_gettext_init();
    return new Config("aspell",
		      config_impl_keys_begin,
		      config_impl_keys_end);
  }
  
}

