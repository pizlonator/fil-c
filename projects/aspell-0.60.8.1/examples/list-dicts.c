/* This file is part of The New Aspell
 * Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/. 
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "aspell.h"

int main(int argc, const char *argv[]) 
{
  AspellConfig * config;
  AspellDictInfoList * dlist;
  AspellDictInfoEnumeration * dels;
  const AspellDictInfo * entry;

  config = new_aspell_config();

  /* the returned pointer should _not_ need to be deleted */
  dlist = get_aspell_dict_info_list(config);

  /* config is no longer needed */
  delete_aspell_config(config);

  dels = aspell_dict_info_list_elements(dlist);

  printf("%-30s%-8s%-20s%-6s%-10s\n", "NAME", "CODE", "JARGON", "SIZE", "MODULE");
  while ( (entry = aspell_dict_info_enumeration_next(dels)) != 0) 
  {
    printf("%-30s%-8s%-20s%-6s%-10s\n",
	   entry->name,
	   entry->code, entry->jargon, 
	   entry->size_str, entry->module->name);
  }

  delete_aspell_dict_info_enumeration(dels);

  return 0;
}

