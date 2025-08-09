/* Toybox infrastructure.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 */

#include "toys.h"

// Populate toy_list[].

#undef NEWTOY
#undef OLDTOY
#define NEWTOY(name, opts, flags) {#name, name##_main, OPTSTR_##name, flags},
#define OLDTOY(name, oldname, flags) \
  {#name, oldname##_main, OPTSTR_##oldname, flags},

struct toy_list toy_list[] = {
#include "generated/newtoys.h"
};

// global context for this command.

struct toy_context toys;
union global_union this;
char *toybox_version = TOYBOX_VERSION, toybuf[4096], libbuf[4096];

struct toy_list *toy_find(char *name)
{
  int top, bottom, middle;

  if (!CFG_TOYBOX || strchr(name, '/')) return 0;

  // Multiplexer name works as prefix, else skip first entry (it's out of order)
  if (!toys.which && strstart(&name, toy_list->name)) return toy_list;
  bottom = 1;

  // Binary search to find this command.
  top = ARRAY_LEN(toy_list)-1;
  for (;;) {
    int result;

    middle = (top+bottom)/2;
    if (middle<bottom || middle>top) return 0;
    result = strcmp(name,toy_list[middle].name);
    if (!result) return toy_list+middle;
    if (result<0) top = --middle;
    else bottom = ++middle;
  }
}

// Figure out whether or not anything is using the option parsing logic,
// because the compiler can't figure out whether or not to optimize it away
// on its' own.  NEED_OPTIONS becomes a constant allowing if() to optimize
// stuff out via dead code elimination.

#undef NEWTOY
#undef OLDTOY
#define NEWTOY(name, opts, flags) opts ||
#define OLDTOY(name, oldname, flags) OPTSTR_##oldname ||
static const int NEED_OPTIONS =
#include "generated/newtoys.h"
0;  // Ends the opts || opts || opts...

// Populate help text array

#undef NEWTOY
#undef OLDTOY
#define NEWTOY(name,opt,flags) HELP_##name "\0"
#if CFG_TOYBOX
#define OLDTOY(name,oldname,flags) "\xff" #oldname "\0"
#else
#define OLDTOY(name, oldname, flags) HELP_##oldname "\0"
#endif

#include "generated/help.h"
static const char help_data[] =
#include "generated/newtoys.h"
;

#if CFG_TOYBOX_ZHELP
#include "generated/zhelp.h"
#else
static char *zhelp_data = 0;
#define ZHELP_LEN 0
#endif

void show_help(FILE *out, int flags)
{
  int i = toys.which-toy_list;
  char *s, *ss, *hd;

  if (!CFG_TOYBOX_HELP) return;

  if (CFG_TOYBOX_ZHELP)
    gunzip_mem(zhelp_data, sizeof(zhelp_data), hd = xmalloc(ZHELP_LEN),
      ZHELP_LEN);
  else hd = (void *)help_data;

  if (flags & HELP_HEADER)
    fprintf(out, "Toybox %s"USE_TOYBOX(" multicall binary")"%s\n\n",
      toybox_version, (CFG_TOYBOX && i) ? " (see toybox --help)"
      : " (see https://landley.net/toybox)");

  for (;;) {
    s = (void *)help_data;
    while (i--) s += strlen(s) + 1;
    // If it's an alias, restart search for real name
    if (*s != 255) break;
    i = toy_find(++s)-toy_list;
    if ((flags & HELP_SEE) && toy_list[i].flags) {
      if (flags & HELP_HTML) fprintf(out, "See <a href=#%s>%s</a>\n", s, s);
      else fprintf(out, "%s see %s\n", toys.which->name, s);

      return;
    }
  }

  if (!(flags & HELP_USAGE)) fprintf(out, "%s\n", s);
  else {
    strstart(&s, "usage: ");
    for (ss = s; *ss && *ss!='\n'; ss++);
    fprintf(out, "%.*s\n", (int)(ss-s), s);
  }
}

static void unknown(char *name)
{
  toys.exitval = 127;
  toys.which = toy_list;
  help_exit("Unknown command %s", name);
}

// Parse --help and --version for (almost) all commands
void check_help(char **arg)
{
  long flags = toys.which->flags;

  if (!CFG_TOYBOX_HELP_DASHDASH || !*arg) return;
  if (!CFG_TOYBOX || toys.which!=toy_list) if (flags&TOYFLAG_NOHELP) return;

  if (!strcmp(*arg, "--help")) {
    if (CFG_TOYBOX && toys.which == toy_list && arg[1]) {
      toys.which = 0;
      if (!(toys.which = toy_find(arg[1]))) unknown(arg[1]);
    }
    show_help(stdout, HELP_HEADER);
    xexit();
  }

  if (!strcmp(*arg, "--version")) {
    // Lie to autoconf when it asks stupid questions, so configure regexes
    // that look for "GNU sed version %f" greater than some old buggy number
    // don't fail us for not matching their narrow expectations.
    *toybuf = 0;
    if (flags&TOYFLAG_AUTOCONF)
      sprintf(toybuf, " (is not GNU %s 9.0)", toys.which->name);
    xprintf("toybox %s%s\n", toybox_version, toybuf);
    xexit();
  }
}

// Setup toybox global state for this command.
void toy_singleinit(struct toy_list *which, char *argv[])
{
  toys.which = which;
  toys.argv = argv;
  toys.toycount = ARRAY_LEN(toy_list);

  if (NEED_OPTIONS && which->options) get_optflags();
  else {
    check_help(toys.optargs = argv+1);
    for (toys.optc = 0; toys.optargs[toys.optc]; toys.optc++);
  }

  // Setup we only want to do once: skip for multiplexer or NOFORK reentry
  if (!(CFG_TOYBOX && which == toy_list) && !(which->flags & TOYFLAG_NOFORK)) {
    char *buf = 0;
    int btype = _IOFBF;

    toys.old_umask = umask(0);
    if (!(which->flags & TOYFLAG_UMASK)) umask(toys.old_umask);

    // Try user's locale, but if that isn't UTF-8 merge in a UTF-8 locale's
    // character type data. (Fall back to en_US for MacOS.)
    setlocale(LC_CTYPE, "");
    if (strcmp("UTF-8", nl_langinfo(CODESET)))
      uselocale(newlocale(LC_CTYPE_MASK, "C.UTF-8", 0) ? :
        newlocale(LC_CTYPE_MASK, "en_US.UTF-8", 0));

    if (which->flags & TOYFLAG_LINEBUF) btype = _IOLBF;
    else if (which->flags & TOYFLAG_NOBUF) btype = _IONBF;
    else buf = xmalloc(4096);
    setvbuf(stdout, buf, btype, buf ? 4096 : 0);
  }
}

// Full init needed by multiplexer or reentrant calls, calls singleinit at end
void toy_init(struct toy_list *which, char *argv[])
{
  void *oldwhich = toys.which;

  // Drop permissions for non-suid commands.
  if (CFG_TOYBOX_SUID) {
    if (!toys.which) toys.which = toy_list;

    uid_t uid = getuid(), euid = geteuid();

    if (!(which->flags & TOYFLAG_STAYROOT)) {
      if (uid != euid) {
        if (setuid(uid)) perror_exit("setuid %d->%d", euid, uid); // drop root
        euid = uid;
        toys.wasroot++;
      }
    } else if (CFG_TOYBOX_DEBUG && uid && which != toy_list)
      error_msg("Not installed suid root");

    if ((which->flags & TOYFLAG_NEEDROOT) && euid) {
      toys.which = which;
      check_help(argv+1);
      help_exit("Not root");
    }
  }

  memset(&toys, 0, offsetof(struct toy_context, rebound));
  if (oldwhich) memset(&this, 0, sizeof(this));

  // Continue to portion of init needed by standalone commands
  toy_singleinit(which, argv);
}

// Run an internal toybox command.
// Only returns if it can't run command internally, otherwise xexit() when done.
void toy_exec_which(struct toy_list *which, char *argv[])
{
  // Return if we can't find it (which includes no multiplexer case),
  if (!which || (which->flags&TOYFLAG_NOFORK)) return;

  // Return if stack depth getting noticeable (proxy for leaked heap, etc).

  // Compiler writers have decided subtracting char * is undefined behavior,
  // so convert to integers. (LP64 says sizeof(long)==sizeof(pointer).)
  // Signed typecast so stack growth direction is irrelevant: we're measuring
  // the distance between two pointers on the same stack, hence the labs().
  if (!CFG_TOYBOX_NORECURSE && toys.stacktop)
    if (labs((long)toys.stacktop-(long)&which)>24000) return;

  // Return if we need to re-exec to acquire root via suid bit.
  if (toys.which && (which->flags&TOYFLAG_ROOTONLY) && toys.wasroot) return;

  // Run command
  toy_init(which, argv);
  if (toys.which) toys.which->toy_main();
  xexit();
}

// Lookup internal toybox command to run via argv[0]
void toy_exec(char *argv[])
{
  toy_exec_which(toy_find(*argv), argv);
}

// Multiplexer command, first argument is command to run, rest are args to that.
// If first argument starts with - output list of command install paths.
void toybox_main(void)
{
  char *toy_paths[] = {"usr/", "bin/", "sbin/", 0}, *s = toys.argv[1];
  int i, len = 0;
  unsigned width = 80;

  // fast path: try to exec immediately.
  // (Leave toys.which null to disable suid return logic.)
  // Try dereferencing symlinks until we hit a recognized name
  while (s) {
    char *ss = basename(s);
    struct toy_list *tl = toy_find(ss);

    if (tl==toy_list && s!=toys.argv[1]) unknown(ss);
    toy_exec_which(tl, toys.argv+1);
    s = (0<readlink(s, libbuf, sizeof(libbuf))) ? libbuf : 0;
  }

  // For early error reporting
  toys.which = toy_list;

  if (toys.argv[1] && strcmp(toys.argv[1], "--long")) unknown(toys.argv[1]);

  // Output list of commands.
  terminal_size(&width, 0);
  for (i = 1; i<ARRAY_LEN(toy_list); i++) {
    int fl = toy_list[i].flags;
    if (fl & TOYMASK_LOCATION) {
      if (toys.argv[1]) {
        int j;
        for (j = 0; toy_paths[j]; j++)
          if (fl & (1<<j)) len += printf("%s", toy_paths[j]);
      }
      len += printf("%s",toy_list[i].name);
      if (++len > width-15) len = 0;
      xputc(len ? ' ' : '\n');
    }
  }
  xputc('\n');
}

int main(int argc, char *argv[])
{
  // don't segfault if our environment is crazy
  // TODO mooted by kernel commit dcd46d897adb7 5.17 kernel Jan 2022
  if (!*argv) return 127;

  // Snapshot stack location so we can detect recursion depth later.
  // Nommu has special reentry path, !stacktop = "vfork/exec self happened"
  if (!CFG_TOYBOX_FORK && (0x80 & **argv)) **argv &= 0x7f;
  else {
    int stack_start;  // here so probe var won't permanently eat stack

    toys.stacktop = &stack_start;
  }

  if (CFG_TOYBOX) {
    // Call the multiplexer with argv[] as its arguments so it can toy_find()
    toys.argv = argv-1;
    toybox_main();
  } else {
    // single command built standalone with no multiplexer is first list entry
    toy_singleinit(toy_list, argv);
    toy_list->toy_main();
  }

  xexit();
}
