/* printf.c - Format and Print the data.
 *
 * Copyright 2014 Sandeep Sharma <sandeep.jack2756@gmail.com>
 * Copyright 2014 Kyungwan Han <asura321@gmail.com>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/printf.html
 *
 * TODO: *m$ ala printf("%1$d:%2$.*3$d:%4$.*3$d\n", hour, min, precision, sec);

USE_PRINTF(NEWTOY(printf, "<1?^", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_MAYFORK))

config PRINTF
  bool "printf"
  default y
  help
    usage: printf FORMAT [ARGUMENT...]

    Format and print ARGUMENT(s) according to FORMAT, using C printf syntax
    (% escapes for cdeEfgGiosuxX, \ escapes for abefnrtv0 or \OCTAL or \xHEX).
*/

#define FOR_printf
#include "toys.h"

// Detect matching character (return true/false) and advance pointer if match.
static int chrstart(char **s, char c)
{
  int x = (**s == c);

  if (x) ++*s;

  return x;
}

// Parse escape sequences.
static int handle_slash(char **esc_val, int posix)
{
  char *ptr = *esc_val;
  int len, base = 0;
  unsigned result = 0, num;

  if (*ptr == 'c') xexit();

  // 0x12 hex escapes have 1-2 digits, \123 octal escapes have 1-3 digits.
  if (chrstart(&ptr, 'x')) base = 16;
  else {
    if (posix && *ptr=='0') ptr++;
    if (*ptr >= '0' && *ptr <= '7') base = 8;
    else if (ptr != *esc_val) goto done;
  }
  len = (char []){0,3,2}[base/8];

  // Not a hex or octal escape? (This catches trailing \)
  if (!len) {
    if (!(result = unescape(*ptr))) result = '\\';
    else ++*esc_val;

    return result;
  }

  while (len) {
    num = tolower(*ptr) - '0';
    if (num >= 'a'-'0') num += '0'-'a'+10;
    if (num >= base) {
      // "\xav" is "\xa"+"v", but "\xva" is an error.
      if (base == 16 && len == 2) error_exit("bad \\x");
      break;
    }
    result = (result*base)+num;
    ptr++;
    len--;
  }
done:
  *esc_val = ptr;

  return result;
}

void printf_main(void)
{
  char **arg = toys.optargs+1;

  // Repeat format until arguments consumed
  for (;;) {
    int seen = 0;
    char *f = *toys.optargs;

    // Loop through characters in format
    while (*f) {
      if (chrstart(&f, '\\')) putchar(handle_slash(&f, 0));
      else if (!chrstart(&f, '%') || *f == '%') putchar(*f++);

      // Handle %escape
      else {
        char c, *end = 0, *aa, *to = toybuf;
        int wp[] = {0,-1}, i = 0;

        // Parse width.precision between % and type indicator.
        *to++ = '%';
        while (strchr("-+# '0", *f) && (to-toybuf)<10) *to++ = *f++;
        for (;;) {
          if (chrstart(&f, '*')) {
            if (*arg) wp[i] = atolx(*arg++);
          } else while (*f >= '0' && *f <= '9') wp[i] = (wp[i]*10)+(*f++)-'0';
          if (i++ || !chrstart(&f, '.')) break;
          wp[1] = 0;
        }
        c = *f++;
        seen = sprintf(to, "*.*%c", c);;
        errno = 0;
        aa = *arg ? *arg++ : "";

        // Output %esc using parsed format string
        if (c == 'b') {
          while (*aa)
            putchar(chrstart(&aa, '\\') ? handle_slash(&aa, 1) : *aa++);

          continue;
        } else if (c == 'c') printf(toybuf, wp[0], wp[1], *aa);
        else if (c == 's') printf(toybuf, wp[0], wp[1], aa);
        else if (strchr("diouxX", c)) {
          long long ll;

          if (*aa == '\'' || *aa == '"') ll = aa[1];
          else ll = strtoll(aa, &end, 0);

          sprintf(to, "*.*ll%c", c);
          printf(toybuf, wp[0], wp[1], ll);
        } else if (strchr("feEgG", c)) {
          long double ld = strtold(aa, &end);

          sprintf(to, "*.*L%c", c);
          printf(toybuf, wp[0], wp[1], ld);
        } else error_exit("bad %%%c@%ld", c, (long)(f-*toys.optargs));

        if (end && (*end || errno==ERANGE)) perror_msg("bad %%%c %s", c, aa);
      }
    }

    // Posix says to keep looping through format until we consume all args.
    // This only works if the format actually consumed at least one arg.
    if (!seen || !*arg) break;
  }
}
