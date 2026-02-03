// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson and Christoph Hintermüller
// under the GNU LGPL license version 2.0 or 2.1.  You should have
// received a copy of the LGPL license along with this library if you
// did not you can find it at http://www.gnu.org/.
//
// Expansion for loading filter libraries and collections upon startup
// was added by Christoph Hintermüller

#include "settings.h"

#include "cache-t.hpp"
#include "asc_ctype.hpp"
#include "config.hpp"
#include "enumeration.hpp"
#include "errors.hpp"
#include "filter.hpp"
#include "filter_entry.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "indiv_filter.hpp"
#include "iostream.hpp"
#include "itemize.hpp"
#include "key_info.hpp"
#include "parm_string.hpp"
#include "posib_err.hpp"
#include "stack_ptr.hpp"
#include "string_enumeration.hpp"
#include "string_list.hpp"
#include "string_map.hpp"
#include "strtonum.hpp"
#include "file_util.hpp"

#include <stdio.h>

#ifdef HAVE_LIBDL
#  include <dlfcn.h>
#endif

namespace acommon
{
#include "static_filters.src.cpp"

  //////////////////////////////////////////////////////////////////////////
  //
  // setup static filters
  //

  PosibErr<const ConfigModule *> get_dynamic_filter(Config * config, ParmStr value);
  extern void activate_filter_modes(Config *config);

  void setup_static_filters(Config * config)
  {
    config->set_filter_modules(filter_modules_begin, filter_modules_end);
    activate_filter_modes(config);
#ifdef HAVE_LIBDL
    config->load_filter_hook = get_dynamic_filter;
#endif
  }

  //////////////////////////////////////////////////////////////////////////
  //
  // 
  //

#ifdef HAVE_LIBDL

  struct ConfigFilterModule : public Cacheable {
    String name; 
    String file; // path of shared object or dll
    String desc; // description of module
    Vector<KeyInfo> options;
    typedef Config CacheConfig;
    typedef String CacheKey;
    static PosibErr<ConfigFilterModule *> get_new(const String & key, const Config *);
    bool cache_key_eq(const String & okey) const {
      return name == okey;
    }
    ConfigFilterModule() : in_option(0) {}
    ~ConfigFilterModule();
    bool in_option;
    KeyInfo * new_option() {
      options.push_back(KeyInfo()); 
      in_option = true; 
      return &options.back();}
    PosibErr<void> end_option();
  };

  static GlobalCache<ConfigFilterModule> filter_module_cache("filters");

  ConfigFilterModule::~ConfigFilterModule()
  {
    for (Vector<KeyInfo>::iterator i = options.begin();
         i != options.end();
         ++i)
    {
      free(const_cast<char *>(i->name));
      free(const_cast<char *>(i->def));
      free(const_cast<char *>(i->desc));
    }
  }

#endif

  class IndividualFilter;

  //
  // actual code
  //

  FilterEntry * get_standard_filter(ParmStr);

  //////////////////////////////////////////////////////////////////////////
  //
  // setup filter
  //

  PosibErr<void> setup_filter(Filter & filter, 
			      Config * config, 
			      bool use_decoder, bool use_filter, bool use_encoder)
  {
    StringList sl;
    config->retrieve_list("filter", &sl);
    StringListEnumeration els = sl.elements_obj();
    const char * filter_name;
    String fun;

    StackPtr<IndividualFilter> ifilter;

    filter.clear();

    while ((filter_name = els.next()) != 0) {
      //fprintf(stderr, "Loading %s ... \n", filter_name);
      FilterEntry * f = get_standard_filter(filter_name);
      // In case libdl is not available a filter is only available if made
      // one of the standard filters. This is done by statically linking
      // the filter sources.
      // On systems providing libdl or in case libtool mimics libdl 
      // The following code parts assure that all filters needed and requested
      // by user are loaded properly or be reported to be missing.
      // 
      FilterHandle decoder_handle, filter_handle, encoder_handle;
#ifdef HAVE_LIBDL
      FilterEntry dynamic_filter;
      if (!f) {

        RET_ON_ERR_SET(get_dynamic_filter(config, filter_name),
                       const ConfigModule *, module);

        if (!(decoder_handle = dlopen(module->file,RTLD_NOW)) ||
            !(encoder_handle = dlopen(module->file,RTLD_NOW)) ||
            !(filter_handle  = dlopen(module->file,RTLD_NOW)))
          return make_err(cant_dlopen_file,dlerror()).with_file(filter_name);

        fun = "new_aspell_";
        fun += filter_name;
        fun += "_decoder";
        dynamic_filter.decoder = (FilterFun *)dlsym(decoder_handle.get(), fun.str());

        fun = "new_aspell_";
        fun += filter_name;
        fun += "_encoder";
        dynamic_filter.encoder = (FilterFun *)dlsym(encoder_handle.get(), fun.str());

        fun = "new_aspell_";
        fun += filter_name;
        fun += "_filter";
        dynamic_filter.filter = (FilterFun *)dlsym(filter_handle.get(), fun.str());

        if (!dynamic_filter.decoder && 
	    !dynamic_filter.encoder &&
	    !dynamic_filter.filter)
          return make_err(empty_filter,filter_name);
        dynamic_filter.name = filter_name;
        f = &dynamic_filter;
      } 
#else
      if (!f)
        return make_err(no_such_filter, filter_name);
#endif
      if (use_decoder && f->decoder && (ifilter = f->decoder())) {
        RET_ON_ERR_SET(ifilter->setup(config), bool, keep);
        ifilter->handle = decoder_handle.release();
	if (!keep) {
	  ifilter.del();
	} else {
          filter.add_filter(ifilter.release());
        }
      } 
      if (use_filter && f->filter && (ifilter = f->filter())) {
        RET_ON_ERR_SET(ifilter->setup(config), bool, keep);
        ifilter->handle = filter_handle.release();
        if (!keep) {
          ifilter.del();
        } else {
          filter.add_filter(ifilter.release());
        }
      }
      if (use_encoder && f->encoder && (ifilter = f->encoder())) {
        RET_ON_ERR_SET(ifilter->setup(config), bool, keep);
        ifilter->handle = encoder_handle.release();
        if (!keep) {
          ifilter.del();
        } else {
          filter.add_filter(ifilter.release());
        }
      }
    }
    return no_err;
  }

  //////////////////////////////////////////////////////////////////////////
  //
  // get filter
  //

  FilterEntry * get_standard_filter(ParmStr filter_name) {
    unsigned int i = 0;
    while (i != standard_filters_size) {
      if (standard_filters[i].name == filter_name) {
	return (FilterEntry *) standard_filters + i;
      }
      ++i;
    }
    return 0;
  }

#ifdef HAVE_LIBDL

  PosibErr<const ConfigModule *> get_dynamic_filter(Config * config, ParmStr filter_name) 
  {
    for (const ConfigModule * cur = config->filter_modules.pbegin();
         cur != config->filter_modules.pend();
         ++cur)
    {
      if (strcmp(cur->name,filter_name) == 0) return cur;
    }

    RET_ON_ERR_SET(get_cache_data(&filter_module_cache, config, filter_name), 
                   ConfigFilterModule *, module);

    ConfigModule m = {
      module->name.str(), module->file.str(), module->desc.str(),
      module->options.pbegin(), module->options.pend()
    };

    config->filter_modules_ptrs.push_back(module);
    config->filter_modules.push_back(m);

    return &config->filter_modules.back();
  }

  PosibErr<ConfigFilterModule *> ConfigFilterModule::get_new(const String & filter_name, 
                                                             const Config * config)
  {
    StackPtr<ConfigFilterModule> module(new ConfigFilterModule);
    module->name = filter_name;
    KeyInfo * cur_opt = NULL;

    String option_file = filter_name;
    option_file += "-filter.info";
    if (!find_file(config, "filter-path", option_file))
      return make_err(no_such_filter, filter_name);

    FStream options;
    RET_ON_ERR(options.open(option_file,"r"));

    String buf; DataPair d;
    while (getdata_pair(options,d,buf))
    {
      to_lower(d.key);

      //
      // key == aspell
      //
      if (d.key == "aspell") 
      {
        if ( d.value == NULL || *d.value == '\0' )
          return make_err(confusing_version).with_file(option_file,d.line_num);
#ifdef FILTER_VERSION_CONTROL 
       PosibErr<void> peb = check_version(d.value.str);
        if (peb.has_err()) return peb.with_file(option_file,d.line_num);
#endif
        continue;
      }
	  
      //
      // key == option
      //
      if (d.key == "option" ) {

        RET_ON_ERR(module->end_option());

        to_lower(d.value.str);

        cur_opt = module->new_option();
        
        char * s = (char *)malloc(2 + filter_name.size() + 1 + d.value.size + 1);
        cur_opt->name = s;
        memcpy(s, "f-", 2); 
        s+= 2;
        memcpy(s, filter_name.str(), filter_name.size());
        s += filter_name.size();
        *s++ = '-';
        memcpy(s, d.value.str, d.value.size);
        s += d.value.size;
        *s = '\0';
        for (Vector<KeyInfo>::iterator cur = module->options.begin();
             cur != module->options.end() - 1; // avoid checking the one just inserted
             ++cur) {
          if (strcmp(cur_opt->name,cur->name) == 0)
            return make_err(identical_option).with_file(option_file,d.line_num);
        }

        continue;
      }

      //
      // key == static
      //
      if (d.key == "static") {
        RET_ON_ERR(module->end_option());
        continue;
      }

      //
      // key == description
      //
      if ((d.key == "desc") ||
          (d.key == "description")) {

        unescape(d.value);

        //
        // filter description
        // 
        if (!module->in_option) {
          module->desc = d.value;
        } 
        //
        //option description
        //
        else {
          //avoid memory leak;
          if (cur_opt->desc) free((char *)cur_opt->desc);
          cur_opt->desc = strdup(d.value.str);
        }
        continue;
      }

      //
      // key == lib-file
      //
      if (d.key == "lib-file")
      {
        module->file = d.value;
        continue;
      }
	  
      //
      // !active_option
      //
      if (!module->in_option) {
        return make_err(options_only).with_file(option_file,d.line_num);
      }

      //
      // key == type
      //
      if (d.key == "type") {
        to_lower(d.value); // This is safe since normally option_value is used
        if (d.value == "list") 
          cur_opt->type = KeyInfoList;
        else if (d.value == "int" || d.value == "integer") 
          cur_opt->type = KeyInfoInt;
        else if (d.value == "string")
          cur_opt->type = KeyInfoString;
        //FIXME why not force user to omit type specifier or explicitly say bool ???
        else
          cur_opt->type = KeyInfoBool;
        continue;
      }

      //
      // key == default
      //
      if (d.key == "def" || d.key == "default") {
            
        if (cur_opt->type == KeyInfoList) {

          int new_len = 0;
          int orig_len = 0;
          if (cur_opt->def) {
            orig_len = strlen(cur_opt->def);
            new_len += orig_len + 1;
          }
          for (const char * s = d.value.str; *s; ++s) {
            if (*s == ':') ++new_len;
            ++new_len;
          }
          new_len += 1;
          char * x = (char *)realloc((char *)cur_opt->def, new_len);
          cur_opt->def = x;
          if (orig_len > 0) {
            x += orig_len;
            *x++ = ':';
          }
          for (const char * s = d.value.str; *s; ++s) {
            if (*s == ':') *x++ = ':';
            *x++ = *s;
          }
          *x = '\0';

        } else {

          // FIXME
          //may try some syntax checking
          //if ( cur_opt->type == KeyInfoBool ) {
          //  check for valid bool values true false 0 1 on off ...
          //  and issue error if wrong or assume false ??
          //}
          //if ( cur_opt->type == KeyInfoInt ) {
          //  check for valid integer or double and issue error if not
          //}
          unescape(d.value);
          cur_opt->def = strdup(d.value.str);
        }
        continue;
      }
          
      //
      // key == flags
      //
      if (d.key == "flags") {
        if (d.value == "utf-8" || d.value == "UTF-8")
          cur_opt->flags = KEYINFO_UTF8;
        continue;
      }
           
      //
      // key == endoption
      //
      if (d.key=="endoption") {
        RET_ON_ERR(module->end_option());
        continue;
      }

      // 
      // error
      // 
      return make_err(invalid_option_modifier).with_file(option_file,d.line_num);
        
    } // end while getdata_pair_c
    RET_ON_ERR(module->end_option());
    const char * slash = strrchr(option_file.str(), '/');
    assert(slash);
    if (module->file.empty()) {
      module->file.assign(option_file.str(), slash + 1 - option_file.str());
      //module->file += "lib";
      module->file += filter_name;
      module->file += "-filter.so";
    } else {
      if (module->file[0] != '/')
        module->file.insert(0, option_file.str(), slash + 1 - option_file.str());
      module->file += ".so";
    }

    return module.release();
  }

  PosibErr<void>  ConfigFilterModule::end_option()
  {
    if (in_option) 
    {
      // FIXME: Check to make sure there is a name and desc.
      KeyInfo * cur_opt = &options.back();
      if (!cur_opt->def) cur_opt->def = strdup("");
    }
    in_option = false;
    return no_err;
  }

#endif

  void load_all_filters(Config * config) {
#ifdef HAVE_LIBDL
    StringList filter_path;
    String toload;
    
    config->retrieve_list("filter-path", &filter_path);
    PathBrowser els(filter_path, "-filter.info");
    
    const char * file;
    while ((file = els.next()) != NULL) {
      
      const char * name = strrchr(file, '/');
      if (!name) name = file;
      else name++;
      unsigned len = strlen(name) - 12;
      
      toload.assign(name, len);
      
      get_dynamic_filter(config, toload);
    }
#endif
  }


  class FiltersEnumeration : public StringPairEnumeration
  {
  public:
    typedef Vector<ConfigModule>::const_iterator Itr;
  private:
    Itr it;
    Itr end;
  public:
    FiltersEnumeration(Itr i, Itr e) : it(i), end(e) {}
    bool at_end() const {return it == end;}
    StringPair next()
    {
      if (it == end) return StringPair();
      StringPair res = StringPair(it->name, it->desc);
      ++it;
      return res;
    }
    StringPairEnumeration * clone() const {return new FiltersEnumeration(*this);}
    void assign(const StringPairEnumeration * other0)
    {
      const FiltersEnumeration * other = (const FiltersEnumeration *)other0;
      *this = *other;
    }
  };

  PosibErr<StringPairEnumeration *> available_filters(Config * config)
  {
    return new FiltersEnumeration(config->filter_modules.begin(),
                                  config->filter_modules.end());
  }

}
