// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <assert.h>
#include <string.h>
#include <math.h>

#include "asc_ctype.hpp"
#include "convert.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "stack_ptr.hpp"
#include "cache-t.hpp"
#include "file_util.hpp"
#include "file_data_util.hpp"
#include "vararray.hpp"

#include "iostream.hpp"

#include "gettext.h"

namespace acommon {

  typedef unsigned char  byte;
  typedef unsigned char  Uni8;
  typedef unsigned short Uni16;
  typedef unsigned int   Uni32;


  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////
  //
  // Lookups
  //
  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////
  //
  // ToUniLookup
  //

  class ToUniLookup 
  {
    Uni32 data[256];
    static const Uni32 npos = (Uni32)(-1);
  public:
    void reset();
    Uni32 operator[] (char key) const {return data[(unsigned char)key];}
    bool have(char key) const {return data[(unsigned char)key] != npos;}
    bool insert(char key, Uni32 value);
  };

  void ToUniLookup::reset() 
  {
    for (int i = 0; i != 256; ++i)
      data[i] = npos;
  }

  bool ToUniLookup::insert(char key, Uni32 value)
  {
    if (data[(unsigned char)key] != npos) 
      return false;
    data[(unsigned char)key] = value;
    return true;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // FromUniLookup
  //

  // Assumes that the maximum number of items in the table is 256
  // Also assumes (unsigned char)i == i % 256

  // Based on the iso-8859-* character sets it is very fast, almost all
  // lookups involving no more than 2 comparisons.
  // NO looks ups involded more than 3 compassions.
  // Also, no division (or modules) is done whatsoever.


  struct UniItem {
    Uni32 key;
    char  value;
  };

  class FromUniLookup 
  {
  private:
    static const Uni32 npos = (Uni32)(-1);
    UniItem * overflow_end;
  
    UniItem data[256*4];

    UniItem overflow[256]; // you can never be too careful;
  
  public:
    FromUniLookup() {}
    void reset();
    inline char operator() (Uni32 key, char unknown = '?') const;
    bool insert(Uni32 key, char value);
  };

  void FromUniLookup::reset()
  {
    for (unsigned i = 0; i != 256*4; ++i)
      data[i].key = npos;
    overflow_end = overflow;
  }

  inline char FromUniLookup::operator() (Uni32 k, char unknown) const
  {
    const UniItem * i = data + (unsigned char)k * 4;

    if (i->key == k) return i->value;
    ++i;
    if (i->key == k) return i->value;
    ++i;
    if (i->key == k) return i->value;
    ++i;
    if (i->key == k) return i->value;
  
    if (i->key == npos) return unknown;
  
    for(i = overflow; i != overflow_end; ++i)
      if (i->key == k) return i->value;

    return unknown;
  }

  bool FromUniLookup::insert(Uni32 k, char v) 
  {
    UniItem * i = data + (unsigned char)k * 4;
    UniItem * e = i + 4;
    while (i != e && i->key != npos) {
      if (i->key == k)
        return false;
      ++i;
    }
    if (i == e) {
      for(i = overflow; i != overflow_end; ++i)
        if (i->key == k) return false;
    }
    i->key = k;
    i->value = v;
    return true;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // CharLookup
  //

  class CharLookup 
  {
  private:
    int data[256];
  public:
    void reset();
    char operator[] (char key) const {return data[(unsigned char)key];}
    bool insert(char key, char value);
  };

  void CharLookup::reset() {
    for (int i = 0; i != 256; ++i) 
      data[i] = -1;
  }

  bool CharLookup::insert(char key, char value) 
  {
    if (data[(unsigned char)key] != -1)
      return false;
    data[(unsigned char)key] = value;
    return true;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // NormLookup
  //

  template <class T>
  struct NormTable
  {
    static const unsigned struct_size;
    unsigned mask;
    unsigned height;
    unsigned width;
    unsigned size;
    T * end;
    T data[1]; // hack for data[]
  };

  template <class T>
  const unsigned NormTable<T>::struct_size = sizeof(NormTable<T>) - 1;

  template <class T, class From>
  struct NormLookupRet
  {
    const typename T::To   * to;
    const From * last;
    NormLookupRet(const typename T::To * t, From * l) 
      : to(t), last(l) {}
  };
  
  template <class T, class From>
  static inline NormLookupRet<T,From> norm_lookup(const NormTable<T> * d, 
                                                  From * s, From * stop,
                                                  const typename T::To * def,
                                                  From * prev) 
  {
  loop:
    if (s != stop) {
      const T * i = d->data + (static_cast<typename T::From>(*s) & d->mask);
      for (;;) {
        if (i->from == static_cast<typename T::From>(*s)) {
          if (i->sub_table) {
            // really tail recursion
            if (i->to[1] != T::to_non_char) {def = i->to; prev = s;}
            d = (const NormTable<T> *)(i->sub_table);
            s++;
            goto loop;
          } else {
            return NormLookupRet<T,From>(i->to, s);
          }
        } else {
          i += d->height;
          if (i >= d->end) break;
        }
      }
    }
    return NormLookupRet<T,From>(def, prev);
  }

  template <class T>
  void free_norm_table(NormTable<T> * d)
  {
    for (T * cur = d->data; cur != d->end; ++cur) {
      if (cur->sub_table) 
        free_norm_table<T>(static_cast<NormTable<T> *>(cur->sub_table));
    }
    free(d);
  }

  struct FromUniNormEntry
  {
    typedef Uni32 From;
    Uni32 from;
    typedef byte To;
    byte  to[4];
    static const From from_non_char = (From)(-1);
    static const To   to_non_char   = 0x10;
    static const unsigned max_to = 4;
    void * sub_table;
  } 
#ifdef __GNUC__    
    __attribute__ ((aligned (16)))
#endif
  ;

  struct ToUniNormEntry
  {
    typedef byte From;
    byte from;
    typedef Uni16 To;
    Uni16 to[3];
    static const From from_non_char = 0x10;
    static const To   to_non_char   = 0x10;
    static const unsigned max_to = 3;
    void * sub_table;
  } 
#ifdef __GNUC__    
    __attribute__ ((aligned (16)))
#endif
  ;
  
  //////////////////////////////////////////////////////////////////////
  //
  // read in char data
  //

  PosibErr<void> read_in_char_data (const Config & config,
                                    ParmStr encoding,
                                    ToUniLookup & to,
                                    FromUniLookup & from)
  {
    to.reset();
    from.reset();
    
    String dir1,dir2,file_name;
    fill_data_dir(&config, dir1, dir2);
    find_file(file_name,dir1,dir2,encoding,".cset");

    FStream data;
    PosibErrBase err = data.open(file_name, "r");
    if (err.get_err()) { 
      char mesg[300];
      snprintf(mesg, 300, _("This could also mean that the file \"%s\" could not be opened for reading or does not exist."),
               file_name.c_str());
      return make_err(unknown_encoding, encoding, mesg);
    }
    unsigned chr;
    Uni32 uni;
    String line;
    char * p;
    do {
      p = get_nb_line(data, line);
    } while (*p != '/');
    for (chr = 0; chr != 256; ++chr) {
      p = get_nb_line(data, line);
      if (strtoul(p, 0, 16) != chr)
        return make_err(bad_file_format, file_name);
      uni = strtoul(p + 3, 0, 16);
      to.insert(chr, uni);
      from.insert(uni, chr);
    }
  
    return no_err;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // read in norm data
  //

  struct Tally 
  {
    int size;
    Uni32 mask;
    int max;
    int * data;
    Tally(int s, int * d) : size(s), mask(s - 1), max(0), data(d) {
      memset(data, 0, sizeof(int)*size);
    }
    void add(Uni32 chr) {
      Uni32 p = chr & mask;
      data[p]++;
      if (data[p] > max) max = data[p];
    }
  };

# define sanity(check) \
    if (!(check)) return sanity_fail(__FILE__, FUNC, __LINE__, #check)

  static PosibErrBase sanity_fail(const char * file, const char * func, 
                                  unsigned line, const char * check_str) 
  {
    char mesg[500];
    snprintf(mesg, 500, "%s:%d: %s: Assertion \"%s\" failed.",
             file,  line, func, check_str);
    return make_err(bad_input_error, mesg);
  }
# define CREATE_NORM_TABLE(T, in, buf, res) \
  do { PosibErr<NormTable<T> *> pe( create_norm_table<T>(in,buf) );\
       if (pe.has_err()) return PosibErrBase(pe); \
       res = pe.data; } while(false)

  template <class T>
  static PosibErr< NormTable<T> * > create_norm_table(IStream & in, String & buf)
  {
    const char FUNC[] = "create_norm_table";
    const char * p = get_nb_line(in, buf);
    sanity(*p == 'N');
    ++p;
    int size = strtoul(p, (char **)&p, 10);
    VARARRAY(T, d, size);
    memset(d, 0, sizeof(T) * size);
    int sz = 1 << (unsigned)floor(log(size <= 1 ? 1.0 : size - 1)/log(2.0));
    VARARRAY(int, tally0_d, sz);   Tally tally0(sz,   tally0_d);
    VARARRAY(int, tally1_d, sz*2); Tally tally1(sz*2, tally1_d);
    VARARRAY(int, tally2_d, sz*4); Tally tally2(sz*4, tally2_d);
    T * cur = d;
    while (p = get_nb_line(in, buf), *p != '.') {
      Uni32 f = strtoul(p, (char **)&p, 16);
      cur->from = static_cast<typename T::From>(f);
      sanity(f == cur->from);
      tally0.add(f);
      tally1.add(f);
      tally2.add(f);
      ++p;
      sanity(*p == '>');
      ++p;
      sanity(*p == ' ');
      ++p;
      unsigned i = 0;
      if (*p != '-') {
        for (;; ++i) {
          const char * q = p;
          Uni32 t = strtoul(p, (char **)&p, 16);
          if (q == p) break;
          sanity(i < d->max_to);
          cur->to[i] = static_cast<typename T::To>(t);
          sanity(t == static_cast<Uni32>(cur->to[i]));
        } 
      } else {
        cur->to[0] = 0;
        cur->to[1] = T::to_non_char;
      }
      if (*p == ' ') ++p;
      if (*p == '/') CREATE_NORM_TABLE(T, in, buf, cur->sub_table);
      ++cur;
    }
    sanity(cur - d == size);
    Tally * which = &tally0;
    if (which->max > tally1.max) which = &tally1;
    if (which->max > tally2.max) which = &tally2;
    NormTable<T> * final = (NormTable<T> *)calloc(1, NormTable<T>::struct_size + 
                                                  sizeof(T) * which->size * which->max);
    memset(final, 0, NormTable<T>::struct_size + sizeof(T) * which->size * which->max);
    final->mask = which->size - 1;
    final->height = which->size;
    final->width = which->max;
    final->end = final->data + which->size * which->max;
    final->size = size;
    for (cur = d; cur != d + size; ++cur) {
      T * dest = final->data + (cur->from & final->mask);
      while (dest->from != 0) dest += final->height;
      *dest = *cur;
      if (dest->from == 0) dest->from = T::from_non_char;
    }
    for (T * dest = final->data; dest < final->end; dest += final->height) {
      if (dest->from == 0 || (dest->from == T::from_non_char && dest->to[0] == 0)) {
        dest->from = T::from_non_char;
        dest->to[0] = T::to_non_char;
      }
    }
    return final;
  }

  static PosibErr<void> init_norm_tables(FStream & in, NormTables * d) 
  {
    const char FUNC[] = "init_norm_tables";
    String l;
    get_nb_line(in, l);
    remove_comments(l);
    sanity (l == "INTERNAL");
    get_nb_line(in, l);
    remove_comments(l);
    sanity (l == "/");
    CREATE_NORM_TABLE(FromUniNormEntry, in, l, d->internal);
    get_nb_line(in, l);
    remove_comments(l);
    sanity (l == "STRICT");
    char * p = get_nb_line(in, l);
    remove_comments(l);
    if (l == "/") {
      CREATE_NORM_TABLE(FromUniNormEntry, in, l, d->strict_d);
      d->strict = d->strict_d;
    } else {
      sanity(*p == '=');
      ++p; ++p;
      sanity(strcmp(p, "INTERNAL") == 0);
      d->strict = d->internal;
    }
    while (get_nb_line(in, l)) {
      remove_comments(l);
      d->to_uni.push_back(NormTables::ToUniTable());
      NormTables::ToUniTable & e = d->to_uni.back();
      e.name.resize(l.size());
      for (unsigned i = 0; i != l.size(); ++i)
        e.name[i] = asc_tolower(l[i]);
      char * p = get_nb_line(in, l);
      remove_comments(l);
      if (l == "/") {
        CREATE_NORM_TABLE(ToUniNormEntry, in, l, e.data);
        e.ptr = e.data;
      } else {
        sanity(*p == '=');
        ++p; ++p;
        for (char * q = p; *q; ++q) *q = asc_tolower(*q);
        Vector<NormTables::ToUniTable>::iterator i = d->to_uni.begin();
        while (i->name != p && i != d->to_uni.end()) ++i;
        sanity(i != d->to_uni.end());
        e.ptr = i->ptr;
        get_nb_line(in, l);
      }
    }  
    return no_err;
  }

  PosibErr<NormTables *> NormTables::get_new(const String & encoding, 
                                             const Config * config)
  {
    String dir1,dir2,file_name;
    fill_data_dir(config, dir1, dir2);
    find_file(file_name,dir1,dir2,encoding,".cmap");
    
    FStream in;
    PosibErrBase err = in.open(file_name, "r");
    if (err.get_err()) { 
      char mesg[300];
      snprintf(mesg, 300, _("This could also mean that the file \"%s\" could not be opened for reading or does not exist."),
               file_name.c_str());
      return make_err(unknown_encoding, encoding, mesg); // FIXME
    }

    NormTables * d = new NormTables;
    d->key = encoding;
    err = init_norm_tables(in, d);
    if (err.has_err()) {
      return make_err(bad_file_format, file_name, err.get_err()->mesg);
    }

    return d;

  }

  NormTables::~NormTables()
  {
    free_norm_table<FromUniNormEntry>(internal);
    if (strict_d)
      free_norm_table<FromUniNormEntry>(strict_d);
    for (unsigned i = 0; i != to_uni.size(); ++i) {
      if (to_uni[i].data)
        free_norm_table<ToUniNormEntry>(to_uni[i].data);
    }
  }

  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////
  //
  //  Convert
  //
  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////


  bool operator== (const Convert & rhs, const Convert & lhs)
  {
    return strcmp(rhs.in_code(), lhs.in_code()) == 0
      && strcmp(rhs.out_code(), lhs.out_code()) == 0;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Trivial Conversion
  //

  const char * unsupported_null_term_wide_string_msg =
    "Null-terminated wide-character strings unsupported when used this way.";

  template <typename Chr>
  struct DecodeDirect : public Decode 
  {
    DecodeDirect() {type_width = sizeof(Chr);}
    void decode(const char * in0, int size, FilterCharVector & out) const {
      const Chr * in = reinterpret_cast<const Chr *>(in0);
      if (size == -sizeof(Chr)) {
        for (;*in; ++in)
          out.append(*in, sizeof(Chr));
      } else if (size <= -1) {
        fprintf(stderr, "%s\n", unsupported_null_term_wide_string_msg);
        abort();
      } else {
        const Chr * stop = reinterpret_cast<const Chr *>(in0) + size/sizeof(Chr);
        for (;in != stop; ++in)
          out.append(*in, sizeof(Chr));
      }
    }
    PosibErr<void> decode_ec(const char * in0, int size, 
                             FilterCharVector & out, ParmStr) const {
      DecodeDirect::decode(in0, size, out);
      return no_err;
    }
  };

  template <typename Chr>
  struct EncodeDirect : public Encode
  {
    EncodeDirect() {type_width = sizeof(Chr);}
    void encode(const FilterChar * in, const FilterChar * stop, 
                CharVector & out) const {
      for (; in != stop; ++in) {
        Chr c = in->chr;
        if (c != in->chr) c = '?';
        out.append(&c, sizeof(Chr));
      }
    }
    PosibErr<void> encode_ec(const FilterChar * in, const FilterChar * stop, 
                             CharVector & out, ParmStr orig) const {
      for (; in != stop; ++in) {
        Chr c = in->chr;
        if (c != in->chr) {
          char m[70];
          snprintf(m, 70, _("The Unicode code point U+%04X is unsupported."), in->chr);
          return make_err(invalid_string, orig, m);
        }
        
        out.append(&c, sizeof(Chr));
      }
      return no_err;
    }
    bool encode(FilterChar * &, FilterChar * &, FilterCharVector &) const {
      return true;
    }
  };

  template <typename Chr>
  struct ConvDirect : public DirectConv
  {
    ConvDirect() {type_width = sizeof(Chr);}
    void convert(const char * in0, int size, CharVector & out) const {
      if (size == -sizeof(Chr)) {
        const Chr * in = reinterpret_cast<const Chr *>(in0);
        for (;*in != 0; ++in)
          out.append(in, sizeof(Chr));
      } else if (size <= -1) {
        fprintf(stderr, "%s\n", unsupported_null_term_wide_string_msg);
        abort();
      } else {
        out.append(in0, size);
      }
    }
    PosibErr<void> convert_ec(const char * in0, int size, 
                              CharVector & out, ParmStr) const {
      ConvDirect::convert(in0, size, out);
      return no_err;
    }
  };

  //////////////////////////////////////////////////////////////////////
  //
  //  Lookup Conversion
  //

  struct DecodeLookup : public Decode 
  {
    ToUniLookup lookup;
    PosibErr<void> init(ParmStr code, const Config & c) {
      FromUniLookup unused;
      return read_in_char_data(c, code, lookup, unused);
    }
    void decode(const char * in, int size, FilterCharVector & out) const {
      if (size == -1) {
        for (;*in; ++in)
          out.append(lookup[*in]);
      } else {
        const char * stop = in + size;
        for (;in != stop; ++in)
          out.append(lookup[*in]);
      }
    }
    PosibErr<void> decode_ec(const char * in, int size, 
                             FilterCharVector & out, ParmStr) const {
      DecodeLookup::decode(in, size, out);
      return no_err;
    }
  };

  struct DecodeNormLookup : public Decode 
  {
    typedef ToUniNormEntry E;
    NormTable<E> * data;
    DecodeNormLookup(NormTable<E> * d) : data(d) {}
    // must be null terminated
    // FIXME: Why must it be null terminated?
    void decode(const char * in, int size, FilterCharVector & out) const {
      const char * stop = in + size; // will work even if size -1
      while (in != stop) {
        if (*in == 0) {
          if (size == -1) break;
          out.append(0);
          ++in;
        } else {
          NormLookupRet<E,const char> ret = norm_lookup<E>(data, in, stop, 0, in);
          for (unsigned i = 0; ret.to[i] && i < E::max_to; ++i)
            out.append(ret.to[i]);
          in = ret.last + 1;
        }
      }
    }
    PosibErr<void> decode_ec(const char * in, int size, 
                             FilterCharVector & out, ParmStr) const {
      DecodeNormLookup::decode(in, size, out);
      return no_err;
    }
  };

  struct EncodeLookup : public Encode 
  {
    FromUniLookup lookup;
    PosibErr<void> init(ParmStr code, const Config & c) 
      {ToUniLookup unused;
      return read_in_char_data(c, code, unused, lookup);}
    void encode(const FilterChar * in, const FilterChar * stop, 
                CharVector & out) const {
      for (; in != stop; ++in) {
        out.append(lookup(*in));
      }
    }
    PosibErr<void> encode_ec(const FilterChar * in, const FilterChar * stop, 
                             CharVector & out, ParmStr orig) const {
      for (; in != stop; ++in) {
        char c = lookup(*in, '\0');
        if (c == '\0' && in->chr != 0) {
          char m[70];
          snprintf(m, 70, _("The Unicode code point U+%04X is unsupported."), in->chr);
          return make_err(invalid_string, orig, m);
        }
        out.append(c);
      }
      return no_err;
    }
    bool encode(FilterChar * & in0, FilterChar * & stop,
                FilterCharVector & out) const {
      FilterChar * in = in0;
      for (; in != stop; ++in)
        *in = lookup(*in);
      return true;
    }
  };

  struct EncodeNormLookup : public Encode 
  {
    typedef FromUniNormEntry E;
    NormTable<E> * data;
    EncodeNormLookup(NormTable<E> * d) : data(d) {}
    // *stop must equal 0
    void encode(const FilterChar * in, const FilterChar * stop, 
                CharVector & out) const {
      while (in < stop) {
        if (*in == 0) {
          out.append('\0');
          ++in;
        } else {
          NormLookupRet<E,const FilterChar> ret = norm_lookup<E>(data, in, stop, (const byte *)"?", in);
          for (unsigned i = 0; i < E::max_to && ret.to[i]; ++i)
            out.append(ret.to[i]);
          in = ret.last + 1;
        }
      }
    }
    PosibErr<void> encode_ec(const FilterChar * in, const FilterChar * stop, 
                             CharVector & out, ParmStr orig) const {
      while (in < stop) {
        if (*in == 0) {
          out.append('\0');
          ++in;
        } else {
          NormLookupRet<E,const FilterChar> ret = norm_lookup<E>(data, in, stop, 0, in);
          if (ret.to == 0) {
            char m[70];
            snprintf(m, 70, _("The Unicode code point U+%04X is unsupported."), in->chr);
            return make_err(invalid_string, orig, m);
          }
          for (unsigned i = 0; i < E::max_to && ret.to[i]; ++i)
            out.append(ret.to[i]);
          in = ret.last + 1;
        }
      }
      return no_err;
    }
    bool encode(FilterChar * & in, FilterChar * & stop,
                FilterCharVector & buf) const {
      buf.clear();
      while (in < stop) {
        if (*in == 0) {
          buf.append(FilterChar(0));
          ++in;
        } else {
          NormLookupRet<E,FilterChar> ret = norm_lookup<E>(data, in, stop, (const byte *)"?", in);
          const FilterChar * end = ret.last + 1;
          unsigned width = 0;
          for (; in != end; ++in) width += in->width;
          buf.append(FilterChar(ret.to[0], width));
          for (unsigned i = 1; i < E::max_to && ret.to[i]; ++i) {
            buf.append(FilterChar(ret.to[i],0));
          }
        }
      }
      buf.append(0);
      in = buf.pbegin();
      stop = buf.pend();
      return true;
    }
  };

  //////////////////////////////////////////////////////////////////////
  //
  //  UTF8
  //
  
#define get_check_next \
  if (in == stop) goto error;          \
  c = *in;                             \
  if ((c & 0xC0/*1100 0000*/) != 0x80/*10xx xxxx*/) goto error;\
  ++in;                                \
  u <<= 6;                             \
  u |= c & 0x3F/*0011 1111*/;          \
  ++w;

  static inline FilterChar from_utf8 (const char * & in, const char * stop = 0,
                                      Uni32 err_char = '?')
  {
    Uni32 u = (Uni32)(-1);
    FilterChar::Width w = 1;

    // the first char is guaranteed not to be off the end
    char c = *in;
    ++in;

    if ((c & 0x80/*1000 0000*/) == 0x00/*0xxx xxx*/) {
      u = c;
    } else if ((c & 0xE0/*1110 0000*/) == 0xC0/*110x xxxx*/) { // 2-byte wide
      u  = c & 0x1F/*0001 1111*/;
      get_check_next;
    } else if ((c & 0xF0/*1111 0000*/) == 0xE0/*1110 xxxx*/) { // 3-byte wide
      u  = c & 0x0F/*0000 1111*/;
      get_check_next;
      get_check_next;
    } else if ((c & 0xF8/*1111 1000*/) == 0xF0/*1111 0xxx*/) { // 4-byte wide
      u  = c & 0x07/*0000 0111*/;
      get_check_next;
      get_check_next;
      get_check_next;
    } else {
      goto error;
    }

    return FilterChar(u, w);
  error:
    return FilterChar(err_char, w);
  }

  static inline void to_utf8 (FilterChar in, CharVector & out)
  {
    FilterChar::Chr c = in;
    
    if (c < 0x80) {
      out.append(c);
    }
    else if (c < 0x800) {
      out.append(0xC0 | (c>>6));
      out.append(0x80 | (c & 0x3F));
    }
    else if (c < 0x10000) {
      out.append(0xE0 | (c>>12));
      out.append(0x80 | (c>>6 & 0x3F));
      out.append(0x80 | (c & 0x3F));
    }
    else if (c < 0x200000) {
      out.append(0xF0 | (c>>18));
      out.append(0x80 | (c>>12 & 0x3F));
      out.append(0x80 | (c>>6 & 0x3F));
      out.append(0x80 | (c & 0x3F));
    }
  }
  
  struct DecodeUtf8 : public Decode 
  {
    ToUniLookup lookup;
    void decode(const char * in, int size, FilterCharVector & out) const {
      if (size == -1) {
        while (*in)
          out.append(from_utf8(in));
      } else {
        const char * stop = in + size;
        while (in != stop)
          out.append(from_utf8(in, stop));
      }
    }
    PosibErr<void> decode_ec(const char * in, int size, 
                             FilterCharVector & out, ParmStr orig) const {
      const char * begin = in;
      if (size == -1) {
        while (*in) {
          FilterChar c = from_utf8(in, 0, (Uni32)-1);
          if (c == (Uni32)-1) goto error;
          out.append(c);
        }
      } else {
        const char * stop = in + size;
        while (in != stop) {
          FilterChar c = from_utf8(in, stop, (Uni32)-1);
          if (c == (Uni32)-1) goto error;
          out.append(c);
        }
      }
      return no_err;
    error:
      char m[70];
      snprintf(m, 70, _("Invalid UTF-8 sequence at position %ld."), (long)(in - begin));
      return make_err(invalid_string, orig, m);
    }
  };

  struct EncodeUtf8 : public Encode 
  {
    FromUniLookup lookup;
    void encode(const FilterChar * in, const FilterChar * stop, 
                CharVector & out) const {
      for (; in != stop; ++in) {
        to_utf8(*in, out);
      }
    }
    PosibErr<void> encode_ec(const FilterChar * in, const FilterChar * stop, 
                             CharVector & out, ParmStr) const {
      for (; in != stop; ++in) {
        to_utf8(*in, out);
      }
      return no_err;
    }
  };

  //////////////////////////////////////////////////////////////////////
  //
  // Cache
  //

  static GlobalCache<Decode> decode_cache("decode");
  static GlobalCache<Encode> encode_cache("encode");
  static GlobalCache<NormTables> norm_tables_cache("norm_tables");
  
  //////////////////////////////////////////////////////////////////////
  //
  // new_aspell_convert
  //

  void Convert::generic_convert(const char * in, int size, CharVector & out)
  {
    buf_.clear();
    decode_->decode(in, size, buf_);
    FilterChar * start = buf_.pbegin();
    FilterChar * stop = buf_.pend();
    if (!filter.empty())
      filter.process(start, stop);
    encode_->encode(start, stop, out);
  }

  const char * fix_encoding_str(ParmStr enc, String & buf)
  {
    buf.clear();
    buf.reserve(enc.size() + 1);
    for (size_t i = 0; i != enc.size(); ++i)
      buf.push_back(asc_tolower(enc[i]));

    if (strncmp(buf.c_str(), "iso8859", 7) == 0)
      buf.insert(buf.begin() + 3, '-'); // For backwards compatibility
    
    if (buf == "ascii" || buf == "ansi_x3.4-1968")
      return "iso-8859-1";
    else if (buf == "machine unsigned 16" || buf == "utf-16")
      return "ucs-2";
    else if (buf == "machine unsigned 32" || buf == "utf-32")
      return "ucs-4";
    else
      return buf.c_str();
  }

  bool ascii_encoding(const Config & c, ParmStr enc0)
  {
    if (enc0.empty()) return true;
    if (enc0 == "ANSI_X3.4-1968" 
        || enc0 == "ASCII" || enc0 == "ascii") return true;
    String buf;
    const char * enc = fix_encoding_str(enc0, buf);
    if (strcmp(enc, "utf-8") == 0 
        || strcmp(enc, "ucs-2") == 0 
        || strcmp(enc, "ucs-4") == 0) return false;
    String dir1,dir2,file_name;
    fill_data_dir(&c, dir1, dir2);
    file_name << dir1 << enc << ".cset";
    if (file_exists(file_name)) return false;
    if (dir1 == dir2) return true;
    file_name.clear();
    file_name << dir2 << enc << ".cset";
    return !file_exists(file_name);
  }

  PosibErr<Convert *> internal_new_convert(const Config & c,
                                           ConvKey in, 
                                           ConvKey out,
                                           bool if_needed,
                                           Normalize norm)
  {
    String in_s;
    in.val = fix_encoding_str(in.val, in_s);

    String out_s;
    out.val = fix_encoding_str(out.val, out_s); 

    if (if_needed && in.val == out.val) return 0;

    StackPtr<Convert> conv(new Convert);
    switch (norm) {
    case NormNone:
      RET_ON_ERR(conv->init(c, in, out)); break;
    case NormFrom:
      RET_ON_ERR(conv->init_norm_from(c, in, out)); break;
    case NormTo:
      RET_ON_ERR(conv->init_norm_to(c, in, out)); break;
    }
    return conv.release();
  }

  PosibErr<Decode *> Decode::get_new(const ConvKey & k, const Config * c)
  {
    StackPtr<Decode> ptr;
    if (k.val == "iso-8859-1") {
      ptr.reset(new DecodeDirect<Uni8>);
    } else if (k.val == "ucs-2") {
      if (k.allow_ucs)
        ptr.reset(new DecodeDirect<Uni16>);
      else
        return make_err(encoding_not_supported, k.val);
    } else if (k.val == "ucs-4") {
      if (k.allow_ucs)
        ptr.reset(new DecodeDirect<Uni32>);
      else
        return make_err(encoding_not_supported, k.val);
    } else if (k.val == "utf-8") {
      ptr.reset(new DecodeUtf8);
    } else {
      ptr.reset(new DecodeLookup);
    }
    RET_ON_ERR(ptr->init(k.val, *c));
    ptr->key = k.val;
    return ptr.release();
  }

  PosibErr<Encode *> Encode::get_new(const ConvKey & k, const Config * c)
  {
    StackPtr<Encode> ptr;
    if (k.val == "iso-8859-1") {
      ptr.reset(new EncodeDirect<Uni8>);
    } else if (k.val == "ucs-2" && k.allow_ucs) {
      if (k.allow_ucs)
        ptr.reset(new EncodeDirect<Uni16>);
      else
        return make_err(encoding_not_supported, k.val);
    } else if (k.val == "ucs-4" && k.allow_ucs) {
      if (k.allow_ucs)
        ptr.reset(new EncodeDirect<Uni32>);
      else
        return make_err(encoding_not_supported, k.val);
    } else if (k.val == "utf-8") {
      ptr.reset(new EncodeUtf8);
    } else {
      ptr.reset(new EncodeLookup);
    }
    RET_ON_ERR(ptr->init(k.val, *c));
    ptr->key = k.val;
    return ptr.release();
  }

  Convert::~Convert() {}

  PosibErr<void> Convert::init(const Config & c, const ConvKey & in, const ConvKey & out)
  {
    RET_ON_ERR(setup(decode_c, &decode_cache, &c, in));
    decode_ = decode_c.get();
    RET_ON_ERR(setup(encode_c, &encode_cache, &c, out));
    encode_ = encode_c.get();

    conv_ = 0;
    if (in.val == out.val) {
      if (in.val == "ucs-2") {
        if (in.allow_ucs) {
          conv_ = new ConvDirect<Uni16>;
        } else {
          return make_err(encoding_not_supported, in.val);
        }
      } else if (in.val == "ucs-4") {
        if (in.allow_ucs) {
          conv_ = new ConvDirect<Uni32>;
        } else {
          return make_err(encoding_not_supported, in.val);
        }
      } else {
        conv_ = new ConvDirect<char>;
      }
    }

    if (conv_)
      RET_ON_ERR(conv_->init(decode_, encode_, c));

    return no_err;
  }

  
  PosibErr<void> Convert::init_norm_from(const Config & c, const ConvKey & in, const ConvKey & out)
  {
    if (!c.retrieve_bool("normalize") && !c.retrieve_bool("norm-required")) 
      return init(c,in,out);

    RET_ON_ERR(setup(norm_tables_, &norm_tables_cache, &c, out.val));

    RET_ON_ERR(setup(decode_c, &decode_cache, &c, in));
    decode_ = decode_c.get();

    if (c.retrieve_bool("norm-strict")) {
      encode_s = new EncodeNormLookup(norm_tables_->strict);
      encode_ = encode_s;
      encode_->key = out.val;
      encode_->key += ":strict";
    } else {
      encode_s = new EncodeNormLookup(norm_tables_->internal);
      encode_ = encode_s;
      encode_->key = out.val;
      encode_->key += ":internal";
    }
    conv_ = 0;

    return no_err;
  }

  PosibErr<void> Convert::init_norm_to(const Config & c, const ConvKey & in, const ConvKey & out)
  {
    String norm_form = c.retrieve("norm-form");
    if ((!c.retrieve_bool("normalize") || norm_form == "none")
        && !c.retrieve_bool("norm-required"))
      return init(c,in,out);
    if (norm_form == "none" && c.retrieve_bool("norm-required"))
      norm_form = "nfc";

    RET_ON_ERR(setup(norm_tables_, &norm_tables_cache, &c, in.val));

    RET_ON_ERR(setup(encode_c, &encode_cache, &c, out));
    encode_ = encode_c.get();

    NormTables::ToUni::const_iterator i = norm_tables_->to_uni.begin();
    for (; i != norm_tables_->to_uni.end() && i->name != norm_form; ++i);
    if (i == norm_tables_->to_uni.end())
      return make_err(aerror_bad_value, "norm-form", norm_form, "one of none, nfd, nfc, or comp");

    decode_s = new DecodeNormLookup(i->ptr);
    decode_ = decode_s;
    decode_->key = in.val;
    decode_->key += ':';
    decode_->key += i->name;

    conv_ = 0;

    return no_err;
  }

  PosibErr<void> MBLen::setup(const Config &, ParmStr enc0)
  {
    String buf;
    const char * enc = fix_encoding_str(enc0,buf);
    if      (strcmp(enc, "utf-8") == 0) encoding = UTF8;
    else if (strcmp(enc, "ucs-2") == 0) encoding = UCS2;
    else if (strcmp(enc, "ucs-4") == 0) encoding = UCS4;
    else                                encoding = Other;
    return no_err;
  }

  unsigned MBLen::operator()(const char * str, const char * stop)
  {
    unsigned size = 0;
    switch (encoding) {
    case Other: 
      return stop - str;
    case UTF8:
      for (; str != stop; ++str) {
        if ((*str & 0x80) == 0 || (*str & 0xC0) == 0xC0) ++size;
      }
      return size;
    case UCS2:
      return (stop - str)/2;
    case UCS4:
      return (stop - str)/4;
    }
    return 0;
  }

  PosibErr<void> unsupported_null_term_wide_string_err_(const char * func) {
    static bool reported_to_stderr = false;
    PosibErr<void> err = make_err(other_error, unsupported_null_term_wide_string_msg);
    if (!reported_to_stderr) {
      CERR.printf("ERROR: %s: %s\n", func, unsupported_null_term_wide_string_msg);
      reported_to_stderr = true;
    }
    return err;
  }

  void unsupported_null_term_wide_string_abort_(const char * func) {
    CERR.printf("%s: %s\n", func, unsupported_null_term_wide_string_msg);
    abort();
  }
 
}
