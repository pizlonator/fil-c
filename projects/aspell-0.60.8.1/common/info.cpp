// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

// POSIX includes
#ifdef __bsdi__
 /* BSDi defines u_intXX_t types in machine/types.h */
#include <machine/types.h>
#endif
#ifdef WIN32
#  include <windows.h>
#  include <winbase.h>
#endif

#include "iostream.hpp"

#include "asc_ctype.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "info.hpp"
#include "itemize.hpp"
#include "string.hpp"
#include "string_list.hpp"
#include "vector.hpp"
#include "stack_ptr.hpp"
#include "strtonum.hpp"
#include "lock.hpp"
#include "string_map.hpp"

#include "gettext.h"

namespace acommon {

  class Dir {
    DIR * d_;
    Dir(const Dir &);
    Dir & operator=(const Dir &);
  public:
    operator DIR * () {return d_;}
    Dir(DIR * d) : d_(d) {}
    ~Dir() {if (d_) closedir(d_);}
  };

  /////////////////////////////////////////////////////////////////
  //
  // Lists of Info Lists
  //

  static void get_data_dirs (Config *,
			     StringList &);

  struct DictExt
  {
    static const size_t max_ext_size = 15;
    const ModuleInfo * module;
    size_t ext_size;
    char ext[max_ext_size + 1];
    DictExt(ModuleInfo * m, const char * e);
  };

  typedef Vector<DictExt> DictExtList;

  struct MDInfoListAll
  // this is in an invalid state if some of the lists
  // has data but others don't
  {
    StringList key;
    StringList for_dirs;
    ModuleInfoList module_info_list;
    StringList dict_dirs;
    DictExtList    dict_exts;
    DictInfoList   dict_info_list;
    StringMap      dict_aliases;
    void clear();
    PosibErr<void> fill(Config *, StringList &);
    bool has_data() const {return module_info_list.head_ != 0;}
    void fill_helper_lists(const StringList &);
    PosibErr<void> fill_dict_aliases(Config *);
  };

  class MDInfoListofLists
  {
    Mutex lock;

    MDInfoListAll * data;
  
    int       offset;
    int       size;
  
    int valid_pos(int pos) {return offset <= pos && pos < size + offset;}

    void clear(Config * c);
    int find(const StringList &);

  public:

    MDInfoListofLists();
    ~MDInfoListofLists();

    PosibErr<const MDInfoListAll *> get_lists(Config * c);

    void flush() {} // unimplemented
  };

  static MDInfoListofLists md_info_list_of_lists;

  /////////////////////////////////////////////////////////////////
  //
  // Utility functions declaration
  //

  static const char * strnchr(const char * i, char c, unsigned int size);
  static const char * strnrchr(const char * stop, char c, unsigned int size);

  /////////////////////////////////////////////////////////////////
  //
  // Built in modules
  //

  struct ModuleInfoDefItem {
    const char * name;
    const char * data;
  };

  static const ModuleInfoDefItem module_info_list_def_list[] = {
    {"default", 
     "order-num 0.50;" 
     "dict-exts .multi,.alias"}
  };
  
  /////////////////////////////////////////////////////////////////
  //
  // ModuleInfoList Impl
  //

  struct ModuleInfoNode
  {
    ModuleInfo c_struct;
    ModuleInfoNode * next;
    ModuleInfoNode(ModuleInfoNode * n = 0) : next(n) {}
    String name;
    String lib_dir;
    StringList dict_exts;
    StringList dict_dirs;
  };

  void ModuleInfoList::clear() 
  {
    while (head_ != 0) {
      ModuleInfoNode * to_del = head_;
      head_ = head_->next;
      delete to_del;
    }
  }

  PosibErr<void> ModuleInfoList::fill(MDInfoListAll & list_all,
				      Config * config)
  {
    const ModuleInfoDefItem * i   = module_info_list_def_list;
    const ModuleInfoDefItem * end = module_info_list_def_list
      + sizeof(module_info_list_def_list)/sizeof(ModuleInfoDefItem);
    for (; i != end; ++i)
    {
      StringIStream in(i->data);
      proc_info(list_all, config, i->name, strlen(i->name), in);
    }

    StringListEnumeration els = list_all.for_dirs.elements_obj();
    const char * dir;
    while ( (dir = els.next()) != 0) {
      Dir d(opendir(dir));
      if (d==0) continue;
    
      struct dirent * entry;
      while ( (entry = readdir(d)) != 0) {
	const char * name = entry->d_name;
	const char * dot_loc = strrchr(name, '.');
	unsigned int name_size = dot_loc == 0 ? strlen(name) :  dot_loc - name;
      
	// check if it ends in suffix
	if (strcmp(name + name_size, ".asmi") != 0)
	  continue;
      
	String path;
	path += dir;
	path += '/';
	path += name;
	FStream in;
	RET_ON_ERR(in.open(path, "r"));
	RET_ON_ERR(proc_info(list_all, config, name, name_size, in));
      }
    }
    return no_err;
  }

  PosibErr<void> ModuleInfoList::proc_info(MDInfoListAll &,
					   Config * config,
					   const char * name,
					   unsigned int name_size,
					   IStream & in)
  {
    ModuleInfoNode * * prev = &head_;
    ModuleInfoNode * to_add = new ModuleInfoNode();
    to_add->c_struct.name = 0;
    to_add->c_struct.order_num = -1;
    to_add->c_struct.lib_dir = 0;
    to_add->c_struct.dict_dirs = 0;

    to_add->name.assign(name, name_size);
    to_add->c_struct.name = to_add->name.c_str();
    
    PosibErr<void> err;

    String buf; DataPair d;
    while (getdata_pair(in, d, buf)) {
      if (d.key == "order-num") {
	to_add->c_struct.order_num = strtod_c(d.value.str, NULL);
	if (!(0 < to_add->c_struct.order_num && 
	      to_add->c_struct.order_num < 1)) 
	  {
	    err.prim_err(bad_value, d.key, d.value,
			 _("a number between 0 and 1"));
	    goto RETURN_ERROR;
	  }
      } else if (d.key == "lib-dir") {
	to_add->lib_dir = d.value.str;
	to_add->c_struct.lib_dir = to_add->lib_dir.c_str();
      } else if (d.key == "dict-dir" || d.key == "dict-dirs") {
	to_add->c_struct.dict_dirs = &(to_add->dict_dirs);
	itemize(d.value, to_add->dict_dirs);
      } else if (d.key == "dict-exts") {
	to_add->c_struct.dict_dirs = &(to_add->dict_exts);
	itemize(d.value, to_add->dict_exts);
      } else {
	err.prim_err(unknown_key, d.key);
	goto RETURN_ERROR;
      }
    }
  
    while (*prev != 0 && 
	   (*prev)->c_struct.order_num < to_add->c_struct.order_num)
      prev = &(*prev)->next;
    to_add->next = *prev;
    *prev = to_add;
    return err;

  RETURN_ERROR:
    delete to_add;
    return err;
  }

  ModuleInfoNode * ModuleInfoList::find(const char * to_find, 
					unsigned int to_find_len)
  {
    for (ModuleInfoNode * n = head_; 
	 n != 0; 
	 n = n->next)
    {
      if (n->name.size() == to_find_len 
	  && strncmp(n->name.c_str(), to_find, to_find_len) == 0) return n;
    }
    return 0;
  }

  /////////////////////////////////////////////////////////////////
  //
  // DictInfoList Impl
  //

  struct DictInfoNode
  {
    DictInfo c_struct;
    DictInfoNode * next;
    DictInfoNode(DictInfoNode * n = 0) : next(n) {}
    String name;
    String code;
    String variety;
    String size_str;
    String info_file;
    bool direct;
  };

  bool operator< (const DictInfoNode & r, const DictInfoNode & l);

  void DictInfoList::clear() 
  {
    while (head_ != 0) {
      DictInfoNode * to_del = head_;
      head_ = head_->next;
      delete to_del;
    }
  }

  const DictExt * find_dict_ext(const DictExtList & l, ParmStr name)
  {
    DictExtList::const_iterator   i = l.begin();
    DictExtList::const_iterator end = l.end();
    for (; i != end; ++i) 
    {
      if (i->ext_size <= name.size() 
          && strncmp(name + (name.size() - i->ext_size),
                     i->ext, i->ext_size) == 0)
        break;
    }
    
    if (i == end) // does not end in one of the extensions in list
      return 0;
    else
      return &*i;
  }


  PosibErr<void> DictInfoList::fill(MDInfoListAll & list_all,
				    Config * config)
  {
    StringList aliases;
    config->retrieve_list("dict-alias", &aliases);
    StringListEnumeration els = aliases.elements_obj();
    const char * str;
    while ( (str = els.next()) != 0) {
      const char * end = strchr(str, ' ');
      assert(end != 0); // FIXME: Return error
      String name(str, end - str);
      RET_ON_ERR(proc_file(list_all, config,
                           0, name.str(), name.size(), 
                           find_dict_ext(list_all.dict_exts, ".alias")->module));
    }

    els = list_all.dict_dirs.elements_obj();
    const char * dir;
    while ( (dir = els.next()) != 0) {
      Dir d(opendir(dir));
      if (d==0) continue;
    
      struct dirent * entry;
      while ( (entry = readdir(d)) != 0) {
	const char * name = entry->d_name;
	unsigned int name_size = strlen(name);

	const DictExt * i = find_dict_ext(list_all.dict_exts, 
                                          ParmString(name, name_size));
	if (i == 0) // does not end in one of the extensions in list
	  continue;

	name_size -= i->ext_size;
      
	RET_ON_ERR(proc_file(list_all, config, 
			     dir, name, name_size, i->module));
      }
    }
    return no_err;
  }

  PosibErr<void> DictInfoList::proc_file(MDInfoListAll & list_all,
					 Config * config,
					 const char * dir,
					 const char * name,
					 unsigned int name_size,
					 const ModuleInfo * module)
  {
    DictInfoNode * * prev = &head_;
    StackPtr<DictInfoNode> to_add(new DictInfoNode());
    const char * p0;
    const char * p1;
    const char * p2;
    p0 = strnchr(name, '-', name_size);
    if (!module)
      p2 = strnrchr(name, '-', name_size);
    else
      p2 = name + name_size;
    if (p0 == 0)
      p0 = p2;
    p1 = p2;
    if (p0 + 2 < p1 && asc_isdigit(p1[-1]) && asc_isdigit(p1[-2]) && p1[-3] == '-')
      p1 -= 2;

    to_add->name.assign(name, p2-name);
    to_add->c_struct.name = to_add->name.c_str();
  
    to_add->code.assign(name, p0-name);
    to_add->c_struct.code = to_add->code.c_str();

    // check if the code is in a valid form and normalize entry.  
    // If its not in a valid form, then ignore this entry

    if (to_add->code.size() >= 2 
	&& asc_isalpha(to_add->code[0]) && asc_isalpha(to_add->code[1])) 
    {
      unsigned s = strcspn(to_add->code.str(), "_");
      if (s > 3) return no_err;
      unsigned i = 0;
      for (; i != s; ++i)
        to_add->name[i] = to_add->code[i] = asc_tolower(to_add->code[i]);
      i++;
      for (; i < to_add->code.size(); ++i)
        to_add->name[i] = to_add->code[i] = asc_toupper(to_add->code[i]);
    } else {
      return no_err;
    }
    
    // Need to do it here as module is about to get a value
    // if it is null
    to_add->direct = module == 0 ? false : true;

    if (!module) {
      assert(p2 != 0); //FIXME: return error
      ModuleInfoNode * mod 
	= list_all.module_info_list.find(p2+1, name_size - (p2+1-name));
      //FIXME: Check for null and return an error on an unknown module
      module = &(mod->c_struct);
    }
    to_add->c_struct.module = module;
  
    if (p0 + 1 < p1)
      to_add->variety.assign(p0+1, p1 - p0 - 1);
    to_add->c_struct.variety = to_add->variety.c_str();
  
    if (p1 != p2) 
      to_add->size_str.assign(p1, 2);
    else
      to_add->size_str = "60";
    to_add->c_struct.size_str = to_add->size_str.c_str();
    to_add->c_struct.size = atoi(to_add->c_struct.size_str);

    if (dir) {
      to_add->info_file  = dir;
      to_add->info_file += '/';
    }
    to_add->info_file += name;
  
    while (*prev != 0 && *(DictInfoNode *)*prev < *to_add)
      prev = &(*prev)->next;
    to_add->next = *prev;
    *prev = to_add.release();

    return no_err;
  }

  bool operator< (const DictInfoNode & r, const DictInfoNode & l)
  {
    const DictInfo & rhs = r.c_struct;
    const DictInfo & lhs = l.c_struct;
    int res = strcmp(rhs.code, lhs.code);
    if (res < 0) return true;
    if (res > 0) return false;
    res = strcmp(rhs.variety,lhs.variety);
    if (res < 0) return true;
    if (res > 0) return false;
    if (rhs.size < lhs.size) return true;
    if (rhs.size > lhs.size) return false;
    res = strcmp(rhs.module->name,lhs.module->name);
    if (res < 0) return true;
    return false;
  }

  PosibErr<void> get_dict_file_name(const DictInfo * mi, 
				    String & main_wl, String & flags)
  {
    const DictInfoNode * node = reinterpret_cast<const DictInfoNode *>(mi);
    if (node->direct) {
      main_wl = node->info_file;
      flags   = "";
      return no_err;
    } else {
      FStream f;
      RET_ON_ERR(f.open(node->info_file, "r"));
      String buf; DataPair dp;
      bool res = getdata_pair(f, dp, buf);
      main_wl = dp.key; flags = dp.value;
      f.close();
      if (!res)
	return make_err(bad_file_format,  node->info_file, "");
      return no_err;
    }
  }

  /////////////////////////////////////////////////////////////////
  //
  // Lists of Info Lists Impl
  //

  void get_data_dirs (Config * config,
		      StringList & lst)
  {
    lst.clear();
    lst.add(config->retrieve("data-dir"));
    lst.add(config->retrieve("dict-dir"));
  }

  DictExt::DictExt(ModuleInfo * m, const char * e)
  {
    module = m;
    ext_size = strlen(e);
    assert(ext_size <= max_ext_size);
    memcpy(ext, e, ext_size + 1);
  }

  void MDInfoListAll::clear()
  {
    module_info_list.clear();
    dict_dirs.clear();
    dict_exts.clear();
    dict_info_list.clear();
  }

  PosibErr<void> MDInfoListAll::fill(Config * c, 
                                     StringList & dirs)
  {
    PosibErr<void> err;

    err = fill_dict_aliases(c);
    if (err.has_err()) goto RETURN_ERROR;

    for_dirs = dirs;
    err = module_info_list.fill(*this, c);
    if (err.has_err()) goto RETURN_ERROR;

    fill_helper_lists(dirs);
    err = dict_info_list.fill(*this, c);
    if (err.has_err()) goto RETURN_ERROR;

    return err;

  RETURN_ERROR:
    clear();
    return err;
  }

  void MDInfoListAll::fill_helper_lists(const StringList & def_dirs)
  {
    dict_dirs = def_dirs;
    dict_exts.append(DictExt(0, ".awli"));

    for (ModuleInfoNode * n = module_info_list.head_; n != 0; n = n->next) 
    {
      {
	StringListEnumeration e = n->dict_dirs.elements_obj();
	const char * item;
	while ( (item = e.next()) != 0 )
	  dict_dirs.add(item);
      }{
	StringListEnumeration e = n->dict_exts.elements_obj();
	const char * item;
	while ( (item = e.next()) != 0 )
	  dict_exts.append(DictExt(&n->c_struct, item));
      }
    }
  }

  PosibErr<void> MDInfoListAll::fill_dict_aliases(Config * c)
  {
    StringList aliases;
    c->retrieve_list("dict-alias", &aliases);
    StringListEnumeration els = aliases.elements_obj();
    const char * str;
    while ( (str = els.next()) != 0) {
      const char * end = strchr(str, ' ');
      if (!end) return make_err(bad_value, "dict-alias", str, 
                                _("in the form \"<name> <value>\""));
      String name(str, end - str);
      while (asc_isspace(*end)) ++end;
      dict_aliases.insert(name.str(), end);
    }
    return no_err;
  }


  MDInfoListofLists::MDInfoListofLists()
    : data(0), offset(0), size(0)
  {
  }

  MDInfoListofLists::~MDInfoListofLists() {
    for (int i = offset; i != offset + size; ++i)
      data[i].clear();
    delete[] data;
  }

  void MDInfoListofLists::clear(Config * c)
  {
    StringList dirs;
    get_data_dirs(c, dirs);
    int pos = find(dirs);
    if (pos == -1) {
      data[pos - offset].clear();
    }
  }

  int MDInfoListofLists::find(const StringList & key)
  {
    for (int i = 0; i != size; ++i) {
      if (data[i].key == key)
	return i + offset;
    }
    return -1;
  }

  PosibErr<const MDInfoListAll *>
  MDInfoListofLists::get_lists(Config * c)
  {
    LOCK(&lock);
    Config * config = (Config *)c; // FIXME: WHY?
    int & pos = config->md_info_list_index;
    StringList dirs;
    StringList key;
    if (!valid_pos(pos)) {
      get_data_dirs(config, dirs);
      key = dirs;
      key.add("////////");
      config->retrieve_list("dict-alias", &key);
      pos = find(key);
    }
    if (!valid_pos(pos)) {
      MDInfoListAll * new_data = new MDInfoListAll[size + 1];
      for (int i = 0; i != size; ++i) {
	new_data[i] = data[i];
      }
      ++size;
      delete[] data;
      data = new_data;
      pos = size - 1 + offset;
    }
    MDInfoListAll & list_all = data[pos - offset];
    if (list_all.has_data())
      return &list_all;

    list_all.key = key;
    RET_ON_ERR(list_all.fill(config, dirs));

    return &list_all;
  }

  /////////////////////////////////////////////////////////////////
  //
  // utility functions
  //

  static const char * strnchr(const char * i, char c, unsigned int size)
  {
    const char * stop = i + size;
    while (i != stop) {
      if (*i == c)
	return i;
      ++i;
    }
    return 0;
  }

  static const char * strnrchr(const char * stop, char c, unsigned int size)
  {
    const char * i = stop + size - 1;
    --stop;
    while (i != stop) {
      if (*i == c)
	return i;
      --i;
    }
    return 0;
  }

  /////////////////////////////////////////////////////////////////
  //
  // user visible functions and enumeration impl
  //

  //
  // ModuleInfo
  //

  PosibErr<const ModuleInfoList *> get_module_info_list(Config * c)
  {
    RET_ON_ERR_SET(md_info_list_of_lists.get_lists(c), const MDInfoListAll *, la);
    return &la->module_info_list;
  }

  ModuleInfoEnumeration * ModuleInfoList::elements() const
  {
    return new ModuleInfoEnumeration((ModuleInfoNode *)head_);
  }

  unsigned int ModuleInfoList::size() const
  {
    return size_;
  }

  bool ModuleInfoList::empty() const
  {
    return size_ != 0;
  }

  ModuleInfoEnumeration * ModuleInfoEnumeration::clone () const
  {
    return new ModuleInfoEnumeration(*this);
  }

  void ModuleInfoEnumeration::assign(const ModuleInfoEnumeration * other)
  {
    *this = *other;
  }
  
  bool ModuleInfoEnumeration::at_end () const
  {
    return node_ == 0;
  }

  const ModuleInfo * ModuleInfoEnumeration::next ()
  {
    if (node_ == 0) return 0;
    const ModuleInfo * data = &(node_->c_struct);
    node_ = (ModuleInfoNode *)(node_->next);
    return data;
  }

  //
  // DictInfo
  //

  PosibErr<const DictInfoList *> get_dict_info_list(Config * c)
  {
    RET_ON_ERR_SET(md_info_list_of_lists.get_lists(c), const MDInfoListAll *, la);
    return &la->dict_info_list;
  }

  PosibErr<const StringMap *> get_dict_aliases(Config * c)
  {
    RET_ON_ERR_SET(md_info_list_of_lists.get_lists(c), const MDInfoListAll *, la);
    return &la->dict_aliases;
  }

  DictInfoEnumeration * DictInfoList::elements() const
  {
    return new DictInfoEnumeration(static_cast<DictInfoNode *>(head_));
  }

  unsigned int DictInfoList::size() const
  {
    return size_;
  }

  bool DictInfoList::empty() const
  {
    return size_ != 0;
  }

  DictInfoEnumeration * DictInfoEnumeration::clone() const
  {
    return new DictInfoEnumeration(*this);
  }

  void DictInfoEnumeration::assign(const DictInfoEnumeration * other)
  {
    *this = *other;
  }

  bool DictInfoEnumeration::at_end() const
  {
    return node_ == 0;
  }

  const DictInfo * DictInfoEnumeration::next ()
  {
    if (node_ == 0) return 0;
    const DictInfo * data = &(node_->c_struct);
    node_ = (DictInfoNode *)(node_->next);
    return data;
  }

}
