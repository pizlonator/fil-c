/* help.c - Show help for toybox commands
 *
 * Copyright 2007 Rob Landley <rob@landley.net>
 *
 * Often a shell builtin.

USE_HELP(NEWTOY(help, "ahu", TOYFLAG_BIN|TOYFLAG_MAYFORK))

config HELP
  bool "help"
  default y
  depends on TOYBOX_HELP
  help
    usage: help [-ahu] [COMMAND]

    -a	All commands
    -u	Usage only
    -h	HTML output

    Show usage information for toybox commands.
    Run "toybox" with no arguments for a list of available commands.
*/

#define FOR_help
#include "toys.h"

static void do_help(struct toy_list *t)
{
  if (FLAG(h))
    xprintf("<a name=\"%s\"><h1>%s</h1><blockquote><pre>\n", t->name, t->name);

  toys.which = t;
  show_help(stdout, HELP_USAGE*FLAG(u) + (HELP_SEE|HELP_HTML)*FLAG(h));

  if (FLAG(h)) xprintf("</blockquote></pre>\n");
}

// Simple help is just toys.which = toy_find("name"); show_help(stdout, 0);
// but iterating through html output and all commands is a bit more

void help_main(void)
{
  long i;

  // If called with no arguments as a builtin from the shell, show all builtins
  if (toys.rebound && !*toys.optargs && !toys.optflags) {
    for (i = 0; i < toys.toycount; i++) {
      if (!(toy_list[i].flags&(TOYFLAG_NOFORK|TOYFLAG_MAYFORK))) continue;
      toys.which = toy_list+i;
      show_help(stdout, HELP_SEE|HELP_USAGE*!toys.optflags);
    }
    return;
  }

  if (!FLAG(a)) {
    struct toy_list *t = toys.which, *tt;

    // Zero which to include "toybox" in search, put it back for error msg
    toys.which = 0;
    tt = *toys.optargs ? toy_find(*toys.optargs) : t;
    toys.which = t;

    if (tt) return do_help(tt);
    else error_exit("Unknown command '%s'", *toys.optargs);
  }

  if (FLAG(h)) {
    sprintf(toybuf, "Toybox %s command help", toybox_version);
    xprintf("<html>\n<title>%s</title>\n<body>\n<h1>%s</h1><hr /><p>",
            toybuf, toybuf);
    for (i = 0; i<toys.toycount; i++) if (toy_list[i].flags)
      xprintf("<a href=\"#%s\">%s</a>\n", toy_list[i].name,toy_list[i].name);
    xprintf("</p>\n");
  }

  for (i = 0; i < toys.toycount; i++) {
    if (!toy_list[i].flags) continue;
    if (FLAG(h)) xprintf("<hr>\n<pre>\n");
    else if (!FLAG(u)) {
      memset(toybuf, '-', 78);
      memcpy(toybuf+3, toy_list[i].name, strlen(toy_list[i].name));
      printf("\n%s\n\n", toybuf);
    }
    do_help(toy_list+i);
    if (FLAG(h)) xprintf("</pre>\n");
  }

  if (FLAG(h)) xprintf("</html>");
}
