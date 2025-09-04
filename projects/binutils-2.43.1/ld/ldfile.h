/* ldfile.h -
   Copyright (C) 1991-2024 Free Software Foundation, Inc.

   This file is part of the GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#ifndef LDFILE_H
#define LDFILE_H

extern bool ldfile_assumed_script;
extern unsigned long ldfile_output_machine;
extern enum bfd_architecture ldfile_output_architecture;
extern const char *ldfile_output_machine_name;

enum search_dir_source
{
  search_dir_cmd_line,
#if BFD_SUPPORTS_PLUGINS
  search_dir_plugin,
#endif
  search_dir_linker_script
};

/* Structure used to hold the list of directories to search for
   libraries.  */

typedef struct search_dirs
{
  /* Next directory on list.  */
  struct search_dirs *next;
  /* Name of directory.  */
  const char *name;
  /* Where the search path came from.  */
  enum search_dir_source source;
#if BFD_SUPPORTS_PLUGINS
  /* For search dirs added by plugins, the plugin that added them.  */
  void * plugin;
#endif
} search_dirs_type;

enum script_open_style
{
  script_nonT,
  script_T,
  script_defaultT
};

struct script_name_list
{
  struct script_name_list *  next;
  enum script_open_style     open_how;
  char                       name[1];
};

extern struct script_name_list * processed_scripts;

extern search_dirs_type *search_head;

extern void ldfile_add_arch
  (const char *);
extern search_dirs_type * ldfile_add_library_path
  (const char *, enum search_dir_source);
extern void ldfile_open_command_file
  (const char *name);
extern void ldfile_open_script_file
  (const char *name);
extern void ldfile_open_default_command_file
  (const char *name);
extern void ldfile_open_file
  (struct lang_input_statement_struct *);
extern bool ldfile_try_open_bfd
  (const char *, struct lang_input_statement_struct *);
extern void ldfile_set_output_arch
  (const char *, enum bfd_architecture);
extern bool ldfile_open_file_search
  (const char *arch, struct lang_input_statement_struct *,
   const char *lib, const char *suffix);

extern void ldfile_add_remap
  (const char *, const char *);
extern bool ldfile_add_remap_file
  (const char *);
extern void ldfile_remap_input_free
  (void);
extern const char * ldfile_possibly_remap_input
  (const char *);
extern void ldfile_print_input_remaps
  (void);

#endif
