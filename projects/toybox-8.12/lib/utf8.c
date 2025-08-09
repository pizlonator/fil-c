#include "toys.h"

// Show width many columns, negative means from right edge, out=0 just measure
// if escout, send it unprintable chars, otherwise pass through raw data.
// Returns width in columns, moves *str to end of data consumed.
int crunch_str(char **str, int width, FILE *out, char *escmore,
  int (*escout)(FILE *out, int cols, int wc))
{
  int columns = 0, col, bytes;
  char *start, *end;
  unsigned wc;

  for (end = start = *str; *end; columns += col, end += bytes) {
    if ((bytes = utf8towc(&wc, end, 4))>0 && (col = wcwidth(wc))>=0) {
      if (!escmore || wc>255 || !strchr(escmore, wc)) {
        if (width-columns<col) break;
        if (out) fwrite(end, bytes, 1, out);

        continue;
      }
    }

    if (bytes<1) {
      bytes = 1;
      wc = *end;
    }
    col = width-columns;
    if (col<1) break;
    if (escout) {
      if ((col = escout(out, col, wc))<0) break;
    } else if (out) fwrite(end, 1, bytes, out);
  }
  *str = end;

  return columns;
}


// standard escapes: ^X if <32, <XX> if invalid UTF8, U+XXXX if UTF8 !iswprint()
int crunch_escape(FILE *out, int cols, int wc)
{
  char buf[11];
  int rc;

  if (wc<' ') rc = sprintf(buf, "^%c", '@'+wc);
  else if (wc<256) rc = sprintf(buf, "<%02X>", wc);
  else rc = sprintf(buf, "U+%04X", wc);

  if (rc > cols) buf[rc = cols] = 0;
  if (out) fputs(buf, out);

  return rc;
}

// Display "standard" escapes in reverse video.
int crunch_rev_escape(FILE *out, int cols, int wc)
{
  int rc;

  xputsn("\e[7m");
  rc = crunch_escape(out, cols, wc);
  xputsn("\e[27m");

  return rc;
}

// Write width chars at start of string to strdout with standard escapes
// Returns length in columns so caller can pad it out with spaces.
int draw_str(char *start, int width)
{
  return crunch_str(&start, width, stdout, 0, crunch_rev_escape);
}

// Return utf8 columns
int utf8len(char *str)
{
  return crunch_str(&str, INT_MAX, 0, 0, crunch_rev_escape);
}

// Return bytes used by (up to) this many columns
int utf8skip(char *str, int width)
{
  char *s = str;

  crunch_str(&s, width, 0, 0, crunch_rev_escape);

  return s-str;
}

// Print utf8 to stdout with standard escapes, trimmed to width and padded
// out to padto. If padto<0 left justify. Returns columns printed
int draw_trim_esc(char *str, int padto, int width, char *escmore,
  int (*escout)(FILE *out, int cols, int wc))
{
  int apad = abs(padto), len = utf8len(str);

  if (padto>=0 && len>width) str += utf8skip(str, len-width);
  if (len>width) len = width;

  // Left pad if right justified
  if (padto>0 && apad>len) printf("%*s", apad-len, "");
  crunch_str(&str, len, stdout, 0, crunch_rev_escape);
  if (padto<0 && apad>len) printf("%*s", apad-len, "");

  return (apad > len) ? apad : len;
}

// draw_trim_esc() with default escape
int draw_trim(char *str, int padto, int width)
{
  return draw_trim_esc(str, padto, width, 0, 0);
}
