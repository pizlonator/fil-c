/* man.c - Read system documentation
 *
 * Copyright 2019 makepost <makepost@firemail.cc>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/man.html

USE_MAN(NEWTOY(man, "k:M:", TOYFLAG_USR|TOYFLAG_BIN))

config MAN
  bool "man"
  default n
  help
    usage: man [-M PATH] [-k STRING] | [SECTION] COMMAND

    Read manual page for system command.

    -k	List pages with STRING in their short description
    -M	Override $MANPATH

    Man pages are divided into 8 sections:
    1 commands      2 system calls  3 library functions  4 /dev files
    5 file formats  6 games         7 miscellaneous      8 system management

    Sections are searched in the order 1 8 3 2 5 4 6 7 unless you specify a
    section. Each section has a page called "intro", and there's a global
    introduction under "man-pages".
*/

#define FOR_man
#include <toys.h>

GLOBALS(
  char *M, *k;

  char any, cell, ex, *f, k_done, *line, *m, **sct, **scts, **sufs;
  regex_t reg;
)

static void newln()
{
  if (FLAG(k)) return;
  if (TT.any) putchar('\n');
  if (TT.any && TT.cell != 2) putchar('\n'); // gawk alias
  TT.any = TT.cell = 0;
}

static void put(char *x)
{
  while (*x && (TT.ex || *x != '\n')) TT.any = putchar(*x++);
}

// Substitute with same length or shorter.
static void s(char *x, char *y)
{
  int i = strlen(x), j = strlen(y), k, l;

  for (k = 0; TT.line[k]; k++) if (!strncmp(x, &TT.line[k], i)) {
    memmove(&TT.line[k], y, j);
    for (l = k += j; TT.line[l]; l++) TT.line[l] = TT.line[l + i - j];
    k--;
  }
}

static char start(char *x)
{
  return !strncmp(x, TT.line, strlen(x));
}

static void trim(char *x)
{
  if (start(x)) while (*x++) TT.line++;
}

static char k(char *s) {
  TT.k_done = 2;
  if (s) TT.line = s;
  return !regexec(&TT.reg, TT.k, 0, 0, 0)||!regexec(&TT.reg, TT.line, 0, 0, 0);
}

static void do_man(char **pline, long len)
{
  if (!pline) return newln();
  TT.line = *pline;

  if (FLAG(k)) {
    if (!TT.k_done && !start(".") && !start("'") && k(strstr(*pline, "- ")))
      printf("%-20s %s%s", TT.k, "- "+2*(TT.line!=*pline), TT.line);
    else if (!TT.k_done && start(".so") && k(basename(*pline + 4)))
      printf("%s - See %s", TT.k, TT.line);
  } else {
    s("\\fB", ""), s("\\fI", ""), s("\\fP", ""), s("\\fR", ""); // bash bold,ita
    s("\\(aq", "'"), s("\\(cq", "'"), s("\\(dq", "\""); // bash,rsync quote
    s("\\*(lq", "\""), s("\\*(rq", "\""); // gawk quote
    s("\\(bu", "*"), s("\\(bv", "|"); // bash symbol
    s("\\&", ""), s("\\f(CW", ""); // gawk,rsync fancy
    s("\\-", "-"), s("\\(", ""), s("\\^", ""), s("\\e", "\\"); // bash escape
    s("\\*(", "#"); // gawk var

    if (start(".BR")) trim(".BR "), s(" ", ""); // bash boldpunct
    if (start(".IP")) newln(), trim(".IP "); // bash list
    if (start(".IR")) trim(".IR "), s(" ", ""); // bash itapunct

    trim(".B "); // bash bold
    trim(".BI "); // gawk boldita
    trim(".FN "); // bash filename
    trim(".I "); // bash ita
    trim(".if n "); // bash nroff
    if (start(".E")) TT.ex = TT.line[2] == 'X'; // stat example
    else if (start(".PP")) newln(); // bash paragraph
    else if (start(".SM")); // bash small
    else if (start(".S")) newln(), put(TT.line + 4), newln(); // bash section
    else if (start(".so")) put("See "), put(basename(TT.line + 4)); // lastb
    else if (start(".TH")) s("\"", " "), put(TT.line + 4); // gawk,git head
    else if (start(".TP")) newln(), TT.cell = 1; // bash table
    else if (start(".") || start("\'")); // bash,git garbage
    else if (!*TT.line); // emerge
    else {
      if (TT.cell) TT.cell++;
      if (!TT.ex) put(" ");
      put(TT.line);
    }
  }
}

// Open file, decompressing if suffix known.
static int zopen(char *s)
{
  int fds[] = {-1, -1};
  char **known = TT.sufs, *suf = strrchr(s, '.');

  if ((*fds = open(s, O_RDONLY)) == -1) return -1;
  while (suf && *known && strcmp(suf, *known++));
  if (!suf || !*known) return *fds;
  sprintf(toybuf, "%czcat"+2*(suf[1]=='g'), suf[1]);
  xpopen_both((char *[]){toybuf, s, 0}, fds);
  close(fds[0]);
  return fds[1];
}

static char manpath()
{
  if (*++TT.sct) return 0;
  if (!(TT.m = strsep(&TT.M, ":"))) return 1;
  TT.sct = TT.scts;
  return 0;
}

// Try opening all the possible file extensions.
static int tryfile(char *name)
{
  int dotnum, fd = -1;
  char *s = xmprintf("%s/man%s/%s.%s.bz2", TT.m, *TT.sct, name, *TT.sct), **suf;
  size_t len = strlen(s) - 4;

  for (dotnum = 0; dotnum <= 2; dotnum += 2) {
    suf = TT.sufs;
    while ((fd == -1) && *suf) strcpy(s + len - dotnum, *suf++), fd = zopen(s);
    // Recheck suf in zopen, because for x.1.gz name here it is "".
  }
  free(s);
  return fd;
}

void man_main(void)
{
  int fd = -1;
  TT.scts = (char *[]) {"1", "8", "3", "2", "5", "4", "6", "7", 0};
  TT.sct = TT.scts - 1; // First manpath() read increments.
  TT.sufs = (char *[]) {".bz2", ".gz", ".xz", "", 0};

  if (!TT.M) TT.M = getenv("MANPATH");
  if (!TT.M) TT.M = "/usr/share/man";

  if (FLAG(k)) {
    char *d, *f;
    DIR *dp;
    struct dirent *entry;

    xregcomp(&TT.reg, TT.k, REG_ICASE|REG_NOSUB);
    while (!manpath()) {
      d = xmprintf("%s/man%s", TT.m, *TT.sct);
      if (!(dp = opendir(d))) continue;
      while ((entry = readdir(dp))) {
        if (entry->d_name[0] == '.') continue;
        f = xmprintf("%s/%s", d, TT.k = entry->d_name);
        if (-1 != (fd = zopen(f))) {
          TT.k_done = 0;
          do_lines(fd, '\n', do_man);
        }
        free(f);
      }
      closedir(dp);
      free(d);
    }
    return regfree(&TT.reg);
  }

  if (!toys.optc) help_exit("which page?");

  if (toys.optc == 1) {
    if (strchr(*toys.optargs, '/')) fd = zopen(*toys.optargs);
    else while ((fd == -1) && !manpath()) fd = tryfile(*toys.optargs);
    if (fd == -1) error_exit("no %s", *toys.optargs);

  // If they specified a section, look for file in that section
  } else {
    TT.scts = (char *[]){*toys.optargs, 0}, TT.sct = TT.scts - 1;
    while ((fd == -1) && !manpath()) fd = tryfile(toys.optargs[1]);
    if (fd == -1) error_exit("section %s no %s", *--TT.sct, toys.optargs[1]);
  }

  do_lines(fd, '\n', do_man);
}
