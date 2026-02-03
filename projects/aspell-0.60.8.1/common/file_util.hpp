// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_FILE_UTIL__HPP
#define ASPELL_FILE_UTIL__HPP

#include <time.h>

#include "string.hpp"
#include "posib_err.hpp"

namespace acommon {

  class FStream;
  class Config;
  class StringList;

  bool need_dir(ParmString file);
  String add_possible_dir(ParmString dir, ParmString file);
  String figure_out_dir(ParmString dir, ParmString file);

  // FIXME: Possible remove
  //void open_file(FStream & in, const string & file,
  //               ParmString mode = "r");
  time_t get_modification_time(FStream & f);
  PosibErr<void> open_file_readlock(FStream& in, ParmString file);
  PosibErr<bool> open_file_writelock(FStream & in, ParmString file);
  // returns true if the file already exists
  void truncate_file(FStream & f, ParmString name);
  bool remove_file(ParmString name);
  bool file_exists(ParmString name);
  bool rename_file(ParmString orig, ParmString new_name);
  // will return NULL if path is NULL.
  const char * get_file_name(const char * path);

  // expands filename to the full path
  // returns the length of the directory part or 0 if nothing found
  unsigned find_file(const Config *, const char * option, String & filename);
  unsigned find_file(const StringList &, String & filename);

  class StringEnumeration;

  class PathBrowser
  {
    String suffix;
    String path;
    StringEnumeration * els;
    void * dir_handle;
    const char * dir;
    PathBrowser(const PathBrowser &);
    void operator= (const PathBrowser &);
  public:
    PathBrowser(const StringList &, const char * suf = "");
    ~PathBrowser();
    const char * next();
  };

}

#endif
