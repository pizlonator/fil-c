/* sort.c - put input lines into order
 *
 * Copyright 2004, 2008 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/007904975/utilities/sort.html
 *
 * Deviations from POSIX: Lots.
 * We invented -x

USE_SORT(NEWTOY(sort, USE_SORT_FLOAT("g")"S:T:m" "o:k*t:" "xVbMCcszdfirun", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_ARGFAIL(2)))

config SORT
  bool "sort"
  default y
  help
    usage: sort [-runbCcdfiMsxVz] [FILE...] [-k#[,#[x]] [-t X]] [-o FILE]

    Sort all lines of text from input files (or stdin) to stdout.

    -r	Reverse
    -u	Unique lines only
    -n	Numeric order (instead of alphabetical)

    -b	Ignore leading blanks (or trailing blanks in second part of key)
    -C	Check whether input is sorted
    -c	Warn if input is unsorted
    -d	Dictionary order (use alphanumeric and whitespace chars only)
    -f	Force uppercase (case insensitive sort)
    -i	Ignore nonprinting characters
    -k	Sort by "key" (see below)
    -M	Month sort (jan, feb, etc)
    -o	Output to FILE instead of stdout
    -s	Skip fallback sort (only sort with keys)
    -t	Use a key separator other than whitespace
    -x	Hexadecimal numerical sort
    -V	Version numbers (name-1.234-rc6.5b.tgz)
    -z	Zero (null) terminated lines

    Sorting by key looks at a subset of the words on each line. -k2 uses the
    second word to the end of the line, -k2,2 looks at only the second word,
    -k2,4 looks from the start of the second to the end of the fourth word.
    -k2.4,5 starts from the fourth character of the second word, to the end
    of the fifth word. Negative values count from the end. Specifying multiple
    keys uses the later keys as tie breakers, in order. A type specifier
    appended to a sort key (such as -2,2n) applies only to sorting that key.

config SORT_FLOAT
  bool
  default y
  depends on TOYBOX_FLOAT
  help
    usage: sort [-g]

    -g	General numeric sort (double precision with nan and inf)
*/

#define FOR_sort
#include "toys.h"

GLOBALS(
  char *t;
  struct arg_list *k;
  char *o, *T, S;

  void *key_list;
  unsigned linecount;
  char **lines, *name;
)

// The sort types are n, g, and M.
// u, c, s, and z apply to top level only, not to keys.
// b at top level implies bb.
// The remaining options can be applied to search keys.

#define FLAG_bb (1<<31)  // Ignore trailing blanks

struct sort_key {
  struct sort_key *next_key;  // linked list
  long range[4];              // start word, start char, end word, end char
  int flags;
};

static int skip_key(char *str)
{
  int end = 0;

  // Skip leading blanks
  if (str[end] && !TT.t) while (isspace(str[end])) end++;

  // Skip body of key
  for (; str[end]; end++) {
    if (TT.t) {
      if (str[end]==*TT.t) {
        end++;
        break;
      }
    } else if (isspace(str[end])) break;
  }

  return end;
}

// Copy of the part of this string corresponding to a key/flags.

static char *get_key_data(char *str, struct sort_key *key, int flags)
{
  long start = 0, end, len, h, i, j, k;

  // Special case whole string, so we don't have to make a copy
  if(key->range[0]==1 && !key->range[1] && !key->range[2] && !key->range[3]
    && !(flags&(FLAG_b|FLAG_d|FLAG_i|FLAG_bb))) return str;

  // Find start of key on first pass, end on second pass
  len = strlen(str);
  for (j=0; j<2; j++) {
    if (!(k = key->range[2*j])) end=len;

    // Loop through fields
    else {
      if (k<1) for (end = h = 0;; end += h) {
        ++k;
        if (!(h = skip_key(str+end))) break;
      }
      if (k<1) end = len*!j;
      else for (end = 0, i = 1; i<k+j; i++) end += skip_key(str+end);
    }
    if (!j) start = end;
  }

  // Key with explicit separator starts after the separator
  if (TT.t && str[start]==*TT.t) start++;

  // Strip leading and trailing whitespace if necessary
  if ((flags&FLAG_b) || (!TT.t && !key->range[3]))
    while (isspace(str[start])) start++;
  if (flags&FLAG_bb) while (end>start && isspace(str[end-1])) end--;

  // Handle offsets on start and end
  if (key->range[3]>0) {
    end += key->range[3]-1;
    if (end>len) end=len;
  }
  if (key->range[1]>0) {
    start += key->range[1]-1;
    if (start>len) start=len;
  }

  // Make the copy
  if (end<start) end = start;
  str = xstrndup(str+start, end-start);

  // Handle -d
  if (flags&FLAG_d) {
    for (start = end = 0; str[end]; end++)
      if (isspace(str[end]) || isalnum(str[end])) str[start++] = str[end];
    str[start] = 0;
  }

  // Handle -i
  if (flags&FLAG_i) {
    for (start = end = 0; str[end]; end++)
      if (isprint(str[end])) str[start++] = str[end];
    str[start] = 0;
  }

  return str;
}

// append a sort_key to key_list.

static struct sort_key *add_key(void)
{
  struct sort_key **pkey = (struct sort_key **)&TT.key_list;

  while (*pkey) pkey = &((*pkey)->next_key);
  return *pkey = xzalloc(sizeof(struct sort_key));
}

// Perform actual comparison
static int compare_values(int flags, char *x, char *y)
{
  if (CFG_SORT_FLOAT && (flags & FLAG_g)) {
    char *xx,*yy;
    double dx = strtod(x,&xx), dy = strtod(y,&yy);
    int xinf, yinf;

    // not numbers < NaN < -infinity < numbers < +infinity

    if (x==xx) return y==yy ? 0 : -1;
    if (y==yy) return 1;

    // Check for isnan
    if (dx!=dx) return (dy!=dy) ? 0 : -1;
    if (dy!=dy) return 1;

    // Check for infinity.  (Could underflow, but avoids needing libm.)
    xinf = (1.0/dx == 0.0);
    yinf = (1.0/dy == 0.0);
    if (xinf) {
      if(dx<0) return (yinf && dy<0) ? 0 : -1;
      return (yinf && dy>0) ? 0 : 1;
    }
    if (yinf) return dy<0 ? 1 : -1;

    return dx<dy ? -1 : dx>dy;
  } else if (flags & FLAG_M) {
    struct tm thyme;
    int dx;
    char *xx,*yy;

    xx = strptime(x,"%b",&thyme);
    dx = thyme.tm_mon;
    yy = strptime(y,"%b",&thyme);
    if (!xx) return !yy ? 0 : -1;
    else if (!yy) return 1;
    else return dx==thyme.tm_mon ? 0 : dx-thyme.tm_mon;

  } else if (flags & FLAG_x) return strtol(x, NULL, 16)-strtol(y, NULL, 16);
  else if (flags & FLAG_V) {
    while (*x && *y) {
      while (*x && *x == *y) x++, y++;
      if (isdigit(*x) && isdigit(*y)) {
        long long xx = strtoll(x, &x, 10), yy = strtoll(y, &y, 10);

        if (xx<yy) return -1;
        if (xx>yy) return 1;
      } else {
        char xx = *x ? *x : x[-1], yy = *y ? *y : y[-1];

        // -rc/-pre hack so abc-123 > abc-123-rc1 (other way already - < 0-9)
        if (xx != yy) {
          if (xx<yy && !strstart(&y, "-rc") && !strstart(&y, "-pre")) return -1;
          else return 1;
        }
      }
    }
    return *x ? !!*y : -1;
  // This is actually an integer sort with decimals sorted by string fallback.
  } else if (flags & FLAG_n) {
    long long dx = atoll(x), dy = atoll(y);

    return dx<dy ? -1 : dx>dy;

  // Ascii sort
  } else return ((flags&FLAG_f) ? strcasecmp : strcmp)(x, y);
}

// Callback from qsort(): Iterate through key_list and perform comparisons.
static int compare_keys(const void *xarg, const void *yarg)
{
  int flags = toys.optflags, retval = 0;
  char *x, *y, *xx = *(char **)xarg, *yy = *(char **)yarg;
  struct sort_key *key;

  for (key=(void *)TT.key_list; !retval && key; key = key->next_key) {
    flags = key->flags ? : toys.optflags;

    // Chop out and modify key chunks, handling -dfib

    x = get_key_data(xx, key, flags);
    y = get_key_data(yy, key, flags);

    retval = compare_values(flags, x, y);

    // Free the copies get_key_data() made.

    if (x != xx) free(x);
    if (y != yy) free(y);

    if (retval) break;
  }

  // Perform fallback sort if necessary (always case insensitive, no -f,
  // the point is to get a stable order even for -f sorts)
  if (!retval && !FLAG(s)) {
    flags = toys.optflags;
    retval = strcmp(xx, yy);
  }

  return retval * ((flags&FLAG_r) ? -1 : 1);
}

// Read each line from file, appending to a big array.
static void sort_lines(char **pline, long len)
{
  char *line;

  if (!pline) return;
  line = *pline;
  if (!FLAG(z) && len && line[len-1]=='\n') line[--len] = 0;
  *pline = 0;

  // handle -c here so we don't allocate more memory than necessary.
  if (FLAG(C)||FLAG(c)) {
    if (TT.lines && compare_keys((void *)&TT.lines, &line)>-FLAG(u)) {
      toys.exitval = 1;
      if (FLAG(C)) xexit();
      error_exit("%s: Check line %u", TT.name, TT.linecount+1);
    }
    free(TT.lines);
    TT.lines = (void *)line;
  } else {
    if (!(TT.linecount&63))
      TT.lines = xrealloc(TT.lines, sizeof(char *)*(TT.linecount+64));
    TT.lines[TT.linecount] = line;
  }
  TT.linecount++;
}

// Callback from loopfiles to handle input files.
static void sort_read(int fd, char *name)
{
  TT.name = name;
  do_lines(fd, '\n'*!FLAG(z), sort_lines);
}

void sort_main(void)
{
  int idx, jdx, fd = 1;

  if (FLAG(u)) toys.optflags |= FLAG_s;

  // Parse -k sort keys.
  if (TT.k) {
    struct arg_list *arg;

    for (arg = TT.k; arg; arg = arg->next) {
      struct sort_key *key = add_key();
      char *temp, *temp2, *optlist;
      int flag;

      idx = 0;
      temp = arg->arg;
      while (*temp) {
        // Start of range
        key->range[2*idx] = strtol(temp, &temp, 10);
        if (*temp=='.') key->range[(2*idx)+1] = strtol(temp+1, &temp, 10);

        // Handle flags appended to a key type.
        for (;*temp;temp++) {

          // Second comma becomes an "Unknown key" error.
          if (*temp==',' && !idx++) {
            temp++;
            break;
          }

          // Which flag is this?
          optlist = toys.which->options;
          temp2 = strchr(optlist, *temp);
          flag = 1<<(optlist-temp2+strlen(optlist)-1);

          // Was it a flag that can apply to a key?
          if (!temp2 || flag>FLAG_x || (flag&(FLAG_u|FLAG_c|FLAG_s|FLAG_z)))
            error_exit("Unknown key option.");

          // b after , means strip _trailing_ space, not leading.
          if (idx && flag==FLAG_b) flag = FLAG_bb;
          key->flags |= flag;
        }
      }
    }
  }

  // global b flag strips both leading and trailing spaces
  if (FLAG(b)) toys.optflags |= FLAG_bb;

  // If no keys, perform alphabetic sort over the whole line.
  if (!TT.key_list) add_key()->range[0] = 1;

  // Open input files and read data, populating TT.lines[TT.linecount]
  loopfiles(toys.optargs, sort_read);

  // The compare (-c) logic was handled in sort_read(),
  // so if we got here, we're done.
  if (FLAG(C)||FLAG(c)) goto exit_now;

  // Perform the actual sort
  qsort(TT.lines, TT.linecount, sizeof(char *), compare_keys);

  // handle unique (-u)
  if (FLAG(u)) {
    for (jdx=0, idx=1; idx<TT.linecount; idx++) {
      if (!compare_keys(&TT.lines[jdx], &TT.lines[idx])) free(TT.lines[idx]);
      else TT.lines[++jdx] = TT.lines[idx];
    }
    if (TT.linecount) TT.linecount = jdx+1;
  }

  // Open output file if necessary. We can't do this until we've finished
  // reading in case the output file is one of the input files.
  if (TT.o) fd = xcreate(TT.o, O_CREAT|O_TRUNC|O_WRONLY, 0666);

  // Output result
  for (idx = 0; idx<TT.linecount; idx++) {
    char *s = TT.lines[idx];
    unsigned i = strlen(s);

    if (!FLAG(z)) s[i] = '\n';
    xwrite(fd, s, i+1);
    if (CFG_TOYBOX_FREE) free(s);
  }

exit_now:
  if (CFG_TOYBOX_FREE) {
    if (fd != 1) close(fd);
    free(TT.lines);
  }
}
