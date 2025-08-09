/* tr.c - translate or delete characters
 *
 * Copyright 2014 Sandeep Sharma <sandeep.jack2756@gmail.com>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/tr.html
 * TODO: -a (ascii)

USE_TR(NEWTOY(tr, "^<1>2Ccstd[+cC]", TOYFLAG_USR|TOYFLAG_BIN))

config TR
  bool "tr"
  default n
  help
    usage: tr [-cdst] SET1 [SET2]

    Translate, squeeze, or delete characters from stdin, writing to stdout

    -c/-C  Take complement of SET1
    -d     Delete input characters coded SET1
    -s     Squeeze multiple output characters of SET2 into one character
    -t     Truncate SET1 to length of SET2
*/

#define FOR_tr
#include "toys.h"

GLOBALS(
  short *map;
  int len1, len2;
)

enum {
  class_alpha, class_alnum, class_digit,
  class_lower,class_upper,class_space,class_blank,
  class_punct,class_cntrl,class_xdigit,class_invalid
};

static void map_translation(char *set1 , char *set2)
{
  int i = TT.len1, k = 0;

  if (FLAG(d))
    for (; i; i--, k++) TT.map[set1[k]] = set1[k]|0x100; //set delete bit

  if (FLAG(s)) {
    for (i = TT.len1, k = 0; i; i--, k++)
      TT.map[set1[k]] = TT.map[set1[k]]|0x200;
    for (i = TT.len2, k = 0; i; i--, k++)
      TT.map[set2[k]] = TT.map[set2[k]]|0x200;
  }
  i = k = 0;
  while (!FLAG(d) && set2 && TT.len1--) { //ignore set2 if -d present
    TT.map[set1[i]] = ((TT.map[set1[i]] & 0xFF00) | set2[k]);
    if (set2[k + 1]) k++;
    i++;
  }
}

static int handle_escape_char(char **esc_val) //taken from printf
{
  char *ptr = *esc_val;
  int esc_length = 0;
  unsigned  base = 0, num = 0, result = 0, count = 0;

  if (*ptr == 'x') {
    ptr++;
    esc_length++;
    base = 16;
  } else if (isdigit(*ptr)) base = 8;

  while (esc_length < 3 && base) {
    num = tolower(*ptr) - '0';
    if (num > 10) num += ('0' - 'a' + 10);
    if (num >= base) {
      if (base == 16) {
        esc_length--;
        if (!esc_length) {// Invalid hex value eg. /xvd, print as it is /xvd
          result = '\\';
          ptr--;
        }
      }
      break;
    }
    esc_length++;
    result = (char)(count = (count * base) + num);
    ptr++;
  }
  if (base) ptr--;
  else if (!(result = unescape(*ptr))) {
    result = '\\';
    ptr--;
  }
  *esc_val = ptr;
  return result;
}

static int find_class(char *class_name)
{
  int i;
  static char *class[] = {
    "[:alpha:]","[:alnum:]","[:digit:]", "[:lower:]","[:upper:]","[:space:]",
    "[:blank:]","[:punct:]","[:cntrl:]", "[:xdigit:]"
  };

  for (i = 0; i != class_invalid; i++)
    if (!memcmp(class_name, class[i], 9+(*class_name == 'x'))) break;

  return i;
}

static char *expand_set(char *arg, int *len, size_t until)
{
  int i = 0, j, k, size = 256;
  char *set = xzalloc(size), *orig = arg;

  while (*arg) {
    if (arg-orig >= until) break;
    if (i >= size) {
      size += 256;
      set = xrealloc(set, size);
    }
    if (*arg == '\\') {
      arg++;
      set[i++] = handle_escape_char(&arg);
      arg++;
      continue;
    }
    if (arg[1] == '-') {
      if (!arg[2]) goto save;
      j = *arg;
      k = arg[2];
      if (j > k) perror_exit("reverse colating order");
      while (j <= k) set[i++] = j++;
      arg += 3;
      continue;
    }
    if (*arg == '[' && arg[1] == ':') {

      if ((j = find_class(arg)) == class_invalid) goto save;

      if ((j == class_alpha) || (j == class_upper) || (j == class_alnum))
        for (k = 'A'; k <= 'Z'; k++) set[i++] = k;
      if ((j == class_alpha) || (j == class_lower) || (j == class_alnum))
        for (k = 'a'; k <= 'z'; k++) set[i++] = k;
      if ((j == class_alnum) || (j == class_digit) || (j == class_xdigit))
        for (k = '0'; k <= '9'; k++) set[i++] = k;
      if (j == class_space || j == class_blank) {
        set[i++] = '\t';
        if (j == class_space) {
          set[i++] = '\n';
          set[i++] = '\f';
          set[i++] = '\r';
          set[i++] = '\v';
        }
        set[i++] = ' ';
      }
      if (j == class_punct)
        for (k = 0; k <= 255; k++) if (ispunct(k)) set[i++] = k;
      if (j == class_cntrl)
        for (k = 0; k <= 255; k++) if (iscntrl(k)) set[i++] = k;
      if (j == class_xdigit) {
        for (k = 'A'; k <= 'F'; k++) {
          set[i + 6] = k | 0x20;
          set[i++] = k;
        }
        i += 6;
        arg += 10;
        continue;
      }

      arg += 9; //never here for class_xdigit.
      continue;
    }
    if (*arg == '[' && arg[1] == '=') { //[=char=] only
      arg += 2;
      if (*arg) set[i++] = *arg;
      if (!arg[1] || arg[1] != '=' || arg[2] != ']')
        error_exit("bad equiv class");
      continue;
    }
save:
    set[i++] = *arg++;
  }
  *len = i;
  return set;
}

static void print_map(char *set1, char *set2)
{
  int n, ch, src, dst, prev = -1;

  while ((n = read(0, toybuf, sizeof(toybuf)))) {
    if (!FLAG(d) && !FLAG(s))
      for (dst = 0; dst < n; dst++) toybuf[dst] = TT.map[toybuf[dst]];
    else for (src = dst = 0; src < n; src++) {
      ch = TT.map[toybuf[src]];
      if (FLAG(d) && (ch & 0x100)) continue;
      if (FLAG(s) && ((ch & 0x200) && prev == ch)) continue;
      toybuf[dst++] = prev = ch;
    }
    xwrite(1, toybuf, dst);
  }
}

static void do_complement(char **set)
{
  int i = 0, j = 0;
  char *comp = xmalloc(256);

  for (; i < 256; i++) {
    if (memchr(*set, i, TT.len1)) continue;
    else comp[j++] = (char)i;
  }
  free(*set);
  TT.len1 = j;
  *set = comp;
}

void tr_main(void)
{
  char *set1, *set2 = NULL;
  int i = 0;

  TT.map = xmalloc(256*sizeof(*TT.map));
  for (; i < 256; i++) TT.map[i] = i; //init map

  set1 = expand_set(*toys.optargs, &TT.len1,
      (FLAG(t) && toys.optargs[1]) ? strlen(toys.optargs[1]) : -1);
  if (FLAG(c)) do_complement(&set1);
  if (toys.optargs[1]) {
    if (!*toys.optargs[1]) error_exit("set2 can't be empty string");
    set2 = expand_set(toys.optargs[1], &TT.len2, -1);
  }
  map_translation(set1, set2);

  print_map(set1, set2);
  free(set1);
  free(set2);
}
