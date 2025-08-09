/* tsort.c - topological sort dependency resolver
 *
 * Copyright 2023 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/tsort.html

USE_TSORT(NEWTOY(tsort, ">1", TOYFLAG_USR|TOYFLAG_BIN))

config TSORT
  bool "tsort"
  default y
  help
    usage: tsort [FILE]

    Topological sort dependency resolver.

    Read pairs of input strings indicating before/after dependency relationships
    and find an ordering that respects all dependencies. On success output each
    string once to stdout, on failure print error and output cycle pairs.
*/

#include "toys.h"

// Comparison callback for qsort and bsearch: sort by second element
static int sbse(char **a, char **b)
{
  return strcmp(a[1], b[1]);
}

// Read pairs of "A must go before B" input strings into pair list. Sort pair
// list by second element. Loop over list to find pairs where the first string
// is not any pair's second string (I.E. nothing depends on this) and remove
// each pair from list. For each removed pair, add first string to output
// list, and also add second string to output if after removing it no other
// pair has it as the second string. Suppress duplicates by checking each
// string added to output against the strings added in the last 2 passes
// through the pair list. (Because "a b c a" removes "c a" pair after checking
// "a b" pair, so removing "a b" next pass would try to output "a" again.)
// If a pass through the pair list finds no pairs to remove, what's left is
// all circular dependencies.

// TODO: this treats NUL in input as EOF
static void do_tsort(int fd, char *name)
{
  off_t plen = 0;
  char *ss, **pair, *keep[2];
  long count,    // remaining unprocessed pairs
       len,      // total strings in pair list
       out,      // most recently added output (counts down from len)
       otop,     // out at start of this loop over pair[]
       otop2,    // out at start of previous loop over pair[]
       ii, jj, kk;
  unsigned long sigh;

  // bsearch()'s first argument is the element to search for,
  // and sbse() compares the second element of each pair, so to find
  // which FIRST element matches a second element, we need to feed bsearch
  // keep-1 but the compiler clutches its pearls about this even when
  // typecast to (void *), so do the usual LP64 trick to MAKE IT SHUT UP.
  // (The search function adds 1 to each argument so we never access
  // memory outside the pair.)
  sigh = ((unsigned long)keep)-sizeof(*keep);

  // Count input entries in data block read from fd
  if (!(ss = readfd(fd, 0, &plen))) return;
  for (ii = len = 0; ii<plen; len++) {
    while (isspace(ss[ii])) ii++;
    if (ii==plen) break;
    while (ii<plen && !isspace(ss[ii])) ii++;
  }
  if (len&1) error_exit("bad input (not pairs)");

  // get dependency pair list, null terminate strings, mark depends-on-self
  pair = xmalloc(len*sizeof(char *));
  for (ii = len = 0;;) {
    while (isspace(ss[ii])) ii++;
    if (ii>=plen) break;
    pair[len] = ss+ii;
    while (ii<plen && !isspace(ss[ii])) ii++;
    if (ii<plen) ss[ii++] = 0;
    if ((len&1) && !strcmp(pair[len], pair[len-1])) pair[len] = pair[len-1];
    len++;
  }

  // sort pair list by 2nd element to binary search "does anyone depend on this"
  count = (out = otop = otop2 = len)/2;
  qsort(pair, count, sizeof(keep), (void *)sbse);

  // repeat until pair list empty or nothing added to output list.
  while (count) {
    // find/remove/process pairs no other pair depends on
    for (ii = 0; ii<count; ii++) {
      // Find pair that depends on itself or no other pair depends on first str
      memcpy(keep, pair+2*ii, sizeof(keep));
      // The compiler won't shut up about keep-1 no matter how we typecast it.
      if (keep[0]!=keep[1] && bsearch((void *)sigh, pair, count, sizeof(keep),
          (void *)sbse)) continue;

      // Remove from pair list
      memmove(pair+2*ii, pair+2*(ii+1), (--count-ii)*sizeof(keep));
      ii--;

      // Drop depends-on-self pairs that any other pair depends on
      if (keep[0]==keep[1] &&
          bsearch(keep, pair, count, sizeof(keep),(void *)sbse)) continue;

      // Process removed pair: add unique strings to output list in reverse
      // order. Output is stored in space at end of list freed up by memmove(),
      // defers output until we know there are no cycles, and zaps duplicates.
      for (kk = 0;; kk++) {
        // duplicate killer checks through previous TWO passes, because
        // "a b c a" removes "c a" pair after checking "a b" pair, so removing
        // "a b" next pass tries to output "a" again.

        for (jj = out; jj<otop2; jj++) if (!strcmp(keep[kk], pair[jj])) break;
        if (jj==otop2) pair[--out] = keep[kk];

        // Print second string too if no other pair depends on it
        if (kk || bsearch(keep, pair, count, sizeof(keep), (void *)sbse)) break;
      }
    }
    if (out == otop) break;
    otop2 = otop;
    otop = out;
  }

  // If we couldn't empty the list there's a cycle
  if (count) {
    error_msg("cycle pairs");
    while (count--) xprintf("%s %s\n", pair[count*2], pair[count*2+1]);
  } else while (len>out) xprintf("%s\n", pair[--len]);
}

void tsort_main(void)
{
  loopfiles(toys.optargs, do_tsort);
}
