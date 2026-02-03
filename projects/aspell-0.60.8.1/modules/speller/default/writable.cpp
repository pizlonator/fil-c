// This file is part of The New Aspell
// Copyright (C) 2000,2011 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include <time.h>

#include "hash-t.hpp"
#include "data.hpp"
#include "data_util.hpp"
#include "enumeration.hpp"
#include "errors.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "language.hpp"
#include "getdata.hpp"

namespace {

//////////////////////////////////////////////////////////////////////
//
// WritableBase
//

using namespace std;
using namespace aspeller;
using namespace acommon;

typedef const char * Str;
typedef unsigned char byte;

struct Hash {
  InsensitiveHash<> f;
  Hash(const Language * l) : f(l) {}
  size_t operator() (Str s) const {
    return f(s);
  }
};

struct Equal {
  InsensitiveEqual f;
  Equal(const Language * l) : f(l) {}
  bool operator() (Str a, Str b) const {
    return f(a, b);
  }
};

void write_n_escape(FStream & o, const char * str) {
  while (*str != '\0') {
    if (*str == '\n') o << "\\n";
    else if (*str == '\r') o << "\\r";
    else if (*str == '\\') o << "\\\\";
    else o << *str;
    ++str;
  }
}

static inline char f_getc(FStream & in) {
  int c = in.get();
  return c == EOF ? '\0' : (char)c;
}
  
bool getline_n_unescape(FStream & in, String & str, char delem) {
  str.clear();
  char c = f_getc(in);
  if (!c) return false;
  while (c && c != delem) {
    if (c == '\\') {
      c = f_getc(in);
      if (c == 'n') str.append('\n');
      else if (c == 'r') str.append('\r');
      else if (c == '\\') str.append('\\');
      else {str.append('\\'); continue;}
    } else {
      str.append(c);
    }
    c = f_getc(in);
  }
  return true;
}

bool getline_n_unescape(FStream & in, DataPair & d, String & buf)
{
  if (!getline_n_unescape(in, buf, '\n')) return false;
  d.value.str  = buf.mstr();
  d.value.size = buf.size();
  return true;
}

typedef Vector<Str> StrVector;

typedef hash_multiset<Str,Hash,Equal> WordLookup;
typedef hash_map<Str,StrVector>  SoundslikeLookup;

class WritableBase : public Dictionary {
protected:
  String suffix;
  String compatibility_suffix;
    
  time_t cur_file_date;
  
  String compatibility_file_name;
    
  WritableBase(BasicType t, const char * n, const char * s, const char * cs, const Config & cfg)
    : Dictionary(t,n),
      suffix(s), compatibility_suffix(cs),
      use_soundslike(true) {
    fast_lookup = true;
    validate_words = cfg.retrieve_bool("validate-words");
  }
  virtual ~WritableBase() {}
  
  virtual PosibErr<void> save(FStream &, ParmString) = 0;
  virtual PosibErr<void> merge(FStream &, ParmString, Config * = 0) = 0;
    
  PosibErr<void> save2(FStream &, ParmString);
  PosibErr<void> update(FStream &, ParmString);
  PosibErr<void> save(bool do_update);
  PosibErr<void> update_file_date_info(FStream &);
  PosibErr<void> load(ParmString, Config &, DictList *, SpellerImpl *);
  PosibErr<void> merge(ParmString);
  PosibErr<void> save_as(ParmString);
  PosibErr<void> clear();

  String file_encoding;
  ConvObj iconv;
  ConvObj oconv;
  PosibErr<void> set_file_encoding(ParmString, Config & c);

  PosibErr<void> synchronize() {return save(true);}
  PosibErr<void> save_noupdate() {return save(false);}

  bool use_soundslike;
  StackPtr<WordLookup> word_lookup;
  SoundslikeLookup     soundslike_lookup_;
  ObjStack             buffer;
 
  void set_lang_hook(Config & c) {
    set_file_encoding(lang()->data_encoding(), c);
    word_lookup.reset(new WordLookup(10, Hash(lang()), Equal(lang())));
    use_soundslike = lang()->have_soundslike();
  }
};

PosibErr<void> WritableBase::update_file_date_info(FStream & f) {
  RET_ON_ERR(update_file_info(f));
  cur_file_date = get_modification_time(f);
  return no_err;
}
  
PosibErr<void> WritableBase::load(ParmString f0, Config & config,
                                  DictList *, SpellerImpl *)
{
  set_file_name(f0);
  const String f = file_name();
  FStream in;

  if (file_exists(f)) {
      
    RET_ON_ERR(open_file_readlock(in, f));
    if (in.peek() == EOF) return make_err(cant_read_file,f); 
    // ^^ FIXME 
    RET_ON_ERR(merge(in, f, &config));
      
  } else if (f.size() >= suffix.size() &&
             f.substr(f.size()-suffix.size(),suffix.size())
             == suffix) {
      
    compatibility_file_name = f.substr(0,f.size() - suffix.size());
    compatibility_file_name += compatibility_suffix;
      
    {
      PosibErr<void> pe = open_file_readlock(in, compatibility_file_name);
      if (pe.has_err()) {compatibility_file_name = ""; return pe;}
    } {
      PosibErr<void> pe = merge(in, compatibility_file_name, &config);
      if (pe.has_err()) {compatibility_file_name = ""; return pe;}
    }
      
  } else {
      
    return make_err(cant_read_file,f);
      
  }

  return update_file_date_info(in);
}

PosibErr<void> WritableBase::merge(ParmString f0) {
  FStream in;
  Dict::FileName fn(f0);
  RET_ON_ERR(open_file_readlock(in, fn.path()));
  RET_ON_ERR(merge(in, fn.path()));
  return no_err;
}

PosibErr<void> WritableBase::update(FStream & in, ParmString fn) {
  typedef PosibErr<void> Ret;
  {
    Ret pe = merge(in, fn);
    if (pe.has_err() && compatibility_file_name.empty()) return pe;
  } {
    Ret pe = update_file_date_info(in);
    if (pe.has_err() && compatibility_file_name.empty()) return pe;
  }
  return no_err;
}
    
PosibErr<void> WritableBase::save2(FStream & out, ParmString fn) {
  truncate_file(out, fn);
      
  RET_ON_ERR(save(out,fn));

  out.flush();

  return no_err;
}

PosibErr<void> WritableBase::save_as(ParmString fn) {
  compatibility_file_name = "";
  set_file_name(fn);
  FStream inout;
  RET_ON_ERR(open_file_writelock(inout, file_name()));
  RET_ON_ERR(save2(inout, file_name()));
  RET_ON_ERR(update_file_date_info(inout));
  return no_err;
}

PosibErr<void> WritableBase::save(bool do_update) {
  FStream inout;
  RET_ON_ERR_SET(open_file_writelock(inout, file_name()),
                 bool, prev_existed);

  if (do_update
      && prev_existed 
      && get_modification_time(inout) > cur_file_date)
    RET_ON_ERR(update(inout, file_name()));

  RET_ON_ERR(save2(inout, file_name()));
  RET_ON_ERR(update_file_date_info(inout));
    
  if (compatibility_file_name.size() != 0) {
    remove_file(compatibility_file_name.c_str());
    compatibility_file_name = "";
  }

  return no_err;
}

PosibErr<void> WritableBase::clear() {
  word_lookup->clear();
  soundslike_lookup_.clear();
  buffer.reset();
  return no_err;
}

PosibErr<void> WritableBase::set_file_encoding(ParmString enc, Config & c)
{
  if (enc == file_encoding) return no_err;
  if (enc == "") enc = lang()->charmap();
  RET_ON_ERR(iconv.setup(c, enc, lang()->charmap(), NormFrom));
  RET_ON_ERR(oconv.setup(c, lang()->charmap(), enc, NormTo));
  if (iconv || oconv) 
    file_encoding = enc;
  else
    file_encoding = "";
  return no_err;
}


/////////////////////////////////////////////////////////////////////
// 
//  Common Stuff
//

// a word is stored in memory as follows
//   <word info><size><word...><null>
// the hash table points to the word and not the start of the block

static inline void set_word(WordEntry & res, Str w)
{
  res.word      = w;
  res.word_size = (byte)w[-1];
  res.word_info = (byte)w[-2];
  res.aff       = "";
}

// a soundslike is stored in memory as follows
//   <word info><size><sl...><null>
// the hash table points to the sl and not the start of the block

static inline void set_sl(WordEntry & res, Str w)
{
  res.word      = w;
  res.word_size = (byte)w[-1];
}

static void soundslike_next(WordEntry * w)
{
  const Str * & i   = (const Str * &)(w->intr[0]);
  const Str *   end = (const Str *  )(w->intr[1]);
  set_word(*w, *i);
  ++i;
  if (i == end) w->adv_ = 0;
}

static void sl_init(const StrVector * tmp, WordEntry & o)
{
  const Str * i   = tmp->pbegin();
  const Str * end = tmp->pend();
  set_word(o, *i);
  ++i;
  if (i != end) {
    o.intr[0] = (void *)i;
    o.intr[1] = (void *)end;
    o.adv_ = soundslike_next;
  } else {
    o.intr[0] = 0;
  }
}

struct SoundslikeElements : public SoundslikeEnumeration {

  typedef SoundslikeLookup::const_iterator Itr;

  Itr i;
  Itr end;

  WordEntry d;

  SoundslikeElements(Itr i0, Itr end0) : i(i0), end(end0) {
    d.what = WordEntry::Soundslike;
  }

  WordEntry * next(int) {
    if (i == end) return 0;
    set_sl(d, i->first);
    d.intr[0] = (void *)(&i->second);
    ++i;
    return &d;
  }
};

struct CleanElements : public SoundslikeEnumeration {

  typedef WordLookup::const_iterator Itr;

  Itr i;
  Itr end;

  WordEntry d;

  CleanElements(Itr i0, Itr end0) : i(i0), end(end0) {
    d.what = WordEntry::Word;
  }

  WordEntry * next(int) {
    if (i == end) return 0;
    set_word(d, *i);
    ++i;
    return &d;
  }
};

struct ElementsParms {
  typedef WordEntry *                Value;
  typedef WordLookup::const_iterator Iterator;
  Iterator end_;
  WordEntry data;
  ElementsParms(Iterator e) : end_(e) {}
  bool endf(Iterator i) const {return i==end_;}
  Value deref(Iterator i) {set_word(data, *i); return &data;}
  static Value end_state() {return 0;}
};

/////////////////////////////////////////////////////////////////////
// 
//  WritableDict
//

class WritableDict : public WritableBase
{
public: //but don't use
  PosibErr<void> save(FStream &, ParmString);
  PosibErr<void> merge(FStream &, ParmString, Config * config);

public:

  WritableDict(const Config & cfg)
    : WritableBase(basic_dict, "WritableDict", ".pws", ".per", cfg) {}

  Size   size()     const;
  bool   empty()    const;
  
  PosibErr<void> add(ParmString w) {return Dictionary::add(w);}
  PosibErr<void> add(ParmString w, ParmString s);

  bool lookup(ParmString word, const SensitiveCompare *, WordEntry &) const;

  bool clean_lookup(ParmString sondslike, WordEntry &) const;

  bool soundslike_lookup(const WordEntry & soundslike, WordEntry &) const;
  bool soundslike_lookup(ParmString soundslike, WordEntry &) const;

  WordEntryEnumeration * detailed_elements() const;

  SoundslikeEnumeration * soundslike_elements() const;
};

WritableDict::Size WritableDict::size() const 
{
  return word_lookup->size();
}

bool WritableDict::empty() const 
{
  return word_lookup->empty();
}

bool WritableDict::lookup(ParmString word, const SensitiveCompare * c,
                          WordEntry & o) const
{
  o.clear();
  pair<WordLookup::iterator, WordLookup::iterator> p(word_lookup->equal_range(word));
  while (p.first != p.second) {
    if ((*c)(word,*p.first)) {
      o.what = WordEntry::Word;
      set_word(o, *p.first);
      return true;
    }
    ++p.first;
  }
  return false;
}

bool WritableDict::clean_lookup(ParmString sl, WordEntry & o) const
{
  o.clear();
  pair<WordLookup::iterator, WordLookup::iterator> p(word_lookup->equal_range(sl));
  if (p.first == p.second) return false;
  o.what = WordEntry::Word;
  set_word(o, *p.first);
  return true;
  // FIXME: Deal with multiple entries
}  

bool WritableDict::soundslike_lookup(const WordEntry & word, WordEntry & o) const 
{
  if (use_soundslike) {

    const StrVector * tmp 
      = (const StrVector *)(word.intr[0]);
    o.clear();

    o.what = WordEntry::Word;
    sl_init(tmp, o);

  } else {
      
    o.what = WordEntry::Word;
    o.word = word.word;
    o.word_size = word.word_size;
    o.word_info = word.word_info;
    o.aff  = "";
    
  }
  return true;
}

bool WritableDict::soundslike_lookup(ParmString word, WordEntry & o) const 
{
  if (use_soundslike) {

    o.clear();
    SoundslikeLookup::const_iterator i = soundslike_lookup_.find(word);
    if (i == soundslike_lookup_.end()) {
      return false;
    } else {
      o.what = WordEntry::Word;
      sl_init(&i->second, o);
      return true;
    }
  
  } else {

    return WritableDict::clean_lookup(word, o);

  }
}

SoundslikeEnumeration * WritableDict::soundslike_elements() const {
  if (use_soundslike)
    return new SoundslikeElements(soundslike_lookup_.begin(), 
                                  soundslike_lookup_.end());
  else
    return new CleanElements(word_lookup->begin(),
                             word_lookup->end());
}

WritableDict::Enum * WritableDict::detailed_elements() const {
  return new MakeEnumeration<ElementsParms>
    (word_lookup->begin(),ElementsParms(word_lookup->end()));
}

PosibErr<void> WritableDict::add(ParmString w, ParmString s) {
  if (validate_words)
    RET_ON_ERR(check_if_valid(*lang(),w));
  else
    RET_ON_ERR(check_if_sane(*lang(),w));
  SensitiveCompare c(lang());
  WordEntry we;
  if (WritableDict::lookup(w,&c,we)) return no_err;
  byte * w2;
  w2 = (byte *)buffer.alloc(w.size() + 3);
  *w2++ = lang()->get_word_info(w);
  *w2++ = w.size();
  memcpy(w2, w.str(), w.size() + 1);
  word_lookup->insert((char *)w2);
  if (use_soundslike) {
    byte * s2;
    s2 = (byte *)buffer.alloc(s.size() + 2);
    *s2++ = s.size();
    memcpy(s2, s.str(), s.size() + 1);
    soundslike_lookup_[(char *)s2].push_back((char *)w2);
  }
  return no_err;
}

PosibErr<void> WritableDict::merge(FStream & in, 
                                   ParmString file_name, 
                                   Config * config)
{
  typedef PosibErr<void> Ret;
  unsigned int ver;

  String buf;
  DataPair dp;

  if (!getline(in, dp, buf))
    make_err(bad_file_format, file_name);

  split(dp);
  if (dp.key == "personal_wl")
    ver = 10;
  else if (dp.key == "personal_ws-1.1")
    ver = 11;
  else 
    return make_err(bad_file_format, file_name);

  split(dp);
  {
    Ret pe = set_check_lang(dp.key, *config);
    if (pe.has_err())
      return pe.with_file(file_name);
  }

  split(dp); // count not used at the moment

  split(dp);
  if (dp.key.size > 0)
    set_file_encoding(dp.key, *config);
  else
    set_file_encoding("", *config);
  
  ConvP conv(iconv);
  while (getline_n_unescape(in, dp, buf)) {
    if (ver == 10)
      split(dp);
    else
      dp.key = dp.value;
    Ret pe = add(conv(dp.key));
    if (pe.has_err()) {
      clear();
      return pe.with_file(file_name);
    }
  }
  return no_err;
}

struct CStrLess {
  bool operator() (const char * x, const char * y) const {
    return strcmp(x, y) < 0;
  }
};

PosibErr<void> WritableDict::save(FStream & out, ParmString file_name) 
{
  out.printf("personal_ws-1.1 %s %i %s\n", 
             lang_name(), word_lookup->size(), file_encoding.c_str());

  Vector<const char *> words;
  words.reserve(word_lookup->size());
  for (WordLookup::const_iterator i = word_lookup->begin(), e = word_lookup->end();
       i != e; ++i)
    words.push_back(*i);
  std::sort(words.begin(), words.end(), CStrLess());
    
  ConvP conv(oconv);
  for (Vector<const char *>::const_iterator i = words.begin(), e = words.end();
       i != e; ++i) {
    write_n_escape(out, conv(*i));
    out << '\n';
  }
  return no_err;
}

/////////////////////////////////////////////////////////////////////
// 
//  WritableReplList
//

static inline StrVector * get_vector(Str s) 
{
  return (StrVector *)(s - sizeof(StrVector) - 2);
}

class WritableReplDict : public WritableBase
{
  WritableReplDict(const WritableReplDict&);
  WritableReplDict& operator=(const WritableReplDict&);

public:
  WritableReplDict(const Config & cfg)
    : WritableBase(replacement_dict, "WritableReplDict", ".prepl",".rpl", cfg) 
  {
    fast_lookup = true;
  }
  ~WritableReplDict();

  Size   size()     const;
  bool   empty()    const;

  bool lookup(ParmString, const SensitiveCompare *, WordEntry &) const;

  bool clean_lookup(ParmString sondslike, WordEntry &) const;

  bool soundslike_lookup(const WordEntry &, WordEntry &) const;
  bool soundslike_lookup(ParmString, WordEntry &) const;

  bool repl_lookup(const WordEntry &, WordEntry &) const;
  bool repl_lookup(ParmString, WordEntry &) const;
      
  WordEntryEnumeration * detailed_elements() const;
  SoundslikeEnumeration * soundslike_elements() const;
      
  PosibErr<void> add_repl(ParmString mis, ParmString cor) {
    return Dictionary::add_repl(mis,cor);}
  PosibErr<void> add_repl(ParmString mis, ParmString cor, ParmString s);

private:
  PosibErr<void> save(FStream &, ParmString );
  PosibErr<void> merge(FStream &, ParmString , Config * config);
};

WritableReplDict::Size WritableReplDict::size() const 
{
  return word_lookup->size();
}

bool WritableReplDict::empty() const 
{
  return word_lookup->empty();
}
    
bool WritableReplDict::lookup(ParmString word, const SensitiveCompare * c,
                              WordEntry & o) const
{
  o.clear();
  pair<WordLookup::iterator, WordLookup::iterator> p(word_lookup->equal_range(word));
  while (p.first != p.second) {
    if ((*c)(word,*p.first)) {
      o.what = WordEntry::Misspelled;
      set_word(o, *p.first);
      o.intr[0] = (void *)*p.first;
      return true;
    }
    ++p.first;
  }
  return false;
}

bool WritableReplDict::clean_lookup(ParmString sl, WordEntry & o) const
{
  o.clear();
  pair<WordLookup::iterator, WordLookup::iterator> p(word_lookup->equal_range(sl));
  if (p.first == p.second) return false;
  o.what = WordEntry::Misspelled;
  set_word(o, *p.first);
  o.intr[0] = (void *)*p.first;
  return true;
  // FIXME: Deal with multiple entries
}  

bool WritableReplDict::soundslike_lookup(const WordEntry & word, WordEntry & o) const 
{
  if (use_soundslike) {
    const StrVector * tmp = (const StrVector *)(word.intr[0]);
    o.clear();
    o.what = WordEntry::Misspelled;
    sl_init(tmp, o);
  } else {
    o.what = WordEntry::Misspelled;
    o.word = word.word;
    o.word_size = word.word_size;
    o.aff = "";
  }
  return true;
}

bool WritableReplDict::soundslike_lookup(ParmString soundslike, WordEntry & o) const
{
  if (use_soundslike) {
    o.clear();
    SoundslikeLookup::const_iterator i = soundslike_lookup_.find(soundslike);
    if (i == soundslike_lookup_.end()) {
      return false;
    } else {
      o.what = WordEntry::Misspelled;
      sl_init(&(i->second), o);
      return true;
    }
  } else {
    return WritableReplDict::clean_lookup(soundslike, o);
  }
}

SoundslikeEnumeration * WritableReplDict::soundslike_elements() const {
  if (use_soundslike)
    return new SoundslikeElements(soundslike_lookup_.begin(), 
                                  soundslike_lookup_.end());
  else
    return new CleanElements(word_lookup->begin(),
                             word_lookup->end());
}

WritableReplDict::Enum * WritableReplDict::detailed_elements() const {
  return new MakeEnumeration<ElementsParms>
    (word_lookup->begin(),ElementsParms(word_lookup->end()));
}

static void repl_next(WordEntry * w)
{
  const Str * & i   = (const Str * &)(w->intr[0]);
  const Str *   end = (const Str *  )(w->intr[1]);
  set_word(*w, *i);
  ++i;
  if (i == end) w->adv_ = 0;
}

static void repl_init(const StrVector * tmp, WordEntry & o)
{
  o.what = WordEntry::Word;
  const Str * i   = tmp->pbegin();
  const Str * end = tmp->pend();
  set_word(o, *i);
  ++i;
  if (i != end) {
    o.intr[0] = (void *)i;
    o.intr[1] = (void *)end;
    o.adv_ = repl_next;
  } else {
    o.intr[0] = 0;
  }
}
  
bool WritableReplDict::repl_lookup(const WordEntry & w, WordEntry & o) const 
{
  const StrVector * repls;
  if (w.intr[0] && !w.intr[1]) { // the intr are not for the sl iter
    repls = get_vector(w.word);
  } else {
    SensitiveCompare c(lang()); // FIXME: This is not exactly right
    WordEntry tmp;
    WritableReplDict::lookup(w.word, &c, tmp);
    repls = get_vector(tmp.word);
    if (!repls) return false;
  }
  o.clear();
  repl_init(repls, o);
  return true;
}

bool WritableReplDict::repl_lookup(ParmString word, WordEntry & o) const 
{
  WordEntry w;
  w.word = word;
  return WritableReplDict::repl_lookup(w, o);
}

PosibErr<void> WritableReplDict::add_repl(ParmString mis, ParmString cor, ParmString sl) 
{
  Str m;
  SensitiveCompare cmp(lang()); // FIXME: I don't think this is completely correct
  WordEntry we;

  pair<WordLookup::iterator, WordLookup::iterator> p0(word_lookup->equal_range(mis));
  WordLookup::iterator p = p0.first;

  for (; p != p0.second && !cmp(mis,*p); ++p);

  if (p == p0.second) {
    byte * m0  = (byte *)buffer.alloc(sizeof(StrVector) + mis.size() + 3, sizeof(void *));
    new (m0) StrVector;
    m0 += sizeof(StrVector);
    *m0++ = lang()->get_word_info(mis);
    *m0++ = mis.size();
    memcpy(m0, mis.str(), mis.size() + 1);
    m = (char *)m0;
    p = word_lookup->insert(m).first;
  } else {
    m = *p;
  }

  StrVector * v = get_vector(m);

  for (StrVector::iterator i = v->begin(); i != v->end(); ++i)
    if (cmp(cor, *i)) return no_err;
    
  byte * c0 = (byte *)buffer.alloc(cor.size() + 3);
  *c0++ = lang()->get_word_info(cor);
  *c0++ = cor.size();
  memcpy(c0, cor.str(), cor.size() + 1);
  v->push_back((char *)c0);

  if (use_soundslike) {
    byte * s0 = (byte *)buffer.alloc(sl.size() + 2);
    *s0++ = sl.size();
    memcpy(s0, sl.str(), sl.size() + 1);
    soundslike_lookup_[(char *)s0].push_back(m);
  }

  return no_err;
}

PosibErr<void> WritableReplDict::save (FStream & out, ParmString file_name) 
{
  out.printf("personal_repl-1.1 %s 0 %s\n", lang_name(), file_encoding.c_str());
  
  Vector<const char *> words;
  words.reserve(word_lookup->size());
  for (WordLookup::const_iterator i = word_lookup->begin(), e = word_lookup->end();
       i != e; ++i)
    words.push_back(*i);
  std::sort(words.begin(), words.end(), CStrLess());

  ConvP conv1(oconv);
  ConvP conv2(oconv);

  Vector<const char *> v;
  for (Vector<const char *>::const_iterator i = words.begin(), e = words.end();
       i != e; ++i) 
  {
    v = *get_vector(*i); // make a copy
    std::sort(v.begin(), v.end(), CStrLess());
    for (StrVector::iterator j = v.begin(); j != v.end(); ++j)
    {
      write_n_escape(out, conv1(*i));
      out << ' ';
      write_n_escape(out, conv2(*j));
      out << '\n';
    }
  }
  return no_err;
}

PosibErr<void> WritableReplDict::merge(FStream & in,
                                       ParmString file_name, 
                                       Config * config)
{
  typedef PosibErr<void> Ret;
  unsigned int version;
  unsigned int num_words, num_repls;

  String buf;
  DataPair dp;

  if (!getline(in, dp, buf))
    make_err(bad_file_format, file_name);

  split(dp);
  if (dp.key == "personal_repl")
    version = 10;
  else if (dp.key == "personal_repl-1.1") 
    version = 11;
  else
    return make_err(bad_file_format, file_name);

  split(dp);
  {
    Ret pe = set_check_lang(dp.key, *config);
    if (pe.has_err())
      return pe.with_file(file_name);
  }

  unsigned int num_soundslikes = 0;
  if (version == 10) {
    split(dp);
    num_soundslikes = atoi(dp.key);
  }

  split(dp); // not used at the moment

  split(dp);
  if (dp.key.size > 0)
    set_file_encoding(dp.key, *config);
  else
    set_file_encoding("", *config);

  if (version == 11) {

    ConvP conv1(iconv);
    ConvP conv2(iconv);
    for (;;) {
      bool res = getline_n_unescape(in, buf, '\n');
      if (!res) break;
      char * mis = buf.mstr();
      char * repl = strchr(mis, ' ');
      if (!repl) continue; // bad line, ignore
      *repl = '\0'; // split string
      ++repl;
      if (!repl[0]) continue; // empty repl, ignore
      WritableReplDict::add_repl(conv1(mis), conv2(repl));
    }
    
  } else {
    
    String mis, sound, repl;
    unsigned int h,i,j;
    for (h=0; h != num_soundslikes; ++h) {
      in >> sound >> num_words;
      for (i = 0; i != num_words; ++i) {
        in >> mis >> num_repls;
        in.ignore(); // ignore space
        for (j = 0; j != num_repls; ++j) {
          in.getline(repl, ',');
          WritableReplDict::add_repl(mis, repl);
        }
      }
    }

  }
  return no_err;
}

WritableReplDict::~WritableReplDict()
{
  WordLookup::iterator i = word_lookup->begin();
  WordLookup::iterator e = word_lookup->end();
  
  for (;i != e; ++i) 
    get_vector(*i)->~StrVector();
}

}

namespace aspeller {

  Dictionary * new_default_writable_dict(const Config & cfg) {
    return new WritableDict(cfg);
  }

  Dictionary * new_default_replacement_dict(const Config & cfg) {
    return new WritableReplDict(cfg);
  }

}
