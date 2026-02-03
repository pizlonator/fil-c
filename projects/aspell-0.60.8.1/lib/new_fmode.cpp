// This file is part of The New Aspell
// Copyright (C) 2004 by Christoph Hintermüller (JEH) under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include "settings.h"

#ifdef USE_POSIX_REGEX
#  include <sys/types.h>
#  include <regex.h>
#endif

#include "stack_ptr.hpp"
#include "cache-t.hpp"
#include "string.hpp"
#include "vector.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "filter.hpp"
#include "string_enumeration.hpp"
#include "string_list.hpp"
#include "posib_err.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "strtonum.hpp"
#include "asc_ctype.hpp"
#include "iostream.hpp"

#include "gettext.h"

namespace acommon {

  class FilterMode {
  public:
    class MagicString {
    public:
      MagicString(const String & mode) : mode_(mode), fileExtensions() {}
      MagicString(const String & magic, const String & mode)
        : magic_(magic), mode_(mode) {} 
      bool matchFile(FILE * in, const String & ext);
      static PosibErr<bool> testMagic(FILE * seekIn, String & magic, const String & mode);
      void addExtension(const String & ext) { fileExtensions.push_back(ext); }
      bool hasExtension(const String & ext);
      void remExtension(const String & ext);
      MagicString & operator += (const String & ext) {addExtension(ext);return *this;}
      MagicString & operator -= (const String & ext) {remExtension(ext);return *this;}
      MagicString & operator = (const String & ext) { 
        fileExtensions.clear();
        addExtension(ext);
        return *this; 
      }
      const String & magic() const { return magic_; }
      const String & magicMode() const { return mode_; }
      ~MagicString() {}
    private:
      String magic_;
      String mode_;
      Vector<String> fileExtensions;
    };

    FilterMode(const String & name);
    PosibErr<bool> addModeExtension(const String & ext, String toMagic);
    PosibErr<bool> remModeExtension(const String & ext, String toMagic);
    bool lockFileToMode(const String & fileName,FILE * in = NULL);
    const String & modeName() const;
    void setDescription(const String & desc) {desc_ = desc;}
    const String & getDescription() const {return desc_;}
    PosibErr<void> expand(Config * config);
    PosibErr<void> build(FStream &, int line = 1, 
                         const char * fname = "mode file");

    ~FilterMode();
  private:
    //map extensions to magic keys 
    String name_;
    String desc_;
    String file_;
    Vector<MagicString> magicKeys;
    struct KeyValue {
      String key;
      String value;
      KeyValue() {}
      KeyValue(ParmStr k, ParmStr v) : key(k), value(v) {}
    };
    Vector<KeyValue> expansion;
  };

  class FilterModeList : public Cacheable, public Vector<FilterMode>
  {
  public:
    typedef Config CacheConfig;
    typedef String CacheKey;
    String key;
    static PosibErr<FilterModeList *> get_new(const String & key, const Config *);
    bool cache_key_eq(const String & okey) const {
      return key == okey;
    }
  };

  class ModeNotifierImpl : public Notifier
  {
  private:
    ModeNotifierImpl();
    ModeNotifierImpl(const ModeNotifierImpl &);
    ModeNotifierImpl & operator= (const ModeNotifierImpl & b);
    CachePtr<FilterModeList> filter_modes_;
  public:
    Config * config;
    PosibErr<FilterModeList *> get_filter_modes();
    
    ModeNotifierImpl(Config * c) : config(c) 
    {
      c->filter_mode_notifier = this;
    }
    ModeNotifierImpl(const ModeNotifierImpl & other,  Config * c) 
      : filter_modes_(other.filter_modes_), config(c) 
    {
      c->filter_mode_notifier = this;
    }
    
    ModeNotifierImpl * clone(Config * c) const {return new ModeNotifierImpl(*this, c);}

    PosibErr<void> item_updated(const KeyInfo * ki, ParmStr);
    PosibErr<void> list_updated(const KeyInfo * ki);

    ~ModeNotifierImpl() {}
  };

  FilterMode::FilterMode(const String & name)
  : name_(name) {}

  PosibErr<bool> FilterMode::addModeExtension(const String & ext, String toMagic) {

    bool extOnly = false;
    
    if (    ( toMagic == "" )
         || ( toMagic == "<noregex>" )
         || ( toMagic == "<nomagic>" )
         || ( toMagic == "<empty>" ) ) {
      extOnly = true;
    }
    else {

      RET_ON_ERR(FilterMode::MagicString::testMagic(NULL,toMagic,name_));

    } 

    Vector<MagicString>::iterator it;

    for ( it = magicKeys.begin() ; it != magicKeys.end() ; it++ ) {
      if (    (    extOnly
                && ( it->magic() == "" ) )
           || ( it->magic() == toMagic ) ) {
        *it += ext;
        return true;
      }
    }
    if ( it != magicKeys.end() ) {
      return false;
    }
    if ( extOnly ) {
      magicKeys.push_back(MagicString(name_));
    }
    else {
      magicKeys.push_back(MagicString(toMagic,name_));
    }
    for ( it = magicKeys.begin() ; it != magicKeys.end() ; it++ ) {
      if (    (    extOnly
                && ( it->magic() == "" ) )
           || ( it->magic() == toMagic ) ) {
        *it += ext;
        return true;
      }
    }
    return make_err(mode_extend_expand,name_.str());
  }

  PosibErr<bool> FilterMode::remModeExtension(const String & ext, String toMagic) {

    bool extOnly = false;

    if (    ( toMagic == "" )
         || ( toMagic == "<nomagic>" )
         || ( toMagic == "<empty>" ) ) {
      extOnly = true;
    }
    else {

      PosibErr<bool> pe = FilterMode::MagicString::testMagic(NULL,toMagic,name_);

      if ( pe.has_err() ) {
        return PosibErrBase(pe);
      }
    }

    for ( Vector<MagicString>::iterator it = magicKeys.begin() ;
          it != magicKeys.end() ; it++ ) {
      if (    (    extOnly
                && ( it->magic() == "" ) )
           || ( it->magic() == toMagic ) ) {
        *it -= ext;
        return true;
      }
    }
    return false;
  }

  bool FilterMode::lockFileToMode(const String & fileName,FILE * in) {

    Vector<unsigned int> extStart;
    int first_point = fileName.size();

    while ( first_point > 0 ) {
      while (    ( --first_point >= 0 )
              && ( fileName[first_point] != '.' ) ) {
      }
      if (    ( first_point >= 0 )
           && ( fileName[first_point] == '.' ) ) {
        extStart.push_back(first_point + 1);
      }
    }
    if ( extStart.size() < 1 )  {
      return false;
    }

    bool closeFile = false;

    if ( in == NULL ) {
      in = fopen(fileName.str(),"rb");
      closeFile= true;
    }
    for ( Vector<unsigned int>::iterator extSIt = extStart.begin() ;
          extSIt != extStart.end() ; extSIt ++ ) {
    
      String ext(fileName);

      ext.erase(0,*extSIt);
      for ( Vector<MagicString>::iterator it = magicKeys.begin() ;
            it != magicKeys.end() ; it++ ) {
        PosibErr<bool> magicMatch = it->matchFile(in,ext);
        if (    magicMatch 
             || magicMatch.has_err() ) {
          if ( closeFile ) {
            fclose ( in );
          }
          if ( magicMatch.has_err() ) {
            magicMatch.ignore_err();
            return false;
          }
          return true;
        }
      }
    }
    if ( closeFile ) {
      fclose(in);
    }
    return false;
  }

  const String & FilterMode::modeName() const {
    return name_;
  }

  FilterMode::~FilterMode() {
  }

  bool FilterMode::MagicString::hasExtension(const String & ext) {
    for ( Vector<String>::iterator it = fileExtensions.begin() ;
          it != fileExtensions.end() ; it++ ) {
      if ( *it == ext ) {
        return true;
      }
    }
    return false;
  }

  void FilterMode::MagicString::remExtension(const String & ext) {
    Vector<String>::iterator it = fileExtensions.begin();
    while (it != fileExtensions.end()) {
      if (*it == ext) {
        it = fileExtensions.erase(it);
      } else {
        it++;
      }
    }
  }


  bool FilterMode::MagicString::matchFile(FILE * in,const String & ext) {

    Vector<String>::iterator extIt;

    for ( extIt = fileExtensions.begin() ; 
          extIt != fileExtensions.end() ; extIt ++ ) {
      if ( *extIt == ext ) {
        break;
      }
    }
    if ( extIt == fileExtensions.end() ) {
      return false;
    }

    PosibErr<bool> pe = testMagic(in,magic_,mode_);

    if ( pe.has_err() ) {
      pe.ignore_err();
      return false;
    }
    return true;
  }


  PosibErr<bool> FilterMode::MagicString::testMagic(FILE * seekIn,String & magic, const String & mode) {

#ifdef USE_POSIX_REGEX

    if ( magic.size() == 0 ) {
      return true;
    }
 
    unsigned int magicFilePosition = 0;

    while (    ( magicFilePosition < magic.size() )
            && ( magic[magicFilePosition] != ':' ) ) {
      magicFilePosition++;
    }

    String number(magic);

    number.erase(magicFilePosition,magic.size() - magicFilePosition);

    const char * num = number.str();
    const char * numEnd = num + number.size();
    const char * endHere = numEnd;
    long position = 0;

    if (    ( number.size() == 0 ) 
         || ( (position = strtoi_c(num,&numEnd)) < 0 )
         || ( numEnd != endHere ) ) {
      return make_err(file_magic_pos,"",magic.str());
    }
    if (    ( magicFilePosition >= magic.size() )
         || (    ( seekIn != NULL )
              && ( fseek(seekIn,position,SEEK_SET) < 0 ) ) ) {
      if ( seekIn != NULL ) {
        rewind(seekIn);
      }
      return false;
    }

    //increment magicFilePosition to skip the `:'
    unsigned int seekRangePos = ++ magicFilePosition; 

    while (    ( magicFilePosition < magic.size() )
            && ( magic[magicFilePosition] != ':' ) ) {
      magicFilePosition++;
    }

    String magicRegExp(magic);

    magicRegExp.erase(0,magicFilePosition + 1);
    if ( magicRegExp.size() == 0 ) {
      if ( seekIn != NULL ) {
        rewind(seekIn);
      }
      return make_err(missing_magic,mode.str(),magic.str()); //no regular expression given
    }
    
    number = magic;
    number.erase(magicFilePosition,magic.size() - magicFilePosition);
    number.erase(0,seekRangePos);//already incremented by one see above
    num = (char*)number.str();
    endHere = numEnd = num + number.size();

    if (    ( number.size() == 0 )
         || ( (position = strtoi_c(num,&numEnd)) < 0 )
         || ( numEnd != endHere ) ) {
      if ( seekIn != NULL ) {
        rewind(seekIn);
      }
      return make_err(file_magic_range,mode.str(),magic.str());//no magic range given
    }

    regex_t seekMagic;
    int regsucess = 0;

    if ( (regsucess = regcomp(&seekMagic,magicRegExp.str(),
                              REG_NEWLINE|REG_NOSUB|REG_EXTENDED)) ){
      if ( seekIn != NULL ) {
        rewind(seekIn);
      }

      char regError[256];
      regerror(regsucess,&seekMagic,&regError[0],256);
      return make_err(bad_magic,mode.str(),magic.str(),regError);
    }
    if ( seekIn == NULL ) {
      regfree(&seekMagic);
      return true;
    }

    char * buffer = new char[(position + 1)];

    if ( buffer == NULL ) {
      regfree(&seekMagic);
      rewind(seekIn);
      return false;
    }
    memset(buffer,0,(position + 1));
    if ( (position = fread(buffer,1,position,seekIn)) == 0 ) {
      rewind(seekIn);
      regfree(&seekMagic);
      delete[] buffer;
      return false;
    }
    if ( regexec(&seekMagic,buffer,0,NULL,0) ) {
      delete[] buffer;
      regfree(&seekMagic);
      rewind(seekIn);
      return false;
    }
    delete[] buffer;
    regfree(&seekMagic);
    rewind(seekIn);
    return true;

#else

    return true;

#endif

  }

  PosibErr<void> FilterMode::expand(Config * config) {

    config->replace("clear-filter","");
    for ( Vector<KeyValue>::iterator it = expansion.begin() ;
          it != expansion.end() ; it++ ) 
    {
      PosibErr<void> pe = config->replace(it->key, it->value);
      if (pe.has_err()) return pe.with_file(file_);
    }
    return no_err;  
  }

  PosibErr<void> FilterMode::build(FStream & toParse, int line0, const char * fname) {

    String buf;
    DataPair dp;

    file_ = fname;

    dp.line_num = line0;

    while ( getdata_pair(toParse, dp, buf) ) {

      to_lower(dp.key);

      if ( dp.key == "filter" ) {

        to_lower(dp.value);
        expansion.push_back(KeyValue("add-filter", dp.value));

      } else if ( dp.key == "option" ) {

        split(dp);
        // FIXME: Add check for empty key

        expansion.push_back(KeyValue(dp.key, dp.value));

      } else {
        
        return make_err(bad_mode_key,dp.key).with_file(fname,dp.line_num);
      }
    }

    return no_err;
  }

  static GlobalCache<FilterModeList> filter_modes_cache("filter_modes");

  PosibErr<void> set_mode_from_extension (Config * config, ParmString filename, FILE * in) 
  {
    RET_ON_ERR_SET(static_cast<ModeNotifierImpl *>(config->filter_mode_notifier)
                   ->get_filter_modes(), FilterModeList *, fm);

    for ( FilterModeList::iterator it = fm->begin(); it != fm->end(); it++ ) 
    {
      if ( it->lockFileToMode(filename,in) ) {
        RET_ON_ERR(config->replace("mode", it->modeName().str()));
        break;
      }
    }
    return no_err;
  }

  void activate_filter_modes(Config *config);

  PosibErr<FilterModeList *>  ModeNotifierImpl::get_filter_modes()
  {
    if (!filter_modes_) {
      //FIXME is filter-path proper for filter mode files ???
      //      if filter-options-path better ???
      //      do we need a filter-mode-path ???
      //      should change to use genetic data-path once implemented
      //        and then search filter-path - KevinA
      String filter_path;
      StringList filter_path_lst;
      config->retrieve_list("filter-path", &filter_path_lst);
      combine_list(filter_path, filter_path_lst);
      RET_ON_ERR(setup(filter_modes_, &filter_modes_cache, config, filter_path));
    }
    return filter_modes_.get();
  }


  PosibErr<void> ModeNotifierImpl::item_updated(const KeyInfo * ki, ParmStr value)
  {
    if ( strcmp(ki->name, "mode") == 0 ) {
      RET_ON_ERR_SET(get_filter_modes(), FilterModeList *, filter_modes);
      for ( Vector<FilterMode>::iterator it = filter_modes->begin() ;
            it != filter_modes->end() ; it++ ) {
        if ( it->modeName() == value )
          return it->expand(config);
      }
      return make_err(unknown_mode, value); 
    }
    return no_err;
  }

  PosibErr<void> ModeNotifierImpl::list_updated(const KeyInfo * ki)
  {
    if (strcmp(ki->name, "filter-path") == 0) {
      filter_modes_.reset(0);
    }
    return no_err;
  }

  PosibErr<FilterModeList *> FilterModeList::get_new(const String & key,
                                                     const Config *) 
  {

    StackPtr<FilterModeList> filter_modes(new FilterModeList);
    filter_modes->key = key;
    StringList mode_path;
    separate_list(key, mode_path);
    
    PathBrowser els(mode_path, ".amf");

    String possMode;
    String possModeFile;

    const char * file;
    while ((file = els.next()) != NULL) 
    {
      possModeFile = file;
      possMode.assign(possModeFile.str(), possModeFile.size() - 4);

      unsigned pathPos = 0;
      unsigned pathPosEnd = 0;

      while (    ( (pathPosEnd = possMode.find('/',pathPos)) < possMode.size() )
              && ( pathPosEnd >= 0 ) ) {
        pathPos = pathPosEnd + 1;
      }
      possMode.erase(0,pathPos);
      to_lower(possMode.mstr());

      Vector<FilterMode>::iterator fmIt = filter_modes->begin();

      for ( fmIt = filter_modes->begin() ; 
            fmIt != filter_modes->end() ; fmIt++ ) {
        if ( (*fmIt).modeName() == possMode ) {
          break;
        }
      }
      if ( fmIt != filter_modes->end() ) {
        continue;
      }

      FStream toParse;

      RET_ON_ERR(toParse.open(possModeFile.str(),"rb"));

      String buf;
      DataPair dp;

      bool get_sucess = getdata_pair(toParse, dp, buf);
      
      to_lower(dp.key);
      to_lower(dp.value);
      if (    !get_sucess
           || ( dp.key != "mode" ) 
           || ( dp.value != possMode.str() ) )
        return make_err(expect_mode_key,"mode").with_file(possModeFile, dp.line_num);

      get_sucess = getdata_pair(toParse, dp, buf);
      to_lower(dp.key);
      if (    !get_sucess
           || ( dp.key != "aspell" )
           || ( dp.value == NULL )
           || ( *(dp.value) == '\0' ) )
        return make_err(mode_version_requirement).with_file(possModeFile, dp.line_num);

#ifdef FILTER_VERSION_CONTROL
      PosibErr<void> peb = check_version(dp.value.str);
      if (peb.has_err()) return peb.with_file(possModeFile, dp.line_num);
#endif
      
      FilterMode collect(possMode);
      
      while ( getdata_pair(toParse,dp,buf) ) {
        to_lower(dp.key);
        if (   ( dp.key == "desc" ) 
            || ( dp.key == "description" ) ) 
        {
          unescape(dp.value);
          collect.setDescription(dp.value);
          break;
        }
        if ( dp.key == "magic" ) {

          char * regbegin = dp.value;

          while (    regbegin
                  && ( *regbegin != '/' ) ) {
            regbegin++;
          }
          if (    ( regbegin == NULL )
               || ( *regbegin == '\0' ) 
               || ( *(++regbegin) == '\0' ) )
            return make_err(missing_magic_expression).with_file(possModeFile, dp.line_num);
          
          char * regend = regbegin;
          bool prevslash = false;

          while (    regend
                  && ( *regend != '\0' )
                  && (    prevslash
                       || ( * regend != '/' ) ) )  {
            if ( *regend == '\\' ) {
              prevslash = !prevslash;
            }
            else {
              prevslash = false;
            }
            regend ++ ;
          }
          if ( regend == regbegin )
            return make_err(missing_magic_expression).with_file(possModeFile, dp.line_num);

          char swap = *regend;

          *regend = '\0';
          
          String magic(regbegin);
          
          *regend = swap;

          unsigned int extCount = 0;

          while ( *regend != '\0' ) {
            regend ++;
            extCount ++;
            regbegin = regend;
            while (    ( *regend != '/' ) 
                    && ( *regend != '\0' ) ) {
              regend++;
            }
            if ( regend == regbegin ) 
            {
              char charCount[64];
              sprintf(&charCount[0],"%li",(long)(regbegin - (char *)dp.value));
              return  make_err(empty_file_ext,charCount).with_file(possModeFile,dp.line_num);
            }

            bool remove = false;
            bool add = true;

            if ( *regbegin == '+' ) {
              regbegin++;
            }
            else if ( *regbegin == '-' ) {
              add = false;
              remove = true;
              regbegin++;
            }
            if ( regend == regbegin ) 
            {
              char charCount[64];
              sprintf(&charCount[0],"%li",(long)(regbegin - (char *)dp.value));
              return  make_err(empty_file_ext,charCount).with_file(possModeFile,dp.line_num);
            }
            swap = *regend;
            *regend = '\0';
            
            String ext(regbegin);

            *regend = swap;

            // partially unescape magic
            
            char * dest = magic.mstr();
            const char * src  = magic.mstr();
            while (*src) {
              if ((*src == '\\' && src[1] == '/') || src[1] == '#')
                ++src;
              *dest++ = *src++;
            }
            magic.resize(dest - magic.mstr());

            PosibErr<bool> pe;

            if ( remove )
              pe = collect.remModeExtension(ext,magic);
            else
              pe = collect.addModeExtension(ext,magic);

            if ( pe.has_err() )
              return pe.with_file(possModeFile, dp.line_num);
          }

          if (extCount > 0 ) continue;

          char charCount[64];
          sprintf(&charCount[0],"%lu",(unsigned long)strlen((char *)dp.value));
          return  make_err(empty_file_ext,charCount).with_file(possModeFile,dp.line_num);
        }

        return make_err(expect_mode_key,"ext[tension]/magic/desc[ription]/rel[ation]")
          .with_file(possModeFile,dp.line_num);
      
      }//while getdata_pair
      
      RET_ON_ERR(collect.build(toParse,dp.line_num,possModeFile.str()));

      filter_modes->push_back(collect);
    }
    return filter_modes.release();
  }

  void activate_filter_modes(Config *config) 
  {
    config->add_notifier(new ModeNotifierImpl(config));
  }

  class FilterModesEnumeration : public StringPairEnumeration
  {
  public:
    typedef Vector<FilterMode>::const_iterator Itr;
  private:
    Itr it;
    Itr end;
  public:
    FilterModesEnumeration(Itr i, Itr e) : it(i), end(e) {}
    bool at_end() const {return it == end;}
    StringPair next()
    {
      if (it == end) return StringPair();
      StringPair res = StringPair(it->modeName().str(), it->getDescription().str());
      ++it;
      return res;
    }
    StringPairEnumeration * clone() const {return new FilterModesEnumeration(*this);}
    void assign(const StringPairEnumeration * other0)
    {
      const FilterModesEnumeration * other = (const FilterModesEnumeration *)other0;
      *this = *other;
    }
  };

  PosibErr<StringPairEnumeration *> available_filter_modes(Config * config)
  {
    RET_ON_ERR_SET(static_cast<ModeNotifierImpl *>(config->filter_mode_notifier)
                   ->get_filter_modes(), FilterModeList *, fm);
    return new FilterModesEnumeration(fm->begin(), fm->end());
  }

}

