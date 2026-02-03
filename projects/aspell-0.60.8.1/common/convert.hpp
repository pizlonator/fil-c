// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_CONVERT__HPP
#define ASPELL_CONVERT__HPP

#include "settings.h"

#include "string.hpp"
#include "posib_err.hpp"
#include "char_vector.hpp"
#include "filter_char.hpp"
#include "filter_char_vector.hpp"
#include "stack_ptr.hpp"
#include "filter.hpp"
#include "cache.hpp"

namespace acommon {

  class OStream;
  class Config;

  struct ConvKey {
    ParmString val;
    bool allow_ucs;
    ConvKey() : val(), allow_ucs() {}
    template <typename T>
    ConvKey(const T & v, bool a = false) : val(v), allow_ucs(a) {}
  };

  struct ConvBase : public Cacheable {
    typedef const Config CacheConfig;
    typedef ConvKey CacheKey;
    String key;
    int type_width; // type width in bytes
    bool is_ucs() const {return type_width != 1;}
    bool cache_key_eq(CacheKey k) const  {return k.allow_ucs ? key == k.val : key == k.val && !is_ucs();}
    ConvBase() : type_width(1) {}
  private:
    ConvBase(const ConvBase &);
    void operator=(const ConvBase &);
  };

  struct Decode : public ConvBase {
    virtual PosibErr<void> init(ParmStr code, const Config &) {return no_err;}
    virtual void decode(const char * in, int size,
			FilterCharVector & out) const = 0;
    virtual PosibErr<void> decode_ec(const char * in, int size,
                                     FilterCharVector & out, ParmStr orig) const = 0;
    static PosibErr<Decode *> get_new(const ConvKey &, const Config *);
    virtual ~Decode() {}
  };
  struct Encode : public ConvBase {
    // null characters should be treated like any other character
    // by the encoder.
    virtual PosibErr<void> init(ParmStr, const Config &) {return no_err;}
    virtual void encode(const FilterChar * in, const FilterChar * stop, 
                        CharVector & out) const = 0;
    virtual PosibErr<void> encode_ec(const FilterChar * in, const FilterChar * stop, 
                                     CharVector & out, ParmStr orig) const = 0;
    // may convert inplace
    virtual bool encode(FilterChar * & in, FilterChar * & stop, 
                        FilterCharVector & buf) const {return false;}
    static PosibErr<Encode *> get_new(const ConvKey &, const Config *);
    virtual ~Encode() {}
  };
  struct DirectConv { // convert directly from in_code to out_code.
    int type_width; // type width in bytes
    DirectConv() : type_width(1) {}
    // should not take ownership of decode and encode.
    // decode and encode guaranteed to stick around for the life
    // of the object.
    virtual PosibErr<void> init(const Decode *, const Encode *, 
				const Config &) {return no_err;}
    virtual void convert(const char * in, int size, 
			 CharVector & out) const = 0;
    virtual PosibErr<void> convert_ec(const char * in, int size, 
                                      CharVector & out, ParmStr orig) const = 0;
    virtual ~DirectConv() {}
  };
  template <class T> struct NormTable;
  struct FromUniNormEntry;
  struct ToUniNormEntry;
  struct NormTables : public Cacheable {
    typedef const Config CacheConfig;
    typedef const char * CacheKey;
    String key;
    bool cache_key_eq(const char * l) const  {return key == l;}
    static PosibErr<NormTables *> get_new(const String &, const Config *);
    NormTable<FromUniNormEntry> * internal;
    NormTable<FromUniNormEntry> * strict_d;
    NormTable<FromUniNormEntry> * strict;
    struct ToUniTable {
      String name;
      NormTable<ToUniNormEntry> * data;
      NormTable<ToUniNormEntry> * ptr;
      ToUniTable() : data(), ptr() {}
    };
    typedef Vector<ToUniTable> ToUni;
    Vector<ToUniTable> to_uni;
    ~NormTables();
  };

  typedef FilterCharVector ConvertBuffer;

  class Convert {
  private:
    CachePtr<Decode> decode_c;
    StackPtr<Decode> decode_s;
    Decode * decode_;
    CachePtr<Encode> encode_c;
    StackPtr<Encode> encode_s;
    Encode * encode_;
    CachePtr<NormTables> norm_tables_;
    StackPtr<DirectConv> conv_;

    ConvertBuffer buf_;

    static const unsigned int null_len_ = 4; // POSIB FIXME: Be more precise

    Convert(const Convert &);
    void operator=(const Convert &);

  public:
    Convert() {}
    ~Convert();

    // This filter is used when the convert method is called.  It must
    // be set up by an external entity as this class does not set up
    // this class in any way.
    Filter filter;

    PosibErr<void> init(const Config &, const ConvKey & in, const ConvKey & out);
    PosibErr<void> init_norm_to(const Config &, const ConvKey & in, const ConvKey & out);
    PosibErr<void> init_norm_from(const Config &, const ConvKey & in, const ConvKey & out);
    
    const char * in_code() const   {return decode_->key.c_str();}
    const char * out_code() const  {return encode_->key.c_str();}

    int in_type_width() const {return decode_->type_width;}
    int out_type_width() const {return encode_->type_width;}

    void append_null(CharVector & out) const
    {
      const char nul[4] = {0,0,0,0}; // 4 should be enough
      out.write(nul, null_len_);
    }

    unsigned int null_len() const {return null_len_;}
  
    // this filters will generally not translate null characters
    // if you need a null character at the end, add it yourself
    // with append_null

    void decode(const char * in, int size, FilterCharVector & out) const 
      {decode_->decode(in,size,out);}
    
    void encode(const FilterChar * in, const FilterChar * stop, 
		CharVector & out) const
      {encode_->encode(in,stop,out);}

    bool encode(FilterChar * & in, FilterChar * & stop, 
                FilterCharVector & buf) const
      {return encode_->encode(in,stop,buf);}

    // does NOT pass it through filters
    // DOES NOT use an internal state
    void convert(const char * in, int size, CharVector & out, ConvertBuffer & buf) const
    {
      if (conv_) {
	conv_->convert(in,size,out);
      } else {
        buf.clear();
        decode_->decode(in, size, buf);
        encode_->encode(buf.pbegin(), buf.pend(), out);
      }
    }

    // does NOT pass it through filters
    // DOES NOT use an internal state
    PosibErr<void> convert_ec(const char * in, int size, CharVector & out, 
                              ConvertBuffer & buf, ParmStr orig) const
    {
      if (conv_) {
	RET_ON_ERR(conv_->convert_ec(in,size,out, orig));
      } else {
        buf.clear();
        RET_ON_ERR(decode_->decode_ec(in, size, buf, orig));
       RET_ON_ERR(encode_->encode_ec(buf.pbegin(), buf.pend(), 
                                      out, orig));
      }
      return no_err;
    }


    // convert has the potential to use internal buffers and
    // is therefore not const.  It is also not thread safe
    // and I have no intention to make it thus.

    void convert(const char * in, int size, CharVector & out) {
      if (filter.empty()) {
        convert(in,size,out,buf_);
      } else {
        generic_convert(in,size,out);
      }
    }

    void convert(const void * in, int size, CharVector & out) {
      convert(static_cast<const char *>(in), size, out);
    }

    void generic_convert(const char * in, int size, CharVector & out);
    
  };

  bool operator== (const Convert & rhs, const Convert & lhs);

  const char * fix_encoding_str(ParmStr enc, String & buf);

  // also returns true if the encoding is unknown
  bool ascii_encoding(const Config & c, ParmStr enc0);

  enum Normalize {NormNone, NormFrom, NormTo};

  PosibErr<Convert *> internal_new_convert(const Config & c, 
                                           ConvKey in, ConvKey out,
                                           bool if_needed,
                                           Normalize n);
  
  static inline PosibErr<Convert *> new_convert(const Config & c,
                                                const ConvKey & in, const ConvKey & out,
                                                Normalize n)
  {
    return internal_new_convert(c,in,out,false,n);
  }
  
  static inline PosibErr<Convert *> new_convert_if_needed(const Config & c,
                                                          const ConvKey & in, const ConvKey & out,
                                                          Normalize n)
  {
    return internal_new_convert(c,in,out,true,n);
  }

  struct ConvObj {
    Convert * ptr;
    ConvObj(Convert * c = 0) : ptr(c) {}
    ~ConvObj() {delete ptr;}
    PosibErr<void> setup(const Config & c, const ConvKey & from, const ConvKey & to, Normalize norm)
    {
      delete ptr;
      ptr = 0;
      PosibErr<Convert *> pe = new_convert_if_needed(c, from, to, norm);
      if (pe.has_err()) return pe;
      ptr = pe.data;
      return no_err;
    }
    operator const Convert * () const {return ptr;}
  private:
    ConvObj(const ConvObj &);
    void operator=(const ConvObj &);
  };

  struct ConvP {
    const Convert * conv;
    ConvertBuffer buf0;
    CharVector buf;
    operator bool() const {return conv;}
    ConvP(const Convert * c = 0) : conv(c) {}
    ConvP(const ConvObj & c) : conv(c.ptr) {}
    ConvP(const ConvP & c) : conv(c.conv) {}
    void operator=(const ConvP & c) { conv = c.conv; }
    PosibErr<void> setup(const Config & c, const ConvKey & from, const ConvKey & to, 
                         Normalize norm)
    {
      delete conv;
      conv = 0;
      PosibErr<Convert *> pe = new_convert_if_needed(c, from, to, norm);
      if (pe.has_err()) return pe;
      conv = pe.data;
      return no_err;
    }
    char * operator() (char * str, size_t sz)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, sz, buf, buf0);
        return buf.mstr();
      } else {
        return str;
      }
    }
    const char * operator() (const char * str, size_t sz)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, sz, buf, buf0);
        return buf.str();
      } else {
        return str;
      }
    }
    char * operator() (MutableString str)
    {
      return operator()(str.str, str.size);
    }
    char * operator() (char * str)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, -1, buf, buf0);
        return buf.mstr();
      } else {
        return str;
      }
    }
    const char * operator() (ParmStr str)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, -1, buf, buf0);
        return buf.mstr();
      } else {
        return str;
      }
    }
    char * operator() (char c)
    {
      buf.clear();
      if (conv) {
        char str[2] = {c, 0};
        conv->convert(str, 1, buf, buf0);
      } else {
        buf.append(c);
      }
      return buf.mstr();
    }
  };

  struct Conv : public ConvP
  {
    ConvObj conv_obj;
    Conv(Convert * c = 0) : ConvP(c), conv_obj(c) {}
    PosibErr<void> setup(const Config & c, const ConvKey & from, const ConvKey & to, Normalize norm)
    {
      RET_ON_ERR(conv_obj.setup(c,from,to,norm));
      conv = conv_obj.ptr;
      return no_err;
    }
  };

  struct ConvECP {
    const Convert * conv;
    ConvertBuffer buf0;
    CharVector buf;
    operator bool() const {return conv;}
    ConvECP(const Convert * c = 0) : conv(c) {}
    ConvECP(const ConvObj & c) : conv(c.ptr) {}
    ConvECP(const ConvECP & c) : conv(c.conv) {}
    void operator=(const ConvECP & c) { conv = c.conv; }
    PosibErr<void> setup(const Config & c, const ConvKey & from, const ConvKey & to, Normalize norm)
    {
      delete conv;
      conv = 0;
      PosibErr<Convert *> pe = new_convert_if_needed(c, from, to, norm);
      if (pe.has_err()) return pe;
      conv = pe.data;
      return no_err;
    }
    PosibErr<char *> operator() (char * str, size_t sz)
    {
      if (conv) {
        buf.clear();
        RET_ON_ERR(conv->convert_ec(str, sz, buf, buf0, str));
        return buf.mstr();
      } else {
        return str;
      }
    }
    PosibErr<char *> operator() (MutableString str)
    {
      return operator()(str.str, str.size);
    }
    PosibErr<char *> operator() (char * str)
    {
      if (conv) {
        buf.clear();
        RET_ON_ERR(conv->convert_ec(str, -1, buf, buf0, str));
        return buf.mstr();
      } else {
        return str;
      }
    }

    PosibErr<const char *> operator() (ParmStr str)
    {
      if (conv) {
        buf.clear();
        RET_ON_ERR(conv->convert_ec(str, -1, buf, buf0, str));
        return buf.mstr();
      } else {
        return str.str();
      }
    }
    PosibErr<const char *> operator() (char c)
    {
      char buf2[2] = {c, 0};
      return operator()(ParmString(buf2,1));
    }
  };

  struct ConvEC : public ConvECP
  {
    ConvObj conv_obj;
    ConvEC(Convert * c = 0) : ConvECP(c), conv_obj(c) {}
    PosibErr<void> setup(const Config & c, const ConvKey & from, const ConvKey & to, Normalize norm)
    {
      RET_ON_ERR(conv_obj.setup(c,from,to,norm));
      conv = conv_obj.ptr;
      return no_err;
    }
  };

  struct MBLen 
  {
    enum Encoding {Other, UTF8, UCS2, UCS4} encoding;
    MBLen() : encoding(Other) {}
    PosibErr<void> setup(const Config &, ParmStr enc);
    unsigned operator()(const char * str, const char * stop);
    unsigned operator()(const char * str, unsigned byte_size) {
      return operator()(str, str + byte_size);}
  };

#ifdef SLOPPY_NULL_TERM_STRINGS
  static const bool sloppy_null_term_strings = true;
#else
  static const bool sloppy_null_term_strings = false;
#endif
  
  PosibErr<void> unsupported_null_term_wide_string_err_(const char * func);
  void unsupported_null_term_wide_string_abort_(const char * func);
    
  static inline PosibErr<int> get_correct_size(const char * func, int conv_type_width, int size) {
    if (sloppy_null_term_strings && size <= -1)
      return -conv_type_width;
    if (size <= -1 && -conv_type_width != size)
      return unsupported_null_term_wide_string_err_(func);
    return size;
  }
  static inline int get_correct_size(const char * func, int conv_type_width, int size, int type_width) {
    if ((sloppy_null_term_strings || type_width <= -1) && size <= -1)
      return -conv_type_width;
    if (size <= -1 && conv_type_width != type_width)
      unsupported_null_term_wide_string_abort_(func);
    return size;
  }

}

#endif
