/* diff.c - compare files line by line
 *
 * Copyright 2014 Sandeep Sharma <sandeep.jack2756@gmail.com>
 * Copyright 2014 Ashwini Kumar <ak.ashwini1981@gmail.com>
 *
 * See https://pubs.opengroup.org/onlinepubs/9699919799/utilities/diff.html
 * and https://www.cs.dartmouth.edu/~doug/diff.pdf
 *
 * Deviations from posix: always does -u

USE_DIFF(NEWTOY(diff, "<2>2(unchanged-line-format):;(old-line-format):;(no-dereference);(new-line-format):;(color)(strip-trailing-cr)B(ignore-blank-lines)d(minimal)b(ignore-space-change)ut(expand-tabs)w(ignore-all-space)i(ignore-case)T(initial-tab)s(report-identical-files)q(brief)a(text)S(starting-file):F(show-function-line):;L(label)*N(new-file)r(recursive)U(unified)#<0=3", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_ARGFAIL(2)))

config DIFF
  bool "diff"
  default n
  help
    usage: diff [-abBdiNqrTstw] [-L LABEL] [-S FILE] [-U LINES] [-F REGEX ] FILE1 FILE2

    -a	Treat all files as text
    -b	Ignore changes in the amount of whitespace
    -B	Ignore changes whose lines are all blank
    -d	Try hard to find a smaller set of changes
    -F 	Show the most recent line matching the regex
    -i	Ignore case differences
    -L	Use LABEL instead of the filename in the unified header
    -N	Treat absent files as empty
    -q	Output only whether files differ
    -r	Recurse
    -S	Start with FILE when comparing directories
    -s	Report when two files are the same
    -T	Make tabs line up by prefixing a tab when necessary
    -t	Expand tabs to spaces in output
    -u	Unified diff
    -U	Output LINES lines of context
    -w	Ignore all whitespace

    --color     Color output   --strip-trailing-cr   Strip '\r' from input lines
    --no-dereference Don't follow symbolic links
    --TYPE-line-format=FORMAT  Display TYPE (unchanged/old/new) lines using FORMAT
      FORMAT uses printf integer escapes (ala %-2.4x) followed by LETTER: FELMNn
    Supported format specifiers are:
    * %l, the contents of the line, without the trailing newline
    * %L, the contents of the line, including the trailing newline
    * %%, the character '%'
*/

#define FOR_diff
#include "toys.h"

GLOBALS(
  long U;
  struct arg_list *L;
  char *F, *S, *new_line_format, *old_line_format, *unchanged_line_format;

  int dir_num, size, is_binary, is_symlink, differ, change, len[2], *offset[2];
  struct stat st[2];
  struct {
    char **list;
    int nr_elm;
  } dir[2];
  struct {
    FILE *fp;
    int len;
  } file[2];
  struct {
    char *name;
    int len;
  } link[2];
)

#define IS_STDIN(s)     (*(s)=='-' && !(s)[1])

struct v_vector {
  unsigned serial:31;
  unsigned last:1;
  union {
    unsigned hash;
    unsigned p;
  };
};

struct diff {
  long a, b, c, d, prev, suff;
};

struct candidate {
  struct candidate *next, *prev;
  int a, b;
};

enum {
  empty = 1 << 9,
  eol = 1 << 10,
  eof = 1 << 11,
  space = 1 << 12
};

void xlstat(char *path, struct stat *st)
{
  if(lstat(path, st)) perror_exit("Can't lstat %s", path);
}

static int comp(void *a, void *b)
{
  int i = ((struct v_vector *)a)->hash - ((struct v_vector *)b)->hash;

  return i ? : ((struct v_vector *)a)->serial - ((struct v_vector *)b)->serial;
}

static int search(struct candidate **K, int r, int k, int j)
{
  int low = r, upper = k, mid;

  while (low<=(mid = (low+upper)/2)) {
    if (K[mid]->b < j && K[mid + 1]->b > j) return mid;
    if (K[mid]->b < j) low = mid + 1;
    else if (K[mid]->b > j) upper = mid - 1;
    else return -1;
  }
  return -1;
}

static struct candidate *new_candidate(int i, int j, struct candidate *prev)
{
  struct candidate *c = xzalloc(sizeof(struct candidate));

  c->a = i;
  c->b = j;
  c->prev = prev;
  return c;
}

/* 1. Search K[r: k] for an element K[s] such that K[s]-> b < j and K[s + 1]->b > j
 * 2. if found do
 *  2.a. If K[s + 1]->b > j do K[r] = c; r = s+1 and c = candidate(i, j, K[s]) //we have a candidate
 *  2.b. if s = k (fence reached move it further) do K[k + 2] = K[k + 1], k++
 * 3. if E[p].last true break i.e we have reached at the end of an equiv class
 *    else p = p + 1 //keep traversing the equiv class.
 * 4. K[r] = c //Save the sucessfully filled k-candidate.
 */
static void do_merge(struct candidate **K, int *k, int i,
    struct v_vector *E, int p)
{
  int r = 0, s, j;
  struct candidate *pr = 0, *c = K[0];

  for (;;) {
    j = E[p].serial;
    s = search(K, r, *k, j);
    if (s>=0 && K[s]->b<j && K[s+1]->b>j) {
      if (K[s+1]->b>j) {
        pr = K[s];
        if (r && K[r]) c->next = K[r];
        K[r] = c;
        r = s+1;
        c = new_candidate(i , j, pr);
      }
      if (s == *k) {
        ++*k;
        K[*k+1] = K[*k];
        break;
      }
    }
    if (E[p].last) break;
    else p++;
  }
  K[r] = c;
}

static int read_tok(FILE *fp, off_t *off, int tok)
{
  int t = 0, is_space;

  tok |= empty;
  while (!(tok & eol)) {
    t = fgetc(fp);

    if (FLAG(strip_trailing_cr) && t == '\r') {
      int t2 = fgetc(fp);
      if (t2 == '\n') {
        t = t2;
        if (off) (*off)++;
      } else {
        ungetc(t2, fp);
      }
    }

    if (off && t != EOF) *off += 1;
    is_space = isspace(t) || (t == EOF);
    tok |= (t & (eof + eol)); //set tok eof+eol when t is eof

    if (t == '\n') tok |= eol;
    if (FLAG(i)) if (t >= 'A' && t <= 'Z') t = tolower(t);

    if (FLAG(w) && is_space) continue;

    if (FLAG(b)) {
      if (tok & space) {
        if (is_space) continue;
        tok &= ~space;
      } else if (is_space) t = space + ' ';
    }
    tok &= ~(empty + 0xff);  //remove empty and char too.
    tok |= t; //add most recent char
    break;
  }

  return tok;
}

int bcomp(void *a, void *b)
{
  struct v_vector *l = (struct v_vector *)a, *r = (struct v_vector *)b;

  return (l->hash-r->hash) ? : r[-1].last ? 0 : -1;
}

/*  file[0] corresponds file 1 and file[1] correspond file 2.
 * 1. calc hashes for both the files and store them in vector(v[0], v[1])
 * 2. sort file[1] with hash as primary and serial as sec. key
 * 3. Form the equivalance class of file[1] stored in e vector. It lists all the equivalence
 *    classes of lines in file[1], with e.last = true on the last element of each class.
 *    The elements are ordered by serial within classes.
 * 4. Form the p vector stored in  p_vector. p_vector[i], if non-zero, now points in e vector
 *    to the beginning of the equiv class of lines in file[1] equivalent to line
 *    i in file[0].
 * 5. Form the k-candidates as discribed in do_merge.
 * 6. Create a vector J[i] = j, such that i'th line in file[0] is j'th line of
 *    file[1], i.e J comprises LCS
 */
static int *create_j_vector()
{
  int tok, i, j, size = 100, k;
  off_t off;
  long hash;
  int *p_vector, *J;
  struct v_vector *v[2], *e;
  struct candidate **kcand, *pr;

  for (i = 0; i < 2; i++) {
    tok = off = 0;
    hash = 5831;
    v[i] = xzalloc(size * sizeof(struct v_vector));
    TT.offset[i] = xzalloc(size * sizeof(int));
    TT.file[i].len = 0;
    if (fseek(TT.file[i].fp, 0, SEEK_SET)) perror_exit("fseek failed");

    while (1) {
      tok  = read_tok(TT.file[i].fp, &off, tok);
      if (!(tok & empty)) {
        hash = ((hash << 5) + hash) + (tok & 0xff);
        continue;
      }

      if (size == ++TT.file[i].len) {
        size = size * 11 / 10;
        v[i] = xrealloc(v[i], size*sizeof(struct v_vector));
        TT.offset[i] = xrealloc(TT.offset[i], size*sizeof(int));
      }

      v[i][TT.file[i].len].hash = hash & INT_MAX;
      TT.offset[i][TT.file[i].len] = off;
      if ((tok & eof)) {
        TT.offset[i][TT.file[i].len] = ++off;
        break;
      }
      hash = 5831;  //next line
      tok = 0;
    }
    if (TT.offset[i][TT.file[i].len]-TT.offset[i][TT.file[i].len-1] == 1)
      TT.file[i].len--;
  }

  for (i = 0; i<=TT.file[1].len; i++) v[1][i].serial = i;
  qsort(v[1]+1, TT.file[1].len, sizeof(struct v_vector), (void *)comp);

  e = v[1];
  e[0].serial = 0;
  e[0].last = 1;
  for (i = 1; i<=TT.file[1].len; i++)
    e[i].last = i==TT.file[1].len || v[1][i].hash!=v[1][i+1].hash;

  p_vector = xzalloc((TT.file[0].len+2)*sizeof(int));
  for (i = 1; i<=TT.file[0].len; i++) {
    void *r = bsearch(&v[0][i], e+1, TT.file[1].len, sizeof(*e), (void *)bcomp);
    if (r) p_vector[i] = (struct v_vector *)r - e;
  }

  for (i = 1; i<=TT.file[0].len; i++) e[i].p = p_vector[i];
  free(p_vector);

  size = 100;
  kcand = xzalloc(size * sizeof(struct candidate *));

  kcand[0] = new_candidate(0 , 0, 0);
  kcand[1] = new_candidate(TT.file[0].len+1, TT.file[1].len+1, 0); //the fence

  k = 0;  //last successfully filled k candidate.
  for (i = 1; i<=TT.file[0].len; i++) {
    if (!e[i].p) continue;
    if ((size - 2) == k) {
      size = size * 11 / 10;
      kcand = xrealloc(kcand, (size*sizeof(struct candidate *)));
    }
    do_merge(kcand, &k, i, e, e[i].p);
  }
  free(v[0]); //no need for v_vector now.
  free(v[1]);

  J = xzalloc((TT.file[0].len+2)*sizeof(int));

  for (pr = kcand[k]; pr; pr = pr->prev) J[pr->a] = pr->b;
  J[TT.file[0].len+1] = TT.file[1].len+1; //mark boundary

  for (i = k+1; i>=0; i--) llist_traverse(kcand[i], free);
  free(kcand);

  for (i = 1; i<=TT.file[0].len; i++) { // jackpot?
    if (!J[i]) continue;

    if (fseek(TT.file[0].fp, TT.offset[0][i-1], SEEK_SET)
     || fseek(TT.file[1].fp, TT.offset[1][J[i]-1], SEEK_SET))
       perror_exit("fseek");

    for (j = J[i]; i<=TT.file[0].len && J[i]==j; i++, j++) {
      int tok0 = 0, tok1 = 0;

      do {
        tok0 = read_tok(TT.file[0].fp, NULL, tok0);
        tok1 = read_tok(TT.file[1].fp, NULL, tok1);
        if (((tok0 ^ tok1) & empty) || ((tok0 & 0xff) != (tok1 & 0xff)))
          J[i] = 0;
      } while (!(tok0 & tok1 & empty));
    }
  }
  return J;
}

static int *diff(char **files)
{
  size_t i ,j;
  int s, t;
  char *bufi, *bufj;

  TT.is_binary = 0; //loop calls to diff
  TT.differ = 0;

  for (i = 0; i < 2; i++) {
    if ((j = !strcmp(files[i], "-")) || S_ISFIFO(TT.st[i].st_mode)) {
      char *tmp_name;
      int srcfd = j ? 0 : open(files[i], O_RDONLY),
        tmpfd = xtempfile("fifo", &tmp_name);

      unlink(tmp_name);
      free(tmp_name);

      xsendfile(srcfd, tmpfd);
      if (!j) close(srcfd);
      TT.file[i].fp = fdopen(tmpfd, "r");
    } else TT.file[i].fp = fopen(files[i], "r");

    if (!TT.file[i].fp) {
      perror_msg("%s", files[i]);
      TT.differ = 2;
      return 0; //return SAME
    }
  }

  s = sizeof(toybuf)/2;
  bufi = toybuf;
  bufj = toybuf+s;

  if (fseek(TT.file[0].fp, 0, SEEK_SET) || fseek(TT.file[1].fp, 0, SEEK_SET))
    perror_exit("fseek");

  if (FLAG(a)) return create_j_vector();

  while (1) {
    i = fread(bufi, 1, s, TT.file[0].fp);
    j = fread(bufj, 1, s, TT.file[1].fp);

    if (i != j) TT.differ = 1;

    for (t = 0; t < i && !TT.is_binary; t++) if (!bufi[t]) TT.is_binary = 1;
    for (t = 0; t < j && !TT.is_binary; t++) if (!bufj[t]) TT.is_binary = 1;

    i = minof(i, j);
    for (t = 0; t < i; t++) if (bufi[t] != bufj[t]) TT.differ = 1;

    if (!i || !j) break;
  }
  if (TT.is_binary || !TT.differ) return 0;

  return create_j_vector();
}

static void print_line_matching_regex(int a, regex_t *reg, int *off_set, FILE *fp) {
  int i = 0, j = 0, line_buf_size = 100, cc = 0;
  char* line = xzalloc(line_buf_size * sizeof(char));
  for (i = a; a > 0; --i) {
    int line_len = 0;
    if (fseek(fp, off_set[i - 1], SEEK_SET)) perror_exit("fseek failed");
    for (j = 0; j < (off_set[i] - off_set[i - 1]); j++) {
      cc = fgetc(fp);
      if (cc == EOF || cc == '\n') {
        break;
      }
      ++line_len;
      if (line_len >= line_buf_size) {
        line_buf_size = line_buf_size * 11 / 10;
        line = xrealloc(line, line_buf_size*sizeof(char));
      }
      line[j] = cc;
    }
    line[line_len] = '\0';
    if (!regexec0(reg, line, line_len, 0, NULL, 0)) {
      printf(" %s", line);
      break;
    }
  }
  free(line);
}

static void print_diff(int a, int b, char c, int *off_set, FILE *fp)
{
  int i, j, cc, cl;
  char *reset = 0, *fmt = 0;

  if (!TT.new_line_format && c!=' ' && FLAG(color)) {
    printf("\e[%dm", 31+(c=='+'));
    reset = "\e[0m";
  }

  for (i = a; i <= b; i++) {
    if (fseek(fp, off_set[i - 1], SEEK_SET)) perror_exit("fseek failed");
    if (TT.new_line_format) {
      if (c == '+') fmt = TT.new_line_format;
      else if (c == '-') fmt = TT.old_line_format;
      else fmt = TT.unchanged_line_format;
      while (*fmt) {
        if (*fmt == '%') {
          fmt++;
          char f = *fmt++;
          if (f == '%') putchar('%');
          else if (f == 'l' || f == 'L') {
            for (j = 0; j <  (off_set[i] - off_set[i - 1]); j++) {
              cc = fgetc(fp);
              if (cc == EOF) break;
              if (cc != '\n' || f == 'L') putchar(cc);
            }
          } else error_exit("Unrecognized format specifier %%%c", f);
        } else putchar(*fmt++);
      }
      continue;
    }
    putchar(c);
    if (FLAG(T)) putchar('\t');
    for (j = 0, cl = 0; j <  (off_set[i] - off_set[i - 1]); j++) {
      cc = fgetc(fp);
      if (cc == EOF) {
        printf("%s\n\\ No newline at end of file\n", reset ? : "");
        return;
      }
      if ((cc == '\t') && FLAG(t)) do putchar(' '); while (++cl & 7);
      else {
        putchar(cc); //xputc has calls to fflush, it hurts performance badly.
        cl++;
      }
    }
  }
  if (reset) xputsn(reset);
}

static char *concat_file_path(char *path, char *default_path)
{
  char *final_path;

  if ('/' == path[strlen(path) - 1]) {
    while (*default_path == '/') ++default_path;
    final_path = xmprintf("%s%s", path, default_path);
  }
  else if (*default_path != '/')
    final_path = xmprintf("%s/%s", path, default_path);
  else final_path = xmprintf("%s%s", path, default_path);
  return final_path;
}

static int skip(struct dirtree *node)
{
  int len = strlen(toys.optargs[TT.dir_num]), ret = 0;
  char *tmp = NULL, *ptr, *f_path = dirtree_path(node, NULL);
  struct stat st;

  ptr = f_path;
  ptr += len;
  if (ptr[0]) {
    tmp = concat_file_path(toys.optargs[1 - TT.dir_num], ptr);
    if (tmp && !stat(tmp, &st)) ret = 0; //it is there on other side
    else ret = 1; //not present on other side.
  }
  free(f_path);
  if (tmp) free(tmp);
  return ret; //add otherwise
}

static void add_to_list(struct dirtree *node)
{
  char *full_path;

  TT.dir[TT.dir_num].list = xrealloc(TT.dir[TT.dir_num].list,
      (TT.size + 1)*sizeof(char*));
  TT.size++;
  full_path = dirtree_path(node, NULL);
  TT.dir[TT.dir_num].list[TT.size - 1] = full_path;
}

static int list_dir(struct dirtree *node)
{
  int ret = 0;

  if (!dirtree_notdotdot(node)) return 0;

  if (S_ISDIR(node->st.st_mode) && !node->parent) { //add root dirs.
    add_to_list(node);
    return (DIRTREE_RECURSE|((FLAG(no_dereference)) ? 0 : DIRTREE_SYMFOLLOW));
  }

  if (S_ISDIR(node->st.st_mode) && FLAG(r)) {
    if (!FLAG(N)) ret = skip(node);
    if (!ret) return DIRTREE_RECURSE|((FLAG(no_dereference)) ? 0 : DIRTREE_SYMFOLLOW);
    else {
      add_to_list(node); //only at one side.
      return 0;
    }
  } else {
    add_to_list(node);
    return S_ISDIR(node->st.st_mode) ? 0 : (DIRTREE_RECURSE|((FLAG(no_dereference)) ? 0 : DIRTREE_SYMFOLLOW));
  }
}

static int cmp(void *p1, void *p2)
{
   return strcmp(*(char **)p1, *(char **)p2);
}

// quote and escape filenames that have awkward characters
char *quote_filename(char *filename)
{
  char *to = "abfnrtv\"\\", *from = "\a\b\f\n\r\t\v\"\\", *s, *t=0, *u;
  int len, quote = 0;

  for (;;) {
    // measure escapes on first pass, write on second
    len = 0;
    for (s = filename; *s; s++) {
      if ((u = strchr(from, *s))) {
        if (t) t[len] = '\\', t[len+1] = to[u-from];
        len += 2;
      } else if (*s<0x20 || *s>=0x80)
        len += snprintf(t+len, 5*!!t, "\\%.3o", *s);
      else {
        if (t) t[len] = *s;
        len++;
      }
    }
    if (t) {
      if (quote) t[len++] = '"';
      t[len] = 0;

      return t-quote;
    }

    // construct the new string
    quote = strlen(filename)!=len || strchr(filename, ' ');
    t = xmalloc(len+1+2*quote);
    if (quote) *t++ = '"';
  }
}

static void show_label(char *prefix, char *filename, struct stat *sb)
{
  char date[36];
  char *quoted_file;

  quoted_file = quote_filename(filename);
  printf("%s %s\t%s\n", prefix, quoted_file,
    format_iso_time(date, sizeof(date), &sb->st_mtim));
  free(quoted_file);
}

static void do_symlink_diff(char **files)
{
  size_t i;
  int s = sizeof(toybuf)/2;

  TT.is_symlink = 1;
  TT.differ = 0;
  TT.link[0].name = TT.link[1].name = NULL;
  for (i = 0; i < 2; i++) {
    TT.link[i].name = xreadlink(files[i]);
    if (TT.link[i].name == 0) {
      perror_msg("readlink failed");
      TT.differ = 2;
      free(TT.link[0].name);
      return;
    }
    TT.link[i].len = strlen(TT.link[i].name);
  }

  if (TT.link[0].len != TT.link[1].len) TT.differ = 1;
  else if (smemcmp(TT.link[0].name, TT.link[1].name, TT.link[0].len))
    TT.differ = 1;
  free(TT.link[0].name);
  free(TT.link[1].name);
  return;
}

static void do_diff(char **files)
{
  long i = 1, size = 1, x = 0, change = 0, ignore_white,
   start1, end1, start2, end2;
  struct diff *d;
  struct arg_list *llist = TT.L;
  int *J;
  regex_t reg;

  TT.offset[0] = TT.offset[1] = NULL;
  J = diff(files);

  if (!J) return; //No need to compare, have to status only

  if (TT.F) {
    xregcomp(&reg, TT.F, 0);
  }

  d = xzalloc(size *sizeof(struct diff));
  do {
    ignore_white = 0;
    for (d[x].a = i; d[x].a<=TT.file[0].len; d[x].a++) {
      if (J[d[x].a] != (J[d[x].a - 1] + 1)) break;
      else continue;
    }
    d[x].c = (J[d[x].a - 1] + 1);

    for (d[x].b = (d[x].a - 1); d[x].b<=TT.file[0].len; d[x].b++) {
      if (J[d[x].b + 1]) break;
      else continue;
    }
    d[x].d = (J[d[x].b + 1] - 1);

    if (FLAG(B)) {
      if (d[x].a <= d[x].b) {
        if ((TT.offset[0][d[x].b] - TT.offset[0][d[x].a - 1])
            == (d[x].b - d[x].a + 1))
          ignore_white = 1;
      } else if (d[x].c <= d[x].d){
        if ((TT.offset[1][d[x].d] - TT.offset[1][d[x].c - 1])
            == (d[x].d - d[x].c + 1))
          ignore_white = 1;
      }
    }

    //is we have diff ?   TODO: lolcat?
    if ((d[x].a <= d[x].b || d[x].c <= d[x].d) && !ignore_white) change = 1;

    if (!ignore_white) d = xrealloc(d, (x + 2) *sizeof(struct diff));
    i = d[x].b + 1;
    if (i>TT.file[0].len) break;
    J[d[x].b] = d[x].d;
    if (!ignore_white) x++;
  } while (i<=TT.file[0].len);

  i = x+1;
  TT.differ = change; //update status, may change bcoz of -w etc.

  if (!FLAG(q) && change) {
    if (!TT.new_line_format) {
      if (FLAG(color)) printf("\e[1m");
      if (FLAG(L)) printf("--- %s\n", llist->arg);
      else show_label("---", files[0], &(TT).st[0]);
      if (!FLAG(L) || !llist->next) show_label("+++", files[1], &(TT).st[1]);
      else {
        while (llist->next) llist = llist->next;
        printf("+++ %s\n", llist->arg);
      }
      if (FLAG(color)) printf("\e[0m");
    }

    struct diff *t, *ptr1 = d, *ptr2 = d;
    while (i) {
      long a,b;

      // trim context to file len.
      if (TT.new_line_format || TT.U>TT.file[0].len) TT.U = TT.file[0].len;
      if (ptr1->b < ptr1->a && ptr1->d < ptr1->c) {
        i--;
        continue;
      }
      //Handle the context stuff
      a =  ptr1->a;
      b = minof(TT.file[0].len, ptr1->b);
      if (i == x + 1) ptr1->suff = maxof(1, a-TT.U);
      else if (ptr1[-1].prev >= ptr1->a-TT.U) ptr1->suff = ptr1[-1].prev+1;
      else ptr1->suff =  ptr1->a-TT.U;
calc_ct:
      if (i > 1) {
        if ((ptr2->b + TT.U) >= (ptr2  + 1)->a) {
          ptr2++;
          i--;
          goto calc_ct;
        } else ptr2->prev = ptr2->b + TT.U;
      } else ptr2->prev = ptr2->b;
      start1 = (ptr2->prev - ptr1->suff + 1);
      end1 = (start1 == 1) ? -1 : start1;
      start2 = maxof(1, ptr1->c - (ptr1->a - ptr1->suff));
      end2 = ptr2->prev - ptr2->b + ptr2->d;

      if (!TT.new_line_format) {
        if (FLAG(color)) printf("\e[36m");
        printf("@@ -%ld", start1 ? ptr1->suff: (ptr1->suff -1));
        if (end1 != -1) printf(",%ld ", ptr2->prev-ptr1->suff + 1);
        else putchar(' ');

        printf("+%ld", (end2 - start2 + 1) ? start2: (start2 -1));
        if ((end2 - start2 +1) != 1) printf(",%ld ", (end2 - start2 +1));
        else putchar(' ');
        printf("@@");
        if (FLAG(color)) printf("\e[0m");
        if (TT.F) {
          print_line_matching_regex(ptr1->suff-1, &reg, TT.offset[0], TT.file[0].fp);
        }
        putchar('\n');
      }

      for (t = ptr1; t <= ptr2; t++) {
        if (t==ptr1) print_diff(t->suff, t->a-1, ' ', TT.offset[0], TT.file[0].fp);
        print_diff(t->a, t->b, '-', TT.offset[0], TT.file[0].fp);
        print_diff(t->c, t->d, '+', TT.offset[1], TT.file[1].fp);
        if (t == ptr2)
          print_diff(t->b+1, (t)->prev, ' ', TT.offset[0], TT.file[0].fp);
        else print_diff(t->b+1, (t+1)->a-1, ' ', TT.offset[0], TT.file[0].fp);
      }
      ptr2++;
      ptr1 = ptr2;
      i--;
    } //end of while
  } //End of !FLAG_q
  free(d);
  free(J);
  free(TT.offset[0]);
  free(TT.offset[1]);
}

static void show_status(char **files)
{
  if (TT.differ==2) return; // TODO: needed?
  if (TT.differ ? FLAG(q) || TT.is_binary || TT.is_symlink : FLAG(s))
    printf("%s %s and %s %s\n", TT.is_symlink ? "Symbolic links" : "Files",
      files[0], files[1], TT.differ ? "differ" : "are identical");
}

static void create_empty_entry(int l , int r, int j)
{
  struct stat st[2];
  char *f[2], *path[2];
  int i;

  for (i = 0; i < 2; i++) {
    if (j) {
      if (!FLAG(N) || i!=(j>0)) continue;
      path[!i] = concat_file_path(TT.dir[!i].list[0],
        TT.dir[i].list[i ? r : l]+TT.len[i]);
      f[!i] = "/dev/null";
    }
    path[i] = f[i] = TT.dir[i].list[i ? r : l];
    (FLAG(no_dereference) ? lstat : stat)(f[i], st+i);
    if (j) st[!i] = st[i];
  }

  for (i = 0; i<2; i++) {
    if (!S_ISREG(st[i].st_mode) && !S_ISDIR(st[i].st_mode) && !S_ISLNK(st[i].st_mode)) {
      printf("File %s is not a regular file, symbolic link, or directory and was skipped\n",
        path[i]);
      break;
    }
  }

  if (i != 2);
  else if ((st[0].st_mode & S_IFMT) != (st[1].st_mode & S_IFMT)) {
    i = S_ISREG(st[0].st_mode) + 2 * S_ISLNK(st[0].st_mode);
    int k = S_ISREG(st[1].st_mode) + 2 * S_ISLNK(st[1].st_mode);
    char *fidir[] = {"directory", "regular file", "symbolic link"};
    printf("File %s is a %s while file %s is a %s\n",
      path[0], fidir[i], path[1], fidir[k]);
  } else if (S_ISDIR(st[0].st_mode))
    printf("Common subdirectories: %s and %s\n", path[0], path[1]);
  else if (S_ISLNK(st[0].st_mode)) {
    do_symlink_diff(f);
    show_status(path);
  } else {
    do_diff(f);
    show_status(path);
    if (TT.file[0].fp) fclose(TT.file[0].fp);
    if (TT.file[1].fp) fclose(TT.file[1].fp);
  }

  if (FLAG(N) && j) free(path[j<=0]);
}

static void diff_dir(int *start)
{
  int l, r, j = 0;

  l = start[0]; //left side file start
  r = start[1]; //right side file start
  while (l < TT.dir[0].nr_elm && r < TT.dir[1].nr_elm) {
    if ((j = strcmp (TT.dir[0].list[l]+TT.len[0],
            (TT.dir[1].list[r]+TT.len[1]))) && !FLAG(N)) {
      if (j > 0) {
        printf("Only in %s: %s\n", TT.dir[1].list[0], TT.dir[1].list[r]+TT.len[1]);
        free(TT.dir[1].list[r++]);
      } else {
        printf ("Only in %s: %s\n", TT.dir[0].list[0], TT.dir[0].list[l]+TT.len[0]);
        free(TT.dir[0].list[l++]);
      }
      TT.differ = 1;
    } else {
      create_empty_entry(l, r, j); //create non empty dirs/files if -N.
      if (j>=0) free(TT.dir[1].list[r++]);
      if (j<=0) free(TT.dir[0].list[l++]);
    }
  }

  if (l == TT.dir[0].nr_elm) {
    while (r<TT.dir[1].nr_elm) {
      if (!FLAG(N)) {
        printf ("Only in %s: %s\n", TT.dir[1].list[0], TT.dir[1].list[r]+TT.len[1]);
        TT.differ = 1;
      } else create_empty_entry(l, r, 1);
      free(TT.dir[1].list[r++]);
    }
  } else if (r == TT.dir[1].nr_elm) {
    while (l<TT.dir[0].nr_elm) {
      if (!FLAG(N)) {
        printf ("Only in %s: %s\n", TT.dir[0].list[0], TT.dir[0].list[l]+TT.len[0]);
        TT.differ = 1;
      } else create_empty_entry(l, r, -1);
      free(TT.dir[0].list[l++]);
    }
  }
  free(TT.dir[0].list[0]); //we are done, free root nodes too
  free(TT.dir[0].list);
  free(TT.dir[1].list[0]);
  free(TT.dir[1].list);
}

void diff_main(void)
{
  int i, j = 0, k = 1, start[2] = {1, 1};
  char **files = toys.optargs;

  toys.exitval = 2;
  if (FLAG(color) && !isatty(1)) toys.optflags ^= FLAG_color;

  for (j = 0; j < 2; j++) {
    if (IS_STDIN(files[j])) fstat(0, &TT.st[j]);
    else if (FLAG(no_dereference)) xlstat(files[j], &TT.st[j]);
    else xstat(files[j], &TT.st[j]);
  }

  if (S_ISDIR(TT.st[0].st_mode) != S_ISDIR(TT.st[1].st_mode))
    error_exit("can't compare directory to non-directory");

  if (TT.unchanged_line_format || TT.old_line_format || TT.new_line_format) {
    if (S_ISDIR(TT.st[0].st_mode) && S_ISDIR(TT.st[1].st_mode))
      error_exit("can't use line format with directories");
    if (!TT.unchanged_line_format) TT.unchanged_line_format = "%l\n";
    if (!TT.old_line_format) TT.old_line_format = "%l\n";
    if (!TT.new_line_format) TT.new_line_format = "%l\n";
  }

  if (same_file(TT.st, TT.st+1)) {
    toys.exitval = 0;
    return show_status(files);
  }

  if (S_ISDIR(TT.st[0].st_mode) && S_ISDIR(TT.st[1].st_mode)) {
    for (j = 0; j < 2; j++) {
      memset(TT.dir+j, 0, sizeof(*TT.dir));
      dirtree_flagread(files[j], (FLAG(no_dereference)) ? 0 : DIRTREE_SYMFOLLOW, list_dir);
      TT.dir[j].nr_elm = TT.size; //size updated in list_dir
      qsort(&TT.dir[j].list[1], TT.size-1, sizeof(char *), (void *)cmp);

      TT.len[j] = strlen(TT.dir[j].list[0]); //calc root node len
      TT.len[j] += TT.dir[j].list[0][TT.len[j]-1] != '/';

      if (FLAG(S)) {
        while (k<TT.size && strcmp(TT.dir[j].list[k]+TT.len[j], TT.S)<0) {
          start[j]++;
          k++;
        }
      }
      TT.dir_num++;
      TT.size = 0;
      k = 1;
    }
    diff_dir(start);
  } else {
    if (S_ISDIR(TT.st[0].st_mode) || S_ISDIR(TT.st[1].st_mode)) {
      int d = S_ISDIR(TT.st[0].st_mode);
      char *slash = strrchr(files[d], '/');

      files[!d] = concat_file_path(files[!d], slash ? slash+1 : files[d]);
      if ((FLAG(no_dereference) ? lstat : stat)(files[!d], &TT.st[!d]))
        perror_exit("%s", files[!d]);
    }
    if ((S_ISLNK(TT.st[0].st_mode)) != S_ISLNK(TT.st[1].st_mode)) {
      i = !strcmp(files[0], "-") ? 0 : S_ISREG(TT.st[0].st_mode) + 2 * S_ISLNK(TT.st[0].st_mode);
      int k = !strcmp(files[0], "-") ? 0 : S_ISREG(TT.st[1].st_mode) + 2 * S_ISLNK(TT.st[1].st_mode);
      char *fidir[] = {"fifo", "regular file", "symbolic link"};
      printf("File %s is a %s while file %s is a %s\n",
        files[0], fidir[i], files[1], fidir[k]);
      TT.differ = 1;
    } else {
      if (S_ISLNK(TT.st[0].st_mode)) do_symlink_diff(files);
      else do_diff(files);
      show_status(files);
    }
    if (TT.file[0].fp) fclose(TT.file[0].fp);
    if (TT.file[1].fp) fclose(TT.file[1].fp);
  }
  toys.exitval = TT.differ; //exit status will be the status
}
