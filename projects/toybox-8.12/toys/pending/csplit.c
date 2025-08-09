/* csplit.c - split files on context
 *
 * Copyright 2023 Oliver Webb <aquahobbyist@proton.me>
 *
 * See https://pubs.opengroup.org/onlinepubs/9699919799/utilities/csplit.html
 *
 * Deviations From POSIX: Add "{*}", file size is %ld, no negative offsets

USE_CSPLIT(NEWTOY(csplit, "<2skf:n#", TOYFLAG_USR|TOYFLAG_BIN))

config CSPLIT
  bool "csplit"
  default n
  help
    usage: csplit [-ks] [-f PREFIX] [-n INTEGER] file arg...

    Split files into multiple files based on list of rules

    -k	Does not delete Files on error
    -s	No file output size messages
    -f [PREFIX] Use [PREFIX] as filename prefix instead of "xx"
    -n [INTEGER] Make all filename numbers [INTEGER] characters long

    Valid Rules:
    /regexp/[INTEGER] Break file before line that regexp matches,
    %regexp%[INTEGER] Exclude untill line matches regexp
    If a offset is specified for these rules, the break will happen [INTEGER]
    lines after the regexp match
    if a offset is specified, it will break at [INTEGER] lines after the offset
    [INTEGER] Break file at line before [INTEGER]
    {INTEGER} Repeat Previous Pattern INTEGER Number of times if INTEGER is *
    The pattern repeats forever
*/

#define FOR_csplit
#include "toys.h"

GLOBALS(
  long n;
  char *f;

  size_t indx, findx, lineno;
  char *filefmt, *prefix;
  // Variables the context checker need to track between lines
  size_t btc, tmp;
  int offset, withld, inf;
)

static _Noreturn void abrt(char *err)
{
  // Cycle down through index instead of keeping track of what files we made
  if (!FLAG(k)) for (; TT.indx>=1; TT.indx--)
    remove(xmprintf(TT.filefmt, TT.prefix, TT.findx));

  error_exit("%s\n", err);
}

static int rgmatch(char *rxrl, char *line, char *fmt)
{
  regex_t rxp;
  int rr;

  sscanf(rxrl, fmt, toybuf, &TT.offset);
  xregcomp(&rxp, toybuf, 0);
  rr = regexec(&rxp, line, 0, 0, 0);
  if (!rr) return 1;
  else if (rr == REG_NOMATCH) return 0;
  abrt("bad regex");
}

static int cntxt(char *line, char *rule)
{
  size_t llv;
  if (TT.indx == toys.optc) return 0;

  if (TT.offset < 0);
  else if (TT.offset == 0) {
    TT.offset = -1;

    return 1;
  } else {
    TT.offset--;

    return 0;
  }

  switch (rule[0]) {
    case '/':
      return rgmatch(rule, line, "/%[^/%]/%d");
      break;

    case '%':
      TT.withld = 1;
      return rgmatch(rule, line, "%%%[^/%]%%%d");

    case '{':
      if (TT.indx < 2) abrt("bad rule order");

      if (!strcmp(rule,"{*}")) {
        TT.btc = -1;
        TT.inf = 1;
      } else if (!sscanf(rule,"{%lu}",&TT.btc)) abrt("bad rule");

      if (TT.tmp == -1) TT.tmp = TT.lineno;
      if ((llv = atoll(toys.optargs[TT.indx-1]))) {
        if (((TT.lineno-TT.tmp) % llv+1) == llv) {
          TT.tmp = -1;
          TT.indx--;

          return 1;
        } else return 0;
      }

      if (cntxt(line, toys.optargs[TT.indx-1])) {
        // Manipulate the rule then return to it later so we create a
        // new file but are still on the same rule. This is the only
        // reason why we differentiate between rule and file Index
        if (TT.btc != 1) {
          toys.optargs[TT.indx] = xmprintf("{%lu}",TT.btc-1);
          TT.indx--;
        }
        return 1;
      }
      return 0;

    default:
      if (TT.lineno > atoll(rule)) abrt("bad rule order");
      else if (!(atoll(rule))) abrt("bad rule");
      else {
        if (TT.lineno == atoll(rule)) TT.offset++;
        return 0;
      }
  }
}

void csplit_main(void)
{
  FILE *actvfile;
  FILE *fin = (*toys.optargs[0] != '-') ? xfopen(toys.optargs[0], "r") : stdin;
  char *line;
  size_t filesize = 0;

  TT.indx = TT.lineno = 1;
  TT.tmp = TT.offset = -1;

  // -f and -n formatting
  TT.filefmt = xmprintf("%%s%%0%lud", TT.n ? TT.n : 2);
  TT.prefix = TT.f ? TT.f : "xx";

  actvfile = xfopen(xmprintf(TT.filefmt, TT.prefix, TT.findx), "w+");
  for (; (line = xgetline(fin)); free(line)) {
    TT.lineno++;
    if (!TT.withld) filesize += strlen(line)+1;

    if (cntxt(line, toys.optargs[TT.indx])) {
      if (!TT.withld) {
        fclose(actvfile);
        if (!FLAG(s)) printf("%ld\n", filesize);
        filesize = 0;
        TT.findx++;
        actvfile = xfopen(xmprintf(TT.filefmt, TT.prefix, TT.findx), "w+");
      }

      TT.indx++;
      TT.withld = 0;
    }
    if (!TT.withld) fprintf(actvfile, "%s\n", line);
  }
  if (!FLAG(s)) printf("%ld\n", filesize);

  // Abort Case: Not All Rules Processed
  if (!((TT.indx == toys.optc) || TT.inf)) abrt("Rules not processed");
}
