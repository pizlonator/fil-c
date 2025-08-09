/* args.c - Command line argument parsing.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 */

// NOTE: If option parsing segfaults, switch on TOYBOX_DEBUG in menuconfig to
// add syntax checks to option string parsing which aren't needed in the final
// code (since get_opt string is hardwired and should be correct when you ship)

#include "toys.h"

// Design goals:
//   Don't use getopt() out of libc.
//   Don't permute original arguments (screwing up ps/top output).
//   Integrated --long options "(noshort)a(along)b(blong1)(blong2)"

/* This uses a getopt-like option string, but not getopt() itself. We call
 * it the get_opt string.
 *
 * Each option in the get_opt string corresponds to a bit position in the
 * return value. The rightmost argument is (1<<0), the next to last is (1<<1)
 * and so on. If the option isn't seen in argv[], its bit remains 0.
 *
 * Options which have an argument fill in the corresponding slot in the global
 * union "this" (see generated/globals.h), which it treats as an array of longs
 * (note that sizeof(long)==sizeof(pointer) is guaranteed by LP64).
 *
 * You don't have to free the option strings, which point into the environment
 * space. List objects should be freed by main() when command_main() returns.
 *
 * Example:
 *   Calling get_optflags() when toys.which->options="ab:c:d" and
 *   argv = ["command", "-b", "fruit", "-d", "walrus"] results in:
 *
 *     Changes to struct toys:
 *       toys.optflags = 5 (I.E. 0101 so -b = 4 | -d = 1)
 *       toys.optargs[0] = "walrus" (leftover argument)
 *       toys.optargs[1] = NULL (end of list)
 *       toys.optc = 1 (there was 1 leftover argument)
 *
 *     Changes to union this:
 *       this[0]=NULL (because -c didn't get an argument this time)
 *       this[1]="fruit" (argument to -b)
 */

// What you can put in a get_opt string:
//   Any otherwise unused character (all letters, unprefixed numbers) specify
//   an option that sets a flag. The bit value is the same as the binary digit
//   if you string the option characters together in order.
//   So in "abcdefgh" a = 128, h = 1
//
//   Suffixes specify that this option takes an argument (stored in GLOBALS):
//       Note that pointer and long are always the same size, even on 64 bit.
//     : string argument, keep most recent if more than one
//     * string argument, appended to a struct arg_list linked list.
//     # signed long argument
//       <LOW     - die if less than LOW
//       >HIGH    - die if greater than HIGH
//       =DEFAULT - value if not specified
//     - signed long argument defaulting to negative (say + for positive)
//     . double precision floating point argument (with CFG_TOYBOX_FLOAT)
//       Chop this option out with USE_TOYBOX_FLOAT() in option string
//       Same <LOW>HIGH=DEFAULT as #
//     @ occurrence counter (which is a long)
//     % time offset in milliseconds with optional s/m/h/d suffix
//     (longopt)
//     | this is required. If more than one marked, only one required.
//     ; Option's argument is optional, and must be collated: -aARG or --a=ARG
//     ^ Stop parsing after encountering this argument
//    " " (space char) the "plus an argument" must be separate
//        I.E. "-j 3" not "-j3". So "kill -stop" != "kill -s top"
//
//   At the beginning of the get_opt string (before any options):
//     <0 die if less than # leftover arguments (default 0)
//     >9 die if > # leftover arguments (default MAX_INT)
//     0 Include argv[0] in optargs
//     ^ stop at first nonoption argument
//     ? Pass unknown arguments through to command (implied when no flags).
//     & first arg has imaginary dash (ala tar/ps/ar) which sets FLAGS_NODASH
//     ~ Collate following bare longopts (as if under short opt, repeatable)
//
//   At the end: [groups] of previously seen options
//     - Only one in group (switch off)    [-abc] means -ab=-b, -ba=-a, -abc=-c
//     + Synonyms (switch on all)          [+abc] means -ab=-abc, -c=-abc
//     ! More than one in group is error   [!abc] means -ab calls error_exit()
//       primarily useful if you can switch things back off again.
//
//   You may use octal escapes with the high bit (128) set to use a control
//   character as an option flag. For example, \300 would be the option -@

// Notes from getopt man page
//   - and -- cannot be arguments.
//     -- force end of arguments
//     - is a synonym for stdin in file arguments
//   -abcd means -a -b -c -d (but if -b takes an argument, then it's -a -b cd)

// Linked list of all known options (option string parsed into this).
// Hangs off getoptflagstate, freed at end of option parsing.
struct opts {
  struct opts *next;
  long *arg;         // Pointer into union "this" to store arguments at.
  int c;             // Argument character to match
  int flags;         // |=1, ^=2, " "=4, ;=8
  unsigned long long dex[3]; // bits to disable/enable/exclude in toys.optflags
  char type;         // Type of arguments to store union "this"
  union {
    long l;
    FLOAT f;
  } val[3];          // low, high, default - range of allowed values
};

// linked list of long options. (Hangs off getoptflagstate, free at end of
// option parsing, details about flag to set and global slot to fill out
// stored in related short option struct, but if opt->c = -1 the long option
// is "bare" (has no corresponding short option).
struct longopts {
  struct longopts *next;
  struct opts *opt;
  char *str;
  int len;
};

// State during argument parsing.
struct getoptflagstate
{
  int argc, minargs, maxargs;
  char *arg;
  struct opts *opts;
  struct longopts *longopts;
  int noerror, nodash_now, stopearly;
  unsigned excludes, requires;
};

static void forget_arg(struct opts *opt)
{
  if (opt->arg) {
    if (opt->type=='*') llist_traverse((void *)*opt->arg, free);
    *opt->arg = opt->val[2].l;
  }
}

// Use getoptflagstate to parse one command line option from argv
// Sets flags, saves/clears opt->arg, advances gof->arg/gof->argc as necessary
static void gotflag(struct getoptflagstate *gof, struct opts *opt, int longopt)
{
  unsigned long long i;
  struct opts *and;
  char *arg;
  int type;

  // Did we recognize this option?
  if (!opt) help_exit("Unknown option '%s'", gof->arg);

  // Might enabling this switch off something else?
  if (toys.optflags & opt->dex[0]) {
    // Forget saved argument for flag we switch back off
    for (and = gof->opts, i = 1; and; and = and->next, i<<=1)
      if (i & toys.optflags & opt->dex[0]) forget_arg(and);
    toys.optflags &= ~opt->dex[0];
  }

  // Set flags
  toys.optflags |= opt->dex[1];
  gof->excludes |= opt->dex[2];
  if (opt->flags&2) gof->stopearly=2;

  if (toys.optflags & gof->excludes) {
    for (and = gof->opts, i = 1; and; and = and->next, i<<=1) {
      if (opt == and || !(i & toys.optflags)) continue;
      if (toys.optflags & and->dex[2]) break;
    }
    if (and) help_exit("No '%c' with '%c'", opt->c, and->c);
  }

  // Are we NOT saving an argument? (Type 0, '@', unattached ';', short ' ')
  if (*(arg = gof->arg)) gof->arg++;
  if ((type = opt->type) == '@') {
    ++*opt->arg;
    return;
  }
  if (!longopt && *gof->arg && (opt->flags & 4)) return forget_arg(opt);
  if (!type || (!arg[!longopt] && (opt->flags & 8))) return forget_arg(opt);

  // Handle "-xblah" and "-x blah", but also a third case: "abxc blah"
  // to make "tar xCjfv blah1 blah2 thingy" work like
  // "tar -x -C blah1 -j -f blah2 -v thingy"

  if (longopt && *arg) arg++;
  else arg = (gof->nodash_now||!*gof->arg) ? toys.argv[++gof->argc] : gof->arg;
  if (!gof->nodash_now) gof->arg = "";
  if (!arg) {
    struct longopts *lo;

    arg = "Missing argument to ";
    if (opt->c != -1) help_exit("%s-%c", arg, opt->c);
    for (lo = gof->longopts; lo->opt != opt; lo = lo->next);
    help_exit("%s--%.*s", arg, lo->len, lo->str);
  }

  // Parse argument by type
  if (type == ':') *(opt->arg) = (long)arg;
  else if (type == '*') {
    struct arg_list **list;

    list = (struct arg_list **)opt->arg;
    while (*list) list=&((*list)->next);
    *list = xzalloc(sizeof(struct arg_list));
    (*list)->arg = arg;
  } else if (type == '#' || type == '-' || type == '%') {
    long long l = (type == '%') ? xparsemillitime(arg) : atolx(arg);

    if (type == '-' && !ispunct(*arg)) l*=-1;
    arg = (type == '%') ? "ms" : "";
    if (l < opt->val[0].l) help_exit("-%c < %ld%s", opt->c, opt->val[0].l, arg);
    if (l > opt->val[1].l) help_exit("-%c > %ld%s", opt->c, opt->val[1].l, arg);

    *(opt->arg) = l;
  } else if (CFG_TOYBOX_FLOAT && type == '.') {
    FLOAT *f = (FLOAT *)(opt->arg);

    *f = strtod(arg, &arg);
    if (opt->val[0].l != LONG_MIN && *f < opt->val[0].f)
      help_exit("-%c < %lf", opt->c, (double)opt->val[0].f);
    if (opt->val[1].l != LONG_MAX && *f > opt->val[1].f)
      help_exit("-%c > %lf", opt->c, (double)opt->val[1].f);
  }
}

// Parse this command's options string into struct getoptflagstate, which
// includes a struct opts linked list in reverse order (I.E. right-to-left)
static int parse_optflaglist(struct getoptflagstate *gof)
{
  char *options = toys.which->options;
  long *nextarg = (long *)&this;
  struct opts *new = 0;
  int idx, rc = 0;

  // Parse option format string
  memset(gof, 0, sizeof(struct getoptflagstate));
  gof->maxargs = INT_MAX;
  if (!options) return 0;

  // Parse leading special behavior indicators
  for (;;) {
    if (*options == '^') gof->stopearly++;
    else if (*options == '<') gof->minargs=*(++options)-'0';
    else if (*options == '>') gof->maxargs=*(++options)-'0';
    else if (*options == '?') gof->noerror++;
    else if (*options == '&') gof->nodash_now = 1;
    else if (*options == '0') rc = 1;
    else break;
    options++;
  }

  // Parse option string into a linked list of options with attributes.

  if (!*options) gof->noerror++;
  while (*options) {
    char *temp;

    // Option groups come after all options are defined
    if (*options == '[') break;

    // Allocate a new list entry when necessary
    if (!new) {
      new = xzalloc(sizeof(struct opts));
      new->next = gof->opts;
      gof->opts = new;
      new->val[0].l = LONG_MIN;
      new->val[1].l = LONG_MAX;
    }
    // Each option must start with "(" or an option character. (Bare
    // longopts only come at the start of the string.)
    if (*options == '(' && new->c != -1) {
      char *end;
      struct longopts *lo;

      // Find the end of the longopt
      for (end = ++options; *end && *end != ')'; end++);
      if (CFG_TOYBOX_DEBUG && !*end) error_exit("(longopt) didn't end");

      // init a new struct longopts
      lo = xmalloc(sizeof(struct longopts));
      lo->next = gof->longopts;
      lo->opt = new;
      lo->str = options;
      lo->len = end-options;
      gof->longopts = lo;
      options = ++end;

      // Mark this struct opt as used, even when no short opt.
      if (!new->c) new->c = -1;

      continue;

    // If this is the start of a new option that wasn't a longopt,

    } else if (strchr(":*#@.-%", *options)) {
      if (CFG_TOYBOX_DEBUG && new->type)
        error_exit("multiple types %c:%c%c", new->c, new->type, *options);
      new->type = *options;
    } else if (-1 != (idx = stridx("|^ ;", *options))) new->flags |= 1<<idx;
    // bounds checking
    else if (-1 != (idx = stridx("<>=", *options))) {
      if (new->type == '#' || new->type == '%') {
        long l = strtol(++options, &temp, 10);
        if (temp != options) new->val[idx].l = l;
      } else if (CFG_TOYBOX_FLOAT && new->type == '.') {
        FLOAT f = strtod(++options, &temp);
        if (temp != options) new->val[idx].f = f;
      } else error_exit("<>= only after .#%%");
      options = --temp;

    // At this point, we've hit the end of the previous option. The
    // current character is the start of a new option. If we've already
    // assigned an option to this struct, loop to allocate a new one.
    // (It'll get back here afterwards and fall through to next else.)
    } else if (new->c) {
      new = 0;
      continue;

    // Claim this option, loop to see what's after it.
    } else new->c = *options;

    options++;
  }

  // Initialize enable/disable/exclude masks and pointers to store arguments.
  // (This goes right to left so we need the whole list before we can start.)
  idx = 0;
  for (new = gof->opts; new; new = new->next) {
    unsigned long long u = 1LL<<idx++;

    if (new->c == 1 || new->c=='~') new->c = 0;
    else new->c &= 127;
    new->dex[1] = u;
    if (new->flags & 1) gof->requires |= u;
    if (new->type) {
      new->arg = (void *)nextarg;
      *(nextarg++) = new->val[2].l;
    }
  }

  // Parse trailing group indicators
  while (*options) {
    unsigned long long bits = 0;

    if (CFG_TOYBOX_DEBUG && *options != '[') error_exit("trailing %s", options);

    idx = stridx("-+!", *++options);
    if (CFG_TOYBOX_DEBUG && idx == -1) error_exit("[ needs +-!");
    if (CFG_TOYBOX_DEBUG && (options[1] == ']' || !options[1]))
      error_exit("empty []");

    // Don't advance past ] but do process it once in loop.
    while (*options++ != ']') {
      struct opts *opt;
      long long ll;

      if (CFG_TOYBOX_DEBUG && !*options) error_exit("[ without ]");
      // Find this option flag (in previously parsed struct opt)
      for (ll = 1, opt = gof->opts; ; ll <<= 1, opt = opt->next) {
        if (*options == ']') {
          if (!opt) break;
          if (bits&ll) opt->dex[idx] |= bits&~ll;
        } else {
          if (*options==1) break;
          if (CFG_TOYBOX_DEBUG && !opt)
            error_exit("[] unknown target %c", *options);
          if (opt->c == (127&*options)) {
            bits |= ll;
            break;
          }
        }
      }
    }
  }

  return rc;
}

// Fill out toys.optflags, toys.optargs, and this[] from toys.argv

void get_optflags(void)
{
  struct getoptflagstate gof;
  struct opts *catch;
  unsigned long long saveflags;
  char *letters[]={"s",""}, *ss;

  // Option parsing is a two stage process: parse the option string into
  // a struct opts list, then use that list to process argv[];

  toys.exitval = toys.which->flags >> 24;

  // Allocate memory for optargs
  saveflags = toys.optc = parse_optflaglist(&gof);
  while (toys.argv[saveflags++]);
  toys.optargs = xzalloc(sizeof(char *)*saveflags);
  if (toys.optc) *toys.optargs = *toys.argv;

  if (toys.argv[1] && toys.argv[1][0] == '-') gof.nodash_now = 0;

  // Iterate through command line arguments, skipping argv[0]
  for (gof.argc=1; toys.argv[gof.argc]; gof.argc++) {
    gof.arg = toys.argv[gof.argc];
    catch = 0;

    // Parse this argument
    if (gof.stopearly>1) goto notflag;

    if (gof.argc>1 || *gof.arg=='-') gof.nodash_now = 0;

    // Various things with dashes
    if (*gof.arg == '-') {

      // Handle -
      if (!gof.arg[1]) goto notflag;
      gof.arg++;
      if (*gof.arg=='-') {
        struct longopts *lo;
        struct arg_list *al = 0, *al2;
        int ii;

        gof.arg++;
        // Handle --
        if (!*gof.arg) {
          gof.stopearly += 2;
          continue;
        }

        // unambiguously match the start of a known --longopt?
        check_help(toys.argv+gof.argc);
        for (lo = gof.longopts; lo; lo = lo->next) {
          for (ii = 0; ii<lo->len; ii++) if (gof.arg[ii] != lo->str[ii]) break;

          // = only terminates when we can take an argument, not type 0 or '@'
          if (!gof.arg[ii] || (gof.arg[ii]=='=' && !strchr("@", lo->opt->type)))
          {
            al2 = xmalloc(sizeof(struct arg_list));
            al2->next = al;
            al2->arg = (void *)lo;
            al = al2;

            // Exact match is unambigous even when longer options available
            if (ii==lo->len) {
              llist_traverse(al, free);
              al = 0;

              break;
            }
          }
        }
        // How many matches?
        if (al) {
          *libbuf = 0;
          if (al->next) for (ss = libbuf, al2 = al; al2; al2 = al2->next) {
            lo = (void *)al2->arg;
            ss += sprintf(ss, " %.*s"+(al2==al), lo->len, lo->str);
          } else lo = (void *)al->arg;
          llist_traverse(al, free);
          if (*libbuf) error_exit("bad --%s (%s)", gof.arg, libbuf);
        }

        // One unambiguous match?
        if (lo) {
          catch = lo->opt;
          while (!strchr("=", *gof.arg)) gof.arg++;
        // Should we handle this --longopt as a non-option argument?
        } else if (gof.noerror) {
          gof.arg -= 2;
          goto notflag;
        }

        // Long option parsed, handle option.
        gotflag(&gof, catch, 1);
        continue;
      }

    // Handle things that don't start with a dash.
    } else {
      if (gof.nodash_now) toys.optflags |= FLAGS_NODASH;
      else goto notflag;
    }

    // At this point, we have the args part of -args. Loop through
    // each entry (could be -abc meaning -a -b -c)
    saveflags = toys.optflags;
    while (gof.arg && *gof.arg) {

      // Identify next option char.
      for (catch = gof.opts; catch; catch = catch->next)
        if (*gof.arg == catch->c)
          if (!gof.arg[1] || (catch->flags&(4|8))!=4) break;

      if (!catch && gof.noerror) {
        toys.optflags = saveflags;
        gof.arg = toys.argv[gof.argc];
        goto notflag;
      }

      // Handle option char (advancing past what was used)
      gotflag(&gof, catch, 0);
    }
    continue;

    // Not a flag, save value in toys.optargs[]
notflag:
    if (gof.stopearly) gof.stopearly++;
    toys.optargs[toys.optc++] = toys.argv[gof.argc];
  }

  // Sanity check
  if (toys.optc<gof.minargs)
    help_exit("Need%s %d argument%s", letters[!!(gof.minargs-1)],
      gof.minargs, letters[!(gof.minargs-1)]);
  if (toys.optc>gof.maxargs)
    help_exit("Max %d argument%s", gof.maxargs, letters[!(gof.maxargs-1)]);
  if (gof.requires && !(gof.requires & toys.optflags)) {
    struct opts *req;
    char needs[32], *s = needs;

    for (req = gof.opts; req; req = req->next)
      if (req->flags & 1) *(s++) = req->c;
    *s = 0;

    help_exit("Needs %s-%s", s[1] ? "one of " : "", needs);
  }

  toys.exitval = 0;

  if (CFG_TOYBOX_FREE) {
    llist_traverse(gof.opts, free);
    llist_traverse(gof.longopts, free);
  }
}
