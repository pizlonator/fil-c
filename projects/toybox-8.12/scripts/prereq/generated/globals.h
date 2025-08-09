struct gzip_data {
  int level;
};

struct realpath_data {
  char *R, *relative_base;
};

struct tr_data {
  short *map;
  int len1, len2;
};

struct basename_data {
  char *s;
};

struct chmod_data {
  char *mode;
};

struct cmp_data {
  long n;

  int fd;
  char *name;
};

struct fold_data {
  long w;
};

struct grep_data {
  long m, A, B, C;
  struct arg_list *f, *e, *M, *S, *exclude_dir;
  char *color;

  char *purple, *cyan, *red, *green, *grey;
  struct double_list *reg;
  int found, tried, delim;
  struct arg_list **fixed;
};

struct head_data {
  long c, n;

  int file_no;
};

struct ln_data {
  char *t;
};

struct ls_data {
  long w, l, block_size;
  char *color, *sort;

  struct dirtree *files, *singledir;
  unsigned screen_width;
  int nl_title;
  char *escmore;
};

struct mkdir_data {
  char *m, *Z;
};

struct od_data {
  struct arg_list *t;
  char *A;
  long N, w, j;

  int address_idx;
  unsigned types, leftover, star;
  char *buf; // Points to buffers[0] or buffers[1].
  char *bufs[2]; // Used to detect duplicate lines.
  off_t pos;
};

struct sed_data {
  char *i;
  struct arg_list *f, *e;

  // processed pattern list
  struct double_list *pattern;

  char *nextline, *remember, *tarxform;
  void *restart, *lastregex;
  long nextlen, rememberlen, count;
  int fdout, noeol;
  unsigned xx, tarxlen, xflags;
  char delim, xftype;
};

struct sort_data {
  char *t;
  struct arg_list *k;
  char *o, *T, S;

  void *key_list;
  unsigned linecount;
  char **lines, *name;
};

struct tail_data {
  long n, c;
  char *s;

  int file_no, last_fd, ss;
  struct xnotify *not;
  struct {
    char *path;
    int fd;
    struct dev_ino di;
  } *F;
};

struct tee_data {
  void *outputs;
  int out;
};

struct wc_data {
  unsigned long totals[5];
};

struct xargs_data {
  long s, n, P;
  char *E;

  long entries, bytes, np;
  char delim;
  FILE *tty;
};
extern union global_union {
	struct gzip_data gzip;
	struct realpath_data realpath;
	struct tr_data tr;
	struct basename_data basename;
	struct chmod_data chmod;
	struct cmp_data cmp;
	struct fold_data fold;
	struct grep_data grep;
	struct head_data head;
	struct ln_data ln;
	struct ls_data ls;
	struct mkdir_data mkdir;
	struct od_data od;
	struct sed_data sed;
	struct sort_data sort;
	struct tail_data tail;
	struct tee_data tee;
	struct wc_data wc;
	struct xargs_data xargs;
} this;
