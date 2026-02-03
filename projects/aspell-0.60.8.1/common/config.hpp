// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_CONFIG___HPP
#define ASPELL_CONFIG___HPP

#include "can_have_error.hpp"
#include "key_info.hpp"
#include "posib_err.hpp"
#include "string.hpp"
#include "vector.hpp"

namespace acommon {

  class OStream;
  class KeyInfoEnumeration;
  class StringPairEnumeration;
  class MutableContainer;
  class Cacheable;
  struct Conv;

  // The Config class is used to hold configuration information.
  // it has a set of keys which it will except.  Inserting or even
  // trying to look at a key that it does not know will produce
  // an error.  Extra accepted keys can be added with the set_extra 
  // method.

  // Keys tagged with KEYINFO_UTF8 are expected to be in UTF-8 format.
  // Keys with file/dir names may contain 8-bit characters and must
  //   remain untranslated
  // All other keys are expected to only contain ASCII characters.

  class Config;

  struct ConfigModule {
    const char * name; 
    const char * file; // path of shared object or dll
    const char * desc; // description of module
    const KeyInfo * begin;
    const KeyInfo * end;
  };

  class Notifier {
  public:
    // returns a copy if a copy should be made otherwise returns null
    virtual Notifier * clone(Config *) const {return 0;}
    virtual ~Notifier() {}

    virtual PosibErr<void> item_updated(const KeyInfo *, bool)    {return no_err;}
    virtual PosibErr<void> item_updated(const KeyInfo *, int)     {return no_err;}
    virtual PosibErr<void> item_updated(const KeyInfo *, ParmStr) {return no_err;}
    virtual PosibErr<void> list_updated(const KeyInfo *)          {return no_err;}
  };

  class PossibleElementsEmul;
  class NotifierEnumeration;
  class GetLine;
  class MDInfoListofLists;

  static const bool REPLACE = true;
  static const bool INSERT  = false;

  // prefixes
  //
  // reset - resets a value to the default
  //
  // enable - sets a boolean value to true
  // dont, disable - sets a boolean value to false
  // -- setting a boolean value to an empty string is the same as setting 
  //    it to true
  //
  // lset - sets a list, items separated by ':'
  // rem, remove - removes item from a list
  // add - add an item to a list
  // clear - removes all items from a list
  // -- setting a list item directly, ie with out a prefix, is the same as 
  //    setting it to a single value

  class Config : public CanHaveError {
    // copy and destructor provided
    friend class MDInfoListofLists;

  public:
    enum Action {NoOp, Set, Reset, Enable, Disable, 
                 ListSet, ListAdd, ListRemove, ListClear};

    struct Entry {
      Entry * next;
      String key;
      String value;
      String file;
      unsigned line_num;
      Action action;
      bool need_conv;
      bool secure; // if the value was set in a secure content
      short place_holder;
      Entry() : line_num(0), action(NoOp), 
                need_conv(false), secure(false), place_holder(-1) {}
    };

  private:
    static const int num_parms_[9];

  public:
    static inline bool list_action(Action a) {return (int)a >= (int)ListAdd; }
    static inline int num_parms(Action a) {return num_parms_[a];}

  private:
    String    name_;

    Entry * first_;
    Entry * * insert_point_;
    Entry * others_;

    bool committed_;
    bool attached_;    // if attached can't copy
    Vector<Notifier *> notifier_list;

    friend class PossibleElementsEmul;

    const KeyInfo       * keyinfo_begin;
    const KeyInfo       * keyinfo_end;
    const KeyInfo       * extra_begin;
    const KeyInfo       * extra_end;

    int md_info_list_index;

    void copy(const Config & other);
    void del();

    PosibErr<int> commit(Entry * entry, Conv * conv = 0);

    bool settings_read_in_;

  public:

    // the first
    // if the second parameter is set than flagged options will be
    // converted to utf-8 if needed
    PosibErr<void> commit_all(Vector<int> * = 0, const char * codeset = 0);

    PosibErr<void> replace(ParmStr, ParmStr);
    PosibErr<void> remove(ParmStr);

    bool empty() const {return !first_;}

    PosibErr<void> merge(const Config & other);

    PosibErr<void> lang_config_merge(const Config & other,
                                     int which, ParmStr codeset);

    bool settings_read_in() {return settings_read_in_;}

    PosibErr<void> set_committed_state(bool val);

    const Entry * lookup(const char * key) const;
    PosibErr<void> lookup_list(const KeyInfo * ki, MutableContainer & m, 
                               bool include_default) const;

    String temp_str;

    PosibErr<const ConfigModule *> (* load_filter_hook)(Config * config, ParmStr value);
    Notifier * filter_mode_notifier;

    Vector<ConfigModule> filter_modules;
    Vector<Cacheable *>  filter_modules_ptrs;

    Config(ParmStr name,
	   const KeyInfo * mainbegin, 
	   const KeyInfo * mainend);

    Config(const Config &);
    ~Config();
    Config & operator= (const Config &);

    bool get_attached() const {return attached_;}
    void set_attached(bool a) {attached_ = a;}

    Config * clone() const;
    void assign(const Config * other);

    const char * name() const {return name_.c_str();}

    NotifierEnumeration * notifiers() const;
  
    bool add_notifier    (      Notifier *);
    bool remove_notifier (const Notifier *);
    bool replace_notifier(const Notifier *, Notifier *);

    void set_extra(const KeyInfo * begin, const KeyInfo * end);

    void set_filter_modules(const ConfigModule * modbegin, const ConfigModule * modend);

    static const char * base_name(const char * name, Action * action = 0);
  
    PosibErr<const KeyInfo *> keyinfo(ParmStr key) const;

    KeyInfoEnumeration * possible_elements(bool include_extra = true,
                                           bool include_modules = true) const;
    
    StringPairEnumeration * elements() {return 0;} // FIXME
    
    String get_default(const KeyInfo * ki) const;
    PosibErr<String> get_default(ParmStr) const;

    PosibErr<String> retrieve(ParmStr key) const;

    struct Value {
      String val;
      bool secure;
      Value() {}
      Value(const String & v, bool s = false) : val(v), secure(s) {}
    };
    PosibErr<Value> retrieve_value(ParmStr key) const;

    // will also retrieve a list, with one value per line
    PosibErr<String> retrieve_any(ParmStr key) const;
  
    bool have (ParmStr key) const;

    PosibErr<void> retrieve_list (ParmStr key, MutableContainer *) const;
    PosibErr<bool> retrieve_bool (ParmStr key) const;
    PosibErr<int>  retrieve_int  (ParmStr key) const;
    
    // will take ownership of entry, even if there is an error
    PosibErr<void> set(Entry * entry, bool do_unescape = false);
      
    void replace_internal (ParmStr, ParmStr);
    
    void write_to_stream(OStream & out, bool include_extra = false);

    PosibErr<bool> read_in_settings(const Config * = 0);

    PosibErr<void> read_in(IStream & in, ParmStr id = "");
    PosibErr<void> read_in_file(ParmStr file);
    PosibErr<void> read_in_string(ParmStr str, const char * what = "");
  };

  Config * new_config();
  Config * new_basic_config(); // config which doesn't require any
			       // external symbols

  class NotifierEnumeration {
    // no copy and destructor needed
    Vector<Notifier *>::const_iterator i;
    Vector<Notifier *>::const_iterator end;
  public:
    NotifierEnumeration(const Vector<Notifier *> & b) 
      : i(b.begin()), end(b.end()) {}
    const Notifier * next() {
      const Notifier * temp = *i;
      if (i != end)
	++i;
      return temp;
    }
    bool at_end() const {return i == end;}
  };

  class KeyInfoEnumeration {
  public:
    typedef const KeyInfo * Value;
    virtual KeyInfoEnumeration * clone() const = 0;
    virtual void assign(const KeyInfoEnumeration *) = 0;
    virtual bool at_end() const = 0;
    virtual const KeyInfo * next() = 0;
    virtual const char * active_filter_module_name(void) = 0;
    virtual const char * active_filter_module_desc(void) = 0;
    virtual bool active_filter_module_changed(void) = 0;
    virtual ~KeyInfoEnumeration() {}
  };

  static const unsigned KEYINFO_MAY_CHANGE = 1 << 0;
  static const unsigned KEYINFO_UTF8       = 1 << 1;
  static const unsigned KEYINFO_HIDDEN     = 1 << 2;
  static const unsigned KEYINFO_COMMON     = 1 << 4;
  
  class AddableContainer;
  class StringList;

  void separate_list(ParmStr value, AddableContainer & out, bool do_unescape = true);
  void combine_list(String & res, const StringList &);


}

#endif

