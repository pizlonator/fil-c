/* Print the source files of a given ELF file.
   Copyright (C) 2023 Red Hat, Inc.
   This file is part of elfutils.
   Written by Housam Alamour <alamourh@redhat.com>.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


/* In case we have a bad fts we include this before config.h because it
   can't handle _FILE_OFFSET_BITS.
   Everything we need here is fine if its declarations just come first.
   Also, include sys/types.h before fts.  On some systems fts.h is not self
   contained.  */
#ifdef BAD_FTS
#include <sys/types.h>
#include <fts.h>
#endif

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "printversion.h"
#include <dwarf.h>
#include <argp.h>
#include <cstring>
#include <set>
#include <string>
#include <cassert>
#include <gelf.h>
#include <memory>

#ifdef ENABLE_LIBDEBUGINFOD
#include "debuginfod.h"
#endif

#include <libdwfl.h>
#include <fcntl.h>
#include <iostream>
#include <libdw.h>
#include <sstream>
#include <vector>

/* Libraries for use by the --zip option */
#ifdef HAVE_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

/* If fts.h is included before config.h, its indirect inclusions may not
   give us the right LFS aliases of these functions, so map them manually.  */
#ifdef BAD_FTS
#ifdef _FILE_OFFSET_BITS
#define open open64
#define fopen fopen64
#endif
#else
  #include <sys/types.h>
  #include <fts.h>
#endif

using namespace std;

/* Name and version of program.  */
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;

constexpr size_t BUFFER_SIZE = 8192;

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, OPTION_DOC, N_("Output options:"), 1 },
  { "null", '0', NULL, 0,
    N_ ("Separate items by a null instead of a newline."), 0 },
  { "verbose", 'v', NULL, 0,
    N_ ("Increase verbosity of logging messages."), 0 },
  { "cu-only", 'c', NULL, 0, N_("Only list the CU names."), 0 },
  #ifdef HAVE_LIBARCHIVE
  { "zip", 'z', NULL, 0, N_("Zip all the source files and send to stdout. "
    "Cannot be used with the null option"), 0 },
    #ifdef ENABLE_LIBDEBUGINFOD
    { "no-backup", 'b', NULL, 0, N_("Disables local source file search when "
      "debuginfod fails to fetch files. This option is only applicable"
      "when fetching and zipping files."), 0 },
    #endif
  #endif
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("Lists the source files of a DWARF/ELF file.  The default input is the file 'a.out'.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("INPUT");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

static struct argp_child argp_children[2]; /* [0] is set in main.  */

/* Data structure to communicate with argp functions.  */
static const struct argp argp =
{
  options, parse_opt, args_doc, doc, argp_children, NULL, NULL
};

/* Verbose message printing.  */
static bool verbose;
/* Delimit the output with nulls.  */
static bool null_arg;
/* Only print compilation unit names.  */
static bool CU_only;
#ifdef HAVE_LIBARCHIVE
  /* Zip all the source files and send to stdout. */
  static bool zip;

  #ifdef ENABLE_LIBDEBUGINFOD
    /* Disables local source file search when debuginfod fails to fetch them.
       This option is only applicable when fetching and zipping files.*/
    static bool no_backup;
  #endif
#endif

/* Handle program arguments.  Note null arg and zip
    cannot be combined due to warnings raised when unzipping.  */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Suppress "unused parameter" warning.  */
  (void)arg;
  switch (key)
    {
    case ARGP_KEY_INIT:
      state->child_inputs[0] = state->input;
      break;

    case '0':
      null_arg = true;
      break;

    case 'v':
      verbose = true;
      break;

    case 'c':
      CU_only = true;
      break;

    #ifdef HAVE_LIBARCHIVE
      case 'z':
      zip = true;
      break;

      #ifdef ENABLE_LIBDEBUGINFOD
        case 'b':
        no_backup = true;
        break;
      #endif
    #endif

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/* Remove the "/./" , "../" and the preceding directory
    that some paths include which raise errors during unzip.  */
string canonicalize_path(string path)
{
    stringstream ss(path);
    string token;
    vector<string> tokens;
    /* Extract each directory of the path and place into a vector.  */
    while (getline(ss, token, '/')) {
      /* Ignore any empty //, or /./ dirs.  */
        if (token == "" || token == ".")
            continue;
      /* When /..  is encountered, remove the most recent directory from the vector.  */
        else if (token == "..") {
            if (!tokens.empty())
                tokens.pop_back();
        } else
            tokens.push_back(token);
    }
    stringstream result;
    if (tokens.empty())
        return "/";
    /* Reconstruct the path from the extracted directories.  */
    for (const string &t : tokens) {
        result << '/' << t;
    }
    return result.str();
}

/* Global list of collected source files and their respective module.
   Normally, it'll contain the sources of just one named binary, but
   the '-K' option can cause multiple dwfl modules to be loaded, thus
   listed.  */
set<pair<string, Dwfl_Module*>> debug_sourcefiles;

static int
collect_sourcefiles (Dwfl_Module *dwflmod,
                     void **userdata __attribute__ ((unused)),
                     const char *name __attribute__ ((unused)),
                     Dwarf_Addr base __attribute__ ((unused)),
                     void *arg __attribute__ ((unused)))
{
  Dwarf *dbg;
  Dwarf_Addr bias; /* ignored - for addressing purposes only.  */

  dbg = dwfl_module_getdwarf (dwflmod, &bias);

  Dwarf_Off offset = 0;
  Dwarf_Off old_offset;
  size_t hsize;
  /* Traverse all CUs of this module.  */
  while (dwarf_nextcu (dbg, old_offset = offset, &offset, &hsize, NULL, NULL, NULL) == 0)
    {
      Dwarf_Die cudie_mem;
      Dwarf_Die *cudie = dwarf_offdie (dbg, old_offset + hsize, &cudie_mem);

      if (cudie == NULL)
        continue;

      const char *cuname = dwarf_diename (cudie) ?: "<unknown>";
      Dwarf_Files *files;
      size_t nfiles;
      if (dwarf_getsrcfiles (cudie, &files, &nfiles) != 0)
        continue;

      /* extract DW_AT_comp_dir to resolve relative file names.  */
      const char *comp_dir = "";
      const char *const *dirs;
      size_t ndirs;

      if (dwarf_getsrcdirs (files, &dirs, &ndirs) == 0 && dirs[0] != NULL)
        comp_dir = dirs[0];
      if (comp_dir == NULL)
        comp_dir = "";

      if (verbose)
        clog << "searching for sources for cu=" << cuname
                  << " comp_dir=" << comp_dir << " #files=" << nfiles
                  << " #dirs=" << ndirs << endl;

      if (comp_dir[0] == '\0' && cuname[0] != '/')
        {
          /* This is a common symptom for dwz-compressed debug files,
             where the altdebug file cannot be resolved.  */
          if (verbose)
            clog << "skipping cu=" << cuname << " due to empty comp_dir" << endl;
          continue;
        }
      for (size_t f = 1; f < nfiles; ++f)
        {
          const char *hat;
          if (CU_only)
          {
            if (strcmp(cuname, "<unknown>") == 0 || strcmp(cuname, "<artificial>") == 0 )
              continue;
            hat = cuname;
          }
          else
            hat = dwarf_filesrc (files, f, NULL, NULL);

          if (hat == NULL)
            continue;

          if (string(hat).find("<built-in>")
              != string::npos) /* gcc intrinsics, don't bother recording */
            continue;

          string waldo;
          if (hat[0] == '/') /* absolute */
            waldo = (string (hat));
          else if (comp_dir[0] != '\0') /* comp_dir relative */
            waldo = (string (comp_dir) + string ("/") + string (hat));
          else
           {
             if (verbose)
              clog << "skipping file=" << hat << " due to empty comp_dir" << endl;
             continue;
           }
          waldo = canonicalize_path (waldo);
          debug_sourcefiles.insert (make_pair(waldo, dwflmod));
        }
    }
  return DWARF_CB_OK;
}

#ifdef HAVE_LIBARCHIVE
void zip_files()
{
  struct archive *a = archive_write_new();
  struct stat st;
  char buff[BUFFER_SIZE];
  int len;
  int fd;
  #ifdef ENABLE_LIBDEBUGINFOD
  /* Initialize a debuginfod client.  */
  static unique_ptr <debuginfod_client, void (*)(debuginfod_client*)>
    client (debuginfod_begin(), &debuginfod_end);
  #endif

  archive_write_set_format_zip(a);
  archive_write_open_fd(a, STDOUT_FILENO);

  int missing_files = 0;
  for (const auto &pair : debug_sourcefiles)
  {
    fd = -1;
    const std::string &file_path = pair.first;

    /* Attempt to query debuginfod client to fetch source files.  */
    #ifdef ENABLE_LIBDEBUGINFOD
    Dwfl_Module* dwflmod = pair.second;
    /* Obtain source file's build ID.  */
    const unsigned char *bits;
    GElf_Addr vaddr;
    int bits_length = dwfl_module_build_id(dwflmod, &bits, &vaddr);
    /* Ensure successful client and build ID acquisition.  */
    if (client.get() != NULL && bits_length > 0)
    {
      fd = debuginfod_find_source(client.get(),
                                    bits, bits_length,
                                    file_path.c_str(), NULL);
    }
    else
    {
        if (client.get() == NULL)
            cerr << "Error: Failed to initialize debuginfod client." << endl;
        else
            cerr << "Error: Invalid build ID length (" << bits_length << ")." << endl;
    }
    #endif

    if (!no_backup)
      /* Files could not be located using debuginfod, search locally */
      if (fd < 0)
        fd = open(file_path.c_str(), O_RDONLY);
    if (fd < 0)
    {
      if (verbose)
        cerr << file_path << endl;
      missing_files++;
      continue;
    }

    /* Create an entry for each file including file information to be placed in the zip.  */
    if (fstat(fd, &st) == -1)
    {
      if (verbose)
        cerr << file_path << endl;
      missing_files++;
      if (verbose)
        cerr << "Error: Failed to get file status for " << file_path << ": " << strerror(errno) << endl;
      continue;
    }
    struct archive_entry *entry = archive_entry_new();
    /* Removing first "/"" to make the path "relative" before zipping, otherwise warnings are raised when unzipping.  */
    string entry_name = file_path.substr(file_path.find_first_of('/') + 1);
    archive_entry_set_pathname(entry, entry_name.c_str());
    archive_entry_copy_stat(entry, &st);
    if (archive_write_header(a, entry) != ARCHIVE_OK)
    {
      if (verbose)
        cerr << file_path << endl;
      missing_files++;
      if (verbose)
        cerr << "Error: failed to write header for " << file_path << ": " << archive_error_string(a) << endl;
      continue;
    }

    /* Write the file to the zip.  */
    len = read(fd, buff, sizeof(buff));
    if (len == -1)
    {
      if (verbose)
        cerr << file_path << endl;
      missing_files++;
      if (verbose)
        cerr << "Error: Failed to open file: " << file_path << ": " << strerror(errno) <<endl;
      continue;
    }
    while (len > 0)
    {
      if (archive_write_data(a, buff, len) < ARCHIVE_OK)
      {
        if (verbose)
          cerr << "Error: Failed to read from the file: " << file_path << ": " << strerror(errno) << endl;
        break;
      }
      len = read(fd, buff, sizeof(buff));
    }
    close(fd);
    archive_entry_free(entry);
  }
  if (verbose && missing_files > 0 )
    cerr << missing_files << " file(s) listed above could not be found.  " << endl;

  archive_write_close(a);
  archive_write_free(a);
}
#endif

int
main (int argc, char *argv[])
{
  int remaining;

  /* Parse and process arguments.  This includes opening the modules.  */
  argp_children[0].argp = dwfl_standard_argp ();
  argp_children[0].group = 1;

  Dwfl *dwfl = NULL;
  (void) argp_parse (&argp, argc, argv, 0, &remaining, &dwfl);
  assert (dwfl != NULL);
  /* Process all loaded modules - probably just one, except if -K or -p is used.  */
  (void) dwfl_getmodules (dwfl, &collect_sourcefiles, NULL, 0);

  if (!debug_sourcefiles.empty ())
  {
    #ifdef HAVE_LIBARCHIVE
      if (zip)
        zip_files();
      else
    #endif
      {
        for (const auto &pair : debug_sourcefiles)
          {
            cout << pair.first;
            if (null_arg)
              cout << '\0';
            else
              cout << '\n';
          }
      }
  }

  dwfl_end (dwfl);
  return 0;
}
