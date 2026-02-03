#include "config.hpp"
#include "file_util.hpp"
#include "file_data_util.hpp"

namespace acommon {
  
  // Fill in values for a local data directory and a system data directory
  // The values are stored under the keys local-data-dir and data-dir.
  // If the is no local-data-dir value, use the directory from master-path.
  // If there is no directory in master-path, use the current working dir.
  // FIXME: The case when there is no "/" in the master-path should not
  //   happen since it is an internal option.  Unofficially, it can still
  //   be set by the user.  This needs to eventually be fixed.
  void fill_data_dir(const Config * config, String & dir1, String & dir2) {
    if (config->have("local-data-dir")) {
      dir1 = config->retrieve("local-data-dir");
      if (!dir1.empty() && dir1[dir1.size()-1] != '/') dir1 += '/';
    } else {
      dir1 = config->retrieve("master-path");
      size_t pos = dir1.rfind('/');
      if (pos != String::npos)
        dir1.resize(pos + 1);
      else
        dir1 = "./";
    }
    dir2 = config->retrieve("data-dir");
    if (dir2[dir2.size()-1] != '/') dir2 += '/';
  }
  
  const String & find_file(String & file,
                           const String & dir1, const String & dir2, 
                           const String & name, const char * extension)
  {
    file = dir1 + name + extension;
    if (file_exists(file)) return dir1;
    file = dir2 + name + extension;
    return dir2;
  }

  bool find_file(String & file,
                 const String & dir1, const String & dir2, 
                 const String & name, 
                 ParmString preext, ParmString ext)
  {
    bool try_name_only = false;
    if (name.size() > ext.size() 
        && memcmp(name.c_str() + name.size() - ext.size(), 
                  ext, ext.size()) == 0) try_name_only = true;
    if (!try_name_only) {
      String n = name; n += preext; n += ext;
      file = dir1 + n;
      if (file_exists(file)) return true;
      file = dir2 + n;
      if (file_exists(file)) return true;

      n = name; n += ext;
      file = dir1 + n;
      if (file_exists(file)) return true;
      file = dir2 + n;
      if (file_exists(file)) return true;
    }

    file = dir1 + name;
    if (file_exists(file)) return true;
    file = dir2 + name;
    if (file_exists(file)) return true;

    if (try_name_only) {file = dir1 + name;}
    else               {file = dir1 + name; file += preext; file += ext;}
    
    return false;
  }
  
  
}
