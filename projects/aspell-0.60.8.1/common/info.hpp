// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_INFO__HPP
#define ASPELL_INFO__HPP

#include "posib_err.hpp"
#include "type_id.hpp"

namespace acommon {

  typedef int c_boolean;

  class Config;
  struct DictInfo;
  class DictInfoEnumeration;
  class DictInfoList;
  struct ModuleInfo;
  class ModuleInfoEnumeration;
  class ModuleInfoList;
  class StringList;
  struct StringListImpl;
  class FStream;
  class StringMap;

  struct ModuleInfo {
    const char * name;
    double order_num;
    const char * lib_dir;
    StringList * dict_exts;
    StringList * dict_dirs;
  };

  PosibErr<void> get_dict_file_name(const DictInfo *, 
				    String & main_wl, String & flags);
  
  struct DictInfo {
    const char * name;
    const char * code;
    const char * variety;
    int size;
    const char * size_str;
    const ModuleInfo * module;
  };

  struct MDInfoListAll;
  
  struct ModuleInfoNode;
  
  class ModuleInfoList {
  public:
    ModuleInfoList() : size_(0), head_(0) {}
    void clear();
    PosibErr<void> fill(MDInfoListAll &, Config *);
    bool empty() const;
    unsigned int size() const;
    ModuleInfoEnumeration * elements() const;
    PosibErr<void> proc_info(MDInfoListAll &,
			     Config *,
			     const char * name,
			     unsigned int name_size,
			     IStream &);
    ModuleInfoNode * find(const char * to_find, 
			  unsigned int to_find_len);
  public: // but don't use
    unsigned int size_;
    ModuleInfoNode * head_;
  };
  
  PosibErr<const ModuleInfoList *> get_module_info_list(Config *);

  struct DictInfoNode;

  class DictInfoList {
  public:
    DictInfoList() : size_(0), head_(0) {}
    void clear();
    PosibErr<void> fill(MDInfoListAll &, Config *);
    bool empty() const;
    unsigned int size() const;
    DictInfoEnumeration * elements() const;
    PosibErr<void> proc_file(MDInfoListAll &,
			     Config *,
			     const char * dir,
			     const char * name,
			     unsigned int name_size,
			     const ModuleInfo *);
  public: // but don't use
    unsigned int size_;
    DictInfoNode * head_;
  };

  PosibErr<const DictInfoList *> get_dict_info_list(Config *);

  PosibErr<const StringMap *> get_dict_aliases(Config *);

  class ModuleInfoEnumeration {
  public:
    typedef const ModuleInfo * Value;

    const ModuleInfoNode * node_;
    ModuleInfoEnumeration(const ModuleInfoNode * n) : node_(n) {}

    bool at_end() const;
    const ModuleInfo * next();
    int ref_count_;
    TypeId type_id_;
    unsigned int type_id() { return type_id_.num; }
    int copyable_;
    int copyable() { return copyable_; }
    ModuleInfoEnumeration * clone() const;
    void assign(const ModuleInfoEnumeration * other);
    ModuleInfoEnumeration() : ref_count_(0), copyable_(2) {}
    virtual ~ModuleInfoEnumeration() {}
  };

  struct DictInfoNode;

  class DictInfoEnumeration {
  public:
    const DictInfoNode * node_;
    DictInfoEnumeration(const DictInfoNode * n) : node_(n) {}

    typedef const DictInfo * Value;

    bool at_end() const;
    const DictInfo * next();
    int ref_count_;
    TypeId type_id_;
    unsigned int type_id() { return type_id_.num; }
    int copyable_;
    int copyable() { return copyable_; }
    DictInfoEnumeration * clone() const;
    void assign(const DictInfoEnumeration * other);
    DictInfoEnumeration() : ref_count_(0), copyable_(2) {}
    virtual ~DictInfoEnumeration() {}
  };




}

#endif /* ASPELL_INFO__HPP */
