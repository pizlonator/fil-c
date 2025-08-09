/* awk.c - An awk implementation.
 * vi: tabstop=2 softtabstop=2 shiftwidth=2
 *
 * Copyright 2024 Ray Gardner <raygard@gmail.com>
 *
 * See https://pubs.opengroup.org/onlinepubs/9799919799/utilities/awk.html
 *
 * Deviations from posix: Don't handle LANG, LC_ALL, etc.
 *   Accept regex for RS
 *   Bitwise functions (from gawk): and, or, xor, lshift, rshift
 *   Attempt to follow tradition (nawk, gawk) where it departs from posix
 *
 * TODO: Lazy field splitting; improve performance; more testing/debugging

USE_AWK(NEWTOY(awk, "F:v*f*bc", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_LINEBUF))

config AWK
  bool "awk"
  default n
  help
    usage:  awk [-F sepstring] [-v assignment]... program [argument...]
      or:
            awk [-F sepstring] -f progfile [-f progfile]... [-v assignment]...
                  [argument...]
      also:
      -b : count bytes, not characters (experimental)
      -c : compile only, do not run
*/

#define FOR_awk
#include "toys.h"

GLOBALS(
  struct arg_list *f;
  struct arg_list *v;
  char *F;

  struct scanner_state {
      char *p;
      char *progstring;
      struct arg_list *prog_args;
      char *filename;
      char *line;
      size_t line_size;
      ssize_t line_len;
      int line_num;
      int ch;
      FILE *fp;
      // state includes latest token seen
      int tok;
      int tokbuiltin;
      int toktype;
      char *tokstr;
      size_t maxtok;
      size_t toklen;
      double numval;
      int error;  // Set if lexical error.
  } *scs;
  char *tokstr;
  int prevtok;

  struct compiler_globals {
    int in_print_stmt;
    int paren_level;
    int in_function_body;
    int funcnum;
    int nparms;
    int compile_error_count;
    int first_begin;
    int last_begin;
    int first_end;
    int last_end;
    int first_recrule;
    int last_recrule;
    int break_dest;
    int continue_dest;
    int stack_offset_to_fix;  // fixup stack if return in for(e in a)
    int range_pattern_num;
    int rule_type;  // tkbegin, tkend, or 0
  } cgl;

  // zvalue: the main awk value type
  // Can be number or string or both, or else map (array) or regex
  struct zvalue {
    unsigned flags;
    double num;
    union { // anonymous union not in C99; not going to fix it now.
      struct zstring *vst;
      struct zmap *map;
      regex_t *rx;
    };
  } nozvalue;   // to shut up compiler warning TODO FIXME

  struct runtime_globals {
    struct zvalue cur_arg;
    FILE *fp;           // current data file
    int narg;           // cmdline arg index
    int nfiles;         // num of cmdline data file args processed
    int eof;            // all cmdline files (incl. stdin) read
    char *recptr;
    struct zstring *zspr;      // Global to receive sprintf() string value
  } rgl;

  // Expanding sequential list
  struct zlist {
    char *base, *limit, *avail;
    size_t size;
  } globals_table,  // global symbol table
    locals_table,     // local symbol table
    func_def_table;  // function symbol table
  // runtime lists
  struct zlist literals, fields, zcode, stack;

  char *progname;

  int spec_var_limit;
  int zcode_last;
  struct zvalue *stackp;  // top of stack ptr

  char *pbuf;   // Used for number formatting in num_to_zstring()
#define RS_MAX  64
  char rs_last[RS_MAX];
  regex_t rx_rs_default, rx_rs_last;
  regex_t rx_default, rx_last, rx_printf_fmt;
#define FS_MAX  64
  char fs_last[FS_MAX];
  char one_char_fs[4];
  int nf_internal;  // should match NF
  char range_sw[64];   // FIXME TODO quick and dirty set of range switches
  int file_cnt, std_file_cnt;

  struct zfile {
    struct zfile *next;
    char *fn;
    FILE *fp;
    char mode;  // w, a, or r
    char file_or_pipe;  // 1 if file, 0 if pipe
    char is_tty, is_std_file;
    char eof;
    int ro, lim, buflen;
    char *buf;
  } *zfiles, *cfile, *zstdout;
)

static void awk_exit(int status)
{
  toys.exitval = status;
  xexit();
}
#ifdef __GNUC__
#define ATTR_FALLTHROUGH_INTENDED __attribute__ ((fallthrough))
#else
#define ATTR_FALLTHROUGH_INTENDED
#endif

////////////////////
////   declarations
////////////////////

#define PBUFSIZE  512 // For num_to_zstring()

enum toktypes {
    // EOF (use -1 from stdio.h)
    ERROR = 2, NEWLINE, VAR, NUMBER, STRING, REGEX, USERFUNC, BUILTIN, TOKEN,
    KEYWORD
    };

// Must align with lbp_table[]
enum tokens {
    tkunusedtoken, tkeof, tkerr, tknl,
    tkvar, tknumber, tkstring, tkregex, tkfunc, tkbuiltin,

// static char *ops = " ;  ,  [  ]  (  )  {  }  $  ++ -- ^  !  *  /  %  +  -     "
//        "<  <= != == >  >= ~  !~ && || ?  :  ^= %= *= /= += -= =  >> |  ";
    tksemi, tkcomma, tklbracket, tkrbracket, tklparen, tkrparen, tklbrace,
    tkrbrace, tkfield, tkincr, tkdecr, tkpow, tknot, tkmul, tkdiv, tkmod,
    tkplus, tkminus,
    tkcat, // !!! Fake operator for concatenation (just adjacent string exprs)
    tklt, tkle, tkne, tkeq, tkgt, tkge, tkmatchop, tknotmatch, tkand, tkor,
    tkternif, tkternelse, tkpowasgn, tkmodasgn, tkmulasgn, tkdivasgn,
    tkaddasgn, tksubasgn, tkasgn, tkappend, tkpipe,

// static char *keywords = " in        BEGIN     END       if        else      "
//    "while     for       do        break     continue  exit      function  "
//    "return    next      nextfile  delete    print     printf    getline   ";
    tkin, tkbegin, tkend, tkif, tkelse,
    tkwhile, tkfor, tkdo, tkbreak, tkcontinue, tkexit, tkfunction,
    tkreturn, tknext, tknextfile, tkdelete, tkprint, tkprintf, tkgetline,

// static char *builtins = " atan2     cos       sin       exp       "
//    "log       sqrt      int       rand      srand     length    "
//    "tolower   toupper   system    fflush    "
//    "and       or        xor       lshift    rshift    ";
    tkatan2, tkcos, tksin, tkexp, tklog, tksqrt, tkint, tkrand, tksrand,
    tklength, tktolower, tktoupper, tksystem, tkfflush,
    tkband, tkbor, tkbxor, tklshift, tkrshift,

// static char *specialfuncs = " close     index     match     split     "
//    "sub       gsub      sprintf   substr    ";
    tkclose, tkindex, tkmatch, tksplit,
    tksub, tkgsub, tksprintf, tksubstr, tklasttk
    };

enum opcodes {
    opunusedop = tklasttk,
    opvarref, opmapref, opfldref, oppush, opdrop, opdrop_n, opnotnot,
    oppreincr, oppredecr, oppostincr, oppostdecr, opnegate, opjump, opjumptrue,
    opjumpfalse, opprepcall, opmap, opmapiternext, opmapdelete, opmatchrec,
    opquit, opprintrec, oprange1, oprange2, oprange3, oplastop
};

// Special variables (POSIX). Must align with char *spec_vars[]
enum spec_var_names { ARGC=1, ARGV, CONVFMT, ENVIRON, FILENAME, FNR, FS, NF,
    NR, OFMT, OFS, ORS, RLENGTH, RS, RSTART, SUBSEP };

struct symtab_slot {    // global symbol table entry
  unsigned flags;
  char *name;
};

// zstring: flexible string type.
// Capacity must be > size because we insert a NUL byte.
struct zstring {
  int refcnt;
  unsigned size;
  unsigned capacity;
  char str[];   // C99 flexible array member
};

// Flag bits for zvalue and symbol tables
#define ZF_MAYBEMAP (1u << 1)
#define ZF_MAP      (1u << 2)
#define ZF_SCALAR   (1u << 3)
#define ZF_NUM      (1u << 4)
#define ZF_RX       (1u << 5)
#define ZF_STR      (1u << 6)
#define ZF_NUMSTR   (1u << 7)   // "numeric string" per posix
#define ZF_REF      (1u << 9)   // for lvalues
#define ZF_MAPREF   (1u << 10)  // for lvalues
#define ZF_FIELDREF (1u << 11)  // for lvalues
#define ZF_EMPTY_RX (1u << 12)
#define ZF_ANYMAP   (ZF_MAP | ZF_MAYBEMAP)

// Macro to help facilitate possible future change in zvalue layout.
#define ZVINIT(flags, num, ptr) {(flags), (double)(num), {(ptr)}}

#define IS_STR(zvalp) ((zvalp)->flags & ZF_STR)
#define IS_RX(zvalp) ((zvalp)->flags & ZF_RX)
#define IS_NUM(zvalp) ((zvalp)->flags & ZF_NUM)
#define IS_MAP(zvalp) ((zvalp)->flags & ZF_MAP)
#define IS_EMPTY_RX(zvalp) ((zvalp)->flags & ZF_EMPTY_RX)

#define GLOBAL      ((struct symtab_slot *)TT.globals_table.base)
#define LOCAL       ((struct symtab_slot *)TT.locals_table.base)
#define FUNC_DEF    ((struct functab_slot *)TT.func_def_table.base)

#define LITERAL     ((struct zvalue *)TT.literals.base)
#define STACK       ((struct zvalue *)TT.stack.base)
#define FIELD       ((struct zvalue *)TT.fields.base)

#define ZCODE       ((int *)TT.zcode.base)

#define FUNC_DEFINED    (1u)
#define FUNC_CALLED     (2u)

#define MIN_STACK_LEFT 1024

struct functab_slot {    // function symbol table entry
  unsigned flags;
  char *name;
  struct zlist function_locals;
  int zcode_addr;
};

// Elements of the hash table (key/value pairs)
struct zmap_slot {
  int hash;       // store hash key to speed hash table expansion
  struct zstring *key;
  struct zvalue val;
};
#define ZMSLOTINIT(hash, key, val) {hash, key, val}

// zmap: Mapping data type for arrays; a hash table. Values in hash are either
// 0 (unused), -1 (marked deleted), or one plus the number of the zmap slot
// containing a key/value pair. The zlist slot entries are numbered from 0 to
// count-1, so need to add one to distinguish from unused.  The probe sequence
// is borrowed from Python dict, using the "perturb" idea to mix in upper bits
// of the original hash value.
struct zmap {
  unsigned mask;  // tablesize - 1; tablesize is 2 ** n
  int *hash;      // (mask + 1) elements
  int limit;      // 80% of table size ((mask+1)*8/10)
  int count;      // number of occupied slots in hash
  int deleted;    // number of deleted slots
  struct zlist slot;     // expanding list of zmap_slot elements
};

#define MAPSLOT    ((struct zmap_slot *)(m->slot).base)
#define FFATAL(format, ...) zzerr("$" format, __VA_ARGS__)
#define FATAL(...) zzerr("$%s\n", __VA_ARGS__)
#define XERR(format, ...) zzerr(format, __VA_ARGS__)

#define NO_EXIT_STATUS  (9999987)  // value unlikely to appear in exit stmt



////////////////////
//// lib
////////////////////

static void xfree(void *p)
{
  free(p);
}

static int hexval(int c)
{
  // Assumes c is valid hex digit
  return isdigit(c) ? c - '0' : (c | 040) - 'a' + 10;
}

////////////////////
//// common defs
////////////////////

// These (ops, keywords, builtins) must align with enum tokens
static char *ops = " ;  ,  [  ]  (  )  {  }  $  ++ -- ^  !  *  /  %  +  -  .. "
        "<  <= != == >  >= ~  !~ && || ?  :  ^= %= *= /= += -= =  >> |  ";

static char *keywords = " in        BEGIN     END       if        else      "
    "while     for       do        break     continue  exit      function  "
    "return    next      nextfile  delete    print     printf    getline   ";

static char *builtins = " atan2     cos       sin       exp       log       "
    "sqrt      int       rand      srand     length    "
    "tolower   toupper   system    fflush    "
    "and       or        xor       lshift    rshift    "
    "close     index     match     split     "
    "sub       gsub      sprintf   substr    ";

static void zzerr(char *format, ...)
{
  va_list args;
  int fatal_sw = 0;
  fprintf(stderr, "%s: ", TT.progname);
  if (format[0] == '$') {
    fprintf(stderr, "FATAL: ");
    format++;
    fatal_sw = 1;
  }
  fprintf(stderr, "file %s line %d: ", TT.scs->filename, TT.scs->line_num);
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  if (format[strlen(format)-1] != '\n') fputc('\n', stderr); // TEMP FIXME !!!
  fflush(stderr);
  if (fatal_sw) awk_exit(2);
        // Don't bump error count for warnings
  else if (!strstr(format, "arning")) TT.cgl.compile_error_count++;
}

static void get_token_text(char *op, int tk)
{
  // This MUST ? be changed if ops string or tk... assignments change!
  memmove(op, ops + 3 * (tk - tksemi) + 1, 2);
  op[ op[1] == ' ' ? 1 : 2 ] = 0;
}

////////////////////
/// UTF-8
////////////////////

// Return number of bytes in 'cnt' utf8 codepoints
static int bytesinutf8(char *str, size_t len, size_t cnt)
{
  if (FLAG(b)) return cnt;
  unsigned wch;
  char *lim = str + len, *s0 = str;
  while (cnt-- && str < lim) {
    int r = utf8towc(&wch, str, lim - str);
    str += r > 0 ? r : 1;
  }
  return str - s0;
}

// Return number of utf8 codepoints in str
static int utf8cnt(char *str, size_t len)
{
  unsigned wch;
  int cnt = 0;
  char *lim;
  if (!len || FLAG(b)) return len;
  for (lim = str + len; str < lim; cnt++) {
    int r = utf8towc(&wch, str, lim - str);
    str += r > 0 ? r : 1;
  }
  return cnt;
}

////////////////////
////   zlist
////////////////////

static struct zlist *zlist_initx(struct zlist *p, size_t size, size_t count)
{
  p->base = p->avail = xzalloc(count * size);
  p->limit = p->base + size * count;
  p->size = size;
  return p;
}

static struct zlist *zlist_init(struct zlist *p, size_t size)
{
#define SLIST_MAX_INIT_BYTES 128
  return zlist_initx(p, size, SLIST_MAX_INIT_BYTES / size);
}

// This is called from zlist_append() and add_stack() in run
static void zlist_expand(struct zlist *p)
{
  size_t offset = p->avail - p->base;
  size_t cap = p->limit - p->base;
  size_t newcap = maxof(cap + p->size, ((cap / p->size) * 3 / 2) * p->size);
  if (newcap <= cap) error_exit("mem req error");
  char *base = xrealloc(p->base, newcap);
  p->base = base;
  p->limit = base + newcap;
  p->avail = base + offset;
}

static size_t zlist_append(struct zlist *p, void *obj)
{
  // Insert obj (p->size bytes) at end of list, expand as needed.
  // Return scaled offset to newly inserted obj; i.e. the
  // "slot number" 0, 1, 2,...
  void *objtemp = 0;
  if (p->avail > p->limit - p->size) {
    objtemp = xmalloc(p->size);     // Copy obj in case it is in
    memmove(objtemp, obj, p->size); // the area realloc might free!
    obj = objtemp;
    zlist_expand(p);
  }
  memmove(p->avail, obj, p->size);
  if (objtemp) xfree(objtemp);
  p->avail += p->size;
  return (p->avail - p->base - p->size) / p->size;  // offset of updated slot
}

static int zlist_len(struct zlist *p)
{
  return (p->avail - p->base) / p->size;
}

////////////////////
////   zstring
////////////////////

static void zstring_release(struct zstring **s)
{
  if (*s && (**s).refcnt-- == 0) xfree(*s); //free_zstring(s);
  *s = 0;
}

static void zstring_incr_refcnt(struct zstring *s)
{
  if (s) s->refcnt++;
}

// !! Use only if 'to' is NULL or its refcnt is 0.
static struct zstring *zstring_modify(struct zstring *to, size_t at, char *s, size_t n)
{
  size_t cap = at + n + 1;
  if (!to || to->capacity < cap) {
    to = xrealloc(to, sizeof(*to) + cap);
    to->capacity = cap;
    to->refcnt = 0;
  }
  memcpy(to->str + at, s, n);
  to->size = at + n;
  to->str[to->size] = '\0';
  return to;
}

// The 'to' pointer may move by realloc, so return (maybe updated) pointer.
// If refcnt is nonzero then there is another pointer to this zstring,
// so copy this one and release it. If refcnt is zero we can mutate this.
static struct zstring *zstring_update(struct zstring *to, size_t at, char *s, size_t n)
{
  if (to && to->refcnt) {
    struct zstring *to_before = to;
    to = zstring_modify(0, 0, to->str, to->size);
    zstring_release(&to_before);
  }
  return zstring_modify(to, at, s, n);
}

static struct zstring *zstring_copy(struct zstring *to, struct zstring *from)
{
  return zstring_update(to, 0, from->str, from->size);
}

static struct zstring *zstring_extend(struct zstring *to, struct zstring *from)
{
  return zstring_update(to, to->size, from->str, from->size);
}

static struct zstring *new_zstring(char *s, size_t size)
{
  return zstring_modify(0, 0, s, size);
}

////////////////////
////   zvalue
////////////////////

static struct zvalue uninit_zvalue = ZVINIT(0, 0.0, 0);

// This will be reassigned in init_globals() with an empty string.
// It's a special value used for "uninitialized" field vars
// referenced past $NF. See push_field().
static struct zvalue uninit_string_zvalue = ZVINIT(0, 0.0, 0);

static struct zvalue new_str_val(char *s)
{
  // Only if no nul inside string!
  struct zvalue v = ZVINIT(ZF_STR, 0.0, new_zstring(s, strlen(s)));
  return v;
}

static void zvalue_release_zstring(struct zvalue *v)
{
  if (v && ! (v->flags & (ZF_ANYMAP | ZF_RX))) zstring_release(&v->vst);
}

// push_val() is used for initializing globals (see init_compiler())
// but mostly used in runtime
// WARNING: push_val may change location of v, so do NOT depend on it after!
// Note the incr refcnt used to be after the zlist_append, but that caused a
// heap-use-after-free error when the zlist_append relocated the zvalue being
// pushed, invalidating the v pointer.
static void push_val(struct zvalue *v)
{
  if (IS_STR(v) && v->vst) v->vst->refcnt++;  // inlined zstring_incr_refcnt()
  *++TT.stackp = *v;
}

static void zvalue_copy(struct zvalue *to, struct zvalue *from)
{
  if (IS_RX(from)) *to = *from;
  else {
    zvalue_release_zstring(to);
    *to = *from;
    zstring_incr_refcnt(to->vst);
  }
}

static void zvalue_dup_zstring(struct zvalue *v)
{
  struct zstring *z = new_zstring(v->vst->str, v->vst->size);
  zstring_release(&v->vst);
  v->vst = z;
}

////////////////////
////   zmap (array) implementation
////////////////////

static int zstring_match(struct zstring *a, struct zstring *b)
{
  return a->size == b->size && memcmp(a->str, b->str, a->size) == 0;
}

static int zstring_hash(struct zstring *s)
{   // djb2 -- small, fast, good enough for this
  unsigned h = 5381;
  char *p = s->str, *lim = p + s->size;
  while (p < lim)
    h = (h << 5) + h + *p++;
  return h;
}

enum { PSHIFT = 5 };  // "perturb" shift -- see find_mapslot() below

static struct zmap_slot *find_mapslot(struct zmap *m, struct zstring *key, int *hash, int *probe)
{
  struct zmap_slot *x = 0;
  unsigned perturb = *hash = zstring_hash(key);
  *probe = *hash & m->mask;
  int n, first_deleted = -1;
  while ((n = m->hash[*probe])) {
    if (n > 0) {
      x = &MAPSLOT[n-1];
      if (*hash == x->hash && zstring_match(key, x->key)) {
        return x;
      }
    } else if (first_deleted < 0) first_deleted = *probe;
    // Based on technique in Python dict implementation. Comment there
    // (https://github.com/python/cpython/blob/3.10/Objects/dictobject.c)
    // says
    //
    // j = ((5*j) + 1) mod 2**i
    // For any initial j in range(2**i), repeating that 2**i times generates
    // each int in range(2**i) exactly once (see any text on random-number
    // generation for proof).
    //
    // The addition of 'perturb' greatly improves the probe sequence. See
    // the Python dict implementation for more details.
    *probe = (*probe * 5 + 1 + (perturb >>= PSHIFT)) & m->mask;
  }
  if (first_deleted >= 0) *probe = first_deleted;
  return 0;
}

static struct zvalue *zmap_find(struct zmap *m, struct zstring *key)
{
  int hash, probe;
  struct zmap_slot *x = find_mapslot(m, key, &hash, &probe);
  return x ? &x->val : 0;
}

static void zmap_init(struct zmap *m)
{
  enum {INIT_SIZE = 8};
  m->mask = INIT_SIZE - 1;
  m->hash = xzalloc(INIT_SIZE * sizeof(*m->hash));
  m->limit = INIT_SIZE * 8 / 10;
  m->count = 0;
  m->deleted = 0;
  zlist_init(&m->slot, sizeof(struct zmap_slot));
}

static void zvalue_map_init(struct zvalue *v)
{
  struct zmap *m = xmalloc(sizeof(*m));
  zmap_init(m);
  v->map = m;
  v->flags |= ZF_MAP;
}

static void zmap_delete_map_incl_slotdata(struct zmap *m)
{
  for (struct zmap_slot *p = &MAPSLOT[0]; p < &MAPSLOT[zlist_len(&m->slot)]; p++) {
    if (p->key) zstring_release(&p->key);
    if (p->val.vst) zstring_release(&p->val.vst);
  }
  xfree(m->slot.base);
  xfree(m->hash);
}

static void zmap_delete_map(struct zmap *m)
{
  zmap_delete_map_incl_slotdata(m);
  zmap_init(m);
}

static void zmap_rehash(struct zmap *m)
{
  // New table is twice the size of old.
  int size = m->mask + 1;
  unsigned mask = 2 * size - 1;
  int *h = xzalloc(2 * size * sizeof(*m->hash));
  // Step through the old hash table, set up location in new table.
  for (int i = 0; i < size; i++) {
    int n = m->hash[i];
    if (n > 0) {
      int hash = MAPSLOT[n-1].hash;
      unsigned perturb = hash;
      int p = hash & mask;
      while (h[p]) {
        p = (p * 5 + 1 + (perturb >>= PSHIFT)) & mask;
      }
      h[p] = n;
    }
  }
  m->mask = mask;
  xfree(m->hash);
  m->hash = h;
  m->limit = 2 * size * 8 / 10;
}

static struct zmap_slot *zmap_find_or_insert_key(struct zmap *m, struct zstring *key)
{
  int hash, probe;
  struct zmap_slot *x = find_mapslot(m, key, &hash, &probe);
  if (x) return x;
  // not found; insert it.
  if (m->count == m->limit) {
    zmap_rehash(m);         // rehash if getting too full.
    // rerun find_mapslot to get new probe index
    x = find_mapslot(m, key, &hash, &probe);
  }
  // Assign key to new slot entry and bump refcnt.
  struct zmap_slot zs = ZMSLOTINIT(hash, key, (struct zvalue)ZVINIT(0, 0.0, 0));
  zstring_incr_refcnt(key);
  int n = zlist_append(&m->slot, &zs);
  m->count++;
  m->hash[probe] = n + 1;
  return &MAPSLOT[n];
}

static void zmap_delete(struct zmap *m, struct zstring *key)
{
  int hash, probe;
  struct zmap_slot *x = find_mapslot(m, key, &hash, &probe);
  if (!x) return;
  zstring_release(&MAPSLOT[m->hash[probe] - 1].key);
  m->hash[probe] = -1;
  m->deleted++;
}

////////////////////
//// scan (lexical analyzer)
////////////////////

// TODO:
// IS line_num getting incr correctly? Newline counts as start of line!?
// Handle nuls in file better.
// Open files "rb" and handle CRs in program.
// Roll gch() into get_char() ?
// Deal with signed char (at EOF? elsewhere?)
//
// 2023-01-11: Allow nul bytes inside strings? regexes?

static void progfile_open(void)
{
  TT.scs->filename = TT.scs->prog_args->arg;
  TT.scs->prog_args = TT.scs->prog_args->next;
  TT.scs->fp = stdin;
  if (strcmp(TT.scs->filename, "-")) TT.scs->fp = fopen(TT.scs->filename, "r");
  if (!TT.scs->fp) error_exit("Can't open %s", TT.scs->filename);
  TT.scs->line_num = 0;
}

static int get_char(void)
{
  static char *nl = "\n";
  // On first entry, TT.scs->p points to progstring if any, or null string.
  for (;;) {
    int c = *(TT.scs->p)++;
    if (c) {
      return c;
    }
    if (TT.scs->progstring) {  // Fake newline at end of progstring.
      if (TT.scs->progstring == nl) return EOF;
      TT.scs->p = TT.scs->progstring = nl;
      continue;
    }
    // Here if getting from progfile(s).
    if (TT.scs->line == nl) return EOF;
    if (!TT.scs->fp) {
      progfile_open();
    }
    // Save last char to allow faking final newline.
    int lastchar = (TT.scs->p)[-2];
    TT.scs->line_len = getline(&TT.scs->line, &TT.scs->line_size, TT.scs->fp);
    if (TT.scs->line_len > 0) {
      TT.scs->line_num++;
      TT.scs->p = TT.scs->line;
      continue;
    }
    // EOF
    // FIXME TODO or check for error? feof() vs. ferror()
    fclose(TT.scs->fp);
    TT.scs->fp = 0;
    TT.scs->p = "  " + 2;
    if (!TT.scs->prog_args) {
      xfree(TT.scs->line);
      if (lastchar == '\n') return EOF;
      // Fake final newline
      TT.scs->line = TT.scs->p = nl;
    }
  }
}

static void append_this_char(int c)
{
  if (TT.scs->toklen == TT.scs->maxtok - 1) {
    TT.scs->maxtok *= 2;
    TT.scs->tokstr = xrealloc(TT.scs->tokstr, TT.scs->maxtok);
  }
  TT.scs->tokstr[TT.scs->toklen++] = c;
  TT.scs->tokstr[TT.scs->toklen] = 0;
}

static void gch(void)
{
  // FIXME probably not right place to skip CRs.
  do {
    TT.scs->ch = get_char();
  } while (TT.scs->ch == '\r');
}

static void append_char(void)
{
  append_this_char(TT.scs->ch);
  gch();
}

static int find_keyword_or_builtin(char *table,
    int first_tok_in_table)
{
  char s[16] = " ", *p;
  // keywords and builtin functions are spaced 10 apart for strstr() lookup,
  // so must be less than that long.
  if (TT.scs->toklen >= 10) return 0;
  strcat(s, TT.scs->tokstr);
  strcat(s, " ");
  p = strstr(table, s);
  if (!p) return 0;
  return first_tok_in_table + (p - table) / 10;
}

static int find_token(void)
{
  char s[6] = " ", *p;
  // tokens are spaced 3 apart for strstr() lookup, so must be less than
  // that long.
  strcat(s, TT.scs->tokstr);
  strcat(s, " ");
  p = strstr(ops, s);
  if (!p) return 0;
  return tksemi + (p - ops) / 3;
}

static int find_keyword(void)
{
  return find_keyword_or_builtin(keywords, tkin);
}

static int find_builtin(void)
{
  return find_keyword_or_builtin(builtins, tkatan2);
}

static void get_number(void)
{
  // Assumes TT.scs->ch is digit or dot on entry.
  // TT.scs->p points to the following character.
  // OK formats: 1 1. 1.2 1.2E3 1.2E+3 1.2E-3 1.E2 1.E+2 1.E-2 1E2 .1 .1E2
  // .1E+2 .1E-2
  // NOT OK: . .E .E1 .E+ .E+1 ; 1E .1E 1.E 1.E+ 1.E- parse as number
  // followed by variable E.
  // gawk accepts 12.E+ and 12.E- as 12; nawk & mawk say syntax error.
  char *leftover;
  int len;
  TT.scs->numval = strtod(TT.scs->p - 1, &leftover);
  len = leftover - TT.scs->p + 1;
  if (len == 0) {
    append_char();
    TT.scs->toktype = ERROR;
    TT.scs->tok = tkerr;
    TT.scs->error = 1;
    FFATAL("Unexpected token '%s'\n", TT.scs->tokstr);
    return;
  }
  while (len--)
    append_char();
}

static void get_string_or_regex(int endchar)
{
  gch();
  while (TT.scs->ch != endchar) {
    if (TT.scs->ch == '\n') {
      // FIXME Handle unterminated string or regex. Is this OK?
      // FIXME TODO better diagnostic here?
      XERR("%s\n", "unterminated string or regex");
      break;
    } else if (TT.scs->ch == '\\') {
      // \\ \a \b \f \n \r \t \v \" \/ \ddd
      char *p, *escapes = "\\abfnrtv\"/";
      gch();
      if (TT.scs->ch == '\n') {  // backslash newline is continuation
        gch();
        continue;
      } else if ((p = strchr(escapes, TT.scs->ch))) {
        // posix regex does not use these escapes,
        // but awk does, so do them.
        int c = "\\\a\b\f\n\r\t\v\"/"[p-escapes];
        append_this_char(c);
        // Need to double up \ inside literal regex
        if (endchar == '/' && c == '\\') append_this_char('\\');
        gch();
      } else if (TT.scs->ch == 'x') {
        gch();
        if (isxdigit(TT.scs->ch)) {
          int c = hexval(TT.scs->ch);
          gch();
          if (isxdigit(TT.scs->ch)) {
            c = c * 16 + hexval(TT.scs->ch);
            gch();
          }
          append_this_char(c);
        } else append_this_char('x');
      } else if (TT.scs->ch == 'u') {
        gch();
        if (isxdigit(TT.scs->ch)) {
          int i = 0, j = 0, c = 0;
          char codep[9] = {0};
          do {
            codep[j++] = TT.scs->ch;
            gch();
          } while (j < 8 && isxdigit(TT.scs->ch));
          c = strtol(codep, 0, 16);
          for (i = wctoutf8(codep, c), j = 0; j < i; j++)
            append_this_char(codep[j]);
        } else append_this_char('u');
      } else if (isdigit(TT.scs->ch)) {
        if (TT.scs->ch < '8') {
          int k, c = 0;
          for (k = 0; k < 3; k++) {
            if (isdigit(TT.scs->ch) && TT.scs->ch < '8') {
              c = c * 8 + TT.scs->ch - '0';
              gch();
            } else
              break;
          }
          append_this_char(c);
        } else {
          append_char();
        }
      } else {
        if (endchar == '/') {
          // pass \ unmolested if not awk escape,
          // so that regex routines can see it.
          if (!strchr(".[]()*+?{}|^$-", TT.scs->ch)) {
            XERR("warning: '\\%c' -- unknown regex escape\n", TT.scs->ch);
          }
          append_this_char('\\');
        } else {
          XERR("warning: '\\%c' treated as plain '%c'\n", TT.scs->ch, TT.scs->ch);
        }
      }
    } else if (TT.scs->ch == EOF) {
      FATAL("EOF in string or regex\n");
    } else {
      append_char();
    }
  }
  gch();
}

static void ascan_opt_div(int div_op_allowed_here)
{
  int n;
  for (;;) {
    TT.scs->tokbuiltin = 0;
    TT.scs->toklen = 0;
    TT.scs->tokstr[0] = 0;
    while (TT.scs->ch == ' ' || TT.scs->ch == '\t')
      gch();
    if (TT.scs->ch == '\\') {
      append_char();
      if (TT.scs->ch == '\n') {
        gch();
        continue;
      }
      TT.scs->toktype = ERROR;   // \ not last char in line.
      TT.scs->tok = tkerr;
      TT.scs->error = 3;
      FATAL("backslash not last char in line\n");
      return;
    }
    break;
  }
  // Note \<NEWLINE> in comment does not continue it.
  if (TT.scs->ch == '#') {
    gch();
    while (TT.scs->ch != '\n')
      gch();
    // Need to fall through here to pick up newline.
  }
  if (TT.scs->ch == '\n') {
    TT.scs->toktype = NEWLINE;
    TT.scs->tok = tknl;
    append_char();
  } else if (isalpha(TT.scs->ch) || TT.scs->ch == '_') {
    append_char();
    while (isalnum(TT.scs->ch) || TT.scs->ch == '_') {
      append_char();
    }
    if ((n = find_keyword()) != 0) {
      TT.scs->toktype = KEYWORD;
      TT.scs->tok = n;
    } else if ((n = find_builtin()) != 0) {
      TT.scs->toktype = BUILTIN;
      TT.scs->tok = tkbuiltin;
      TT.scs->tokbuiltin = n;
    } else if (TT.scs->ch == '(') {
      TT.scs->toktype = USERFUNC;
      TT.scs->tok = tkfunc;
    } else {
      TT.scs->toktype = VAR;
      TT.scs->tok = tkvar;
      // skip whitespace to be able to check for , or )
      while (TT.scs->ch == ' ' || TT.scs->ch == '\t')
        gch();
    }
    return;
  } else if (TT.scs->ch == '"') {
    TT.scs->toktype = STRING;
    TT.scs->tok = tkstring;
    get_string_or_regex('"');
  } else if (isdigit(TT.scs->ch) || TT.scs->ch == '.') {
    TT.scs->toktype = NUMBER;
    TT.scs->tok = tknumber;
    get_number();
  } else if (TT.scs->ch == '/' && ! div_op_allowed_here) {
    TT.scs->toktype = REGEX;
    TT.scs->tok = tkregex;
    get_string_or_regex('/');
  } else if (TT.scs->ch == EOF) {
    TT.scs->toktype = EOF;
    TT.scs->tok = tkeof;
  } else if (TT.scs->ch == '\0') {
    append_char();
    TT.scs->toktype = ERROR;
    TT.scs->tok = tkerr;
    TT.scs->error = 5;
    FATAL("null char\n");
  } else {
    // All other tokens.
    TT.scs->toktype = TT.scs->ch;
    append_char();
    // Special case for **= and ** tokens
    if (TT.scs->toktype == '*' && TT.scs->ch == '*') {
      append_char();
      if (TT.scs->ch == '=') {
        append_char();
        TT.scs->tok = tkpowasgn;
      } else TT.scs->tok = tkpow;
      TT.scs->toktype = TT.scs->tok + 200;
      return;
    }
    // Is it a 2-character token?
    if (TT.scs->ch != ' ' && TT.scs->ch != '\n') {
      append_this_char(TT.scs->ch);
      if (find_token()) {
        TT.scs->tok = find_token();
        TT.scs->toktype = TT.scs->tok + 200;
        gch();  // Eat second char of token.
        return;
      }
      TT.scs->toklen--;  // Not 2-character token; back off.
      TT.scs->tokstr[TT.scs->toklen] = 0;
    }
    TT.scs->tok = find_token();
    if (TT.scs->tok) return;
    TT.scs->toktype = ERROR;
    TT.scs->tok = tkerr;
    TT.scs->error = 4;
    FFATAL("Unexpected token '%s'\n", TT.scs->tokstr);
  }
}

static void scan_opt_div(int div_op_allowed_here)
{
  // TODO FIXME need better diags for bad tokens!
  // TODO Also set global syntax error flag.
  do ascan_opt_div(div_op_allowed_here); while (TT.scs->tok == tkerr);
}

static void init_scanner(void)
{
  TT.prevtok = tkeof;
  gch();
}

// POSIX says '/' does not begin a regex wherever '/' or '/=' can mean divide.
// Pretty sure if / or /= comes after these, it means divide:
static char div_preceders[] = {tknumber, tkstring, tkvar, tkgetline, tkrparen, tkrbracket, tkincr, tkdecr, 0};

// For checking end of prev statement for termination and if '/' can come next

static void scan(void)
{
  TT.prevtok = TT.scs->tok;
  if (TT.prevtok && strchr(div_preceders, TT.prevtok)) scan_opt_div(1);
  else scan_opt_div(0);
  TT.tokstr = TT.scs->tokstr;
}

////////////////////
//// compile
////////////////////

//  NOTES:
//  NL ok after , { && || do else OR after right paren after if/while/for
//  TODO:
//    see case tkgetline -- test more
//    case tkmatchop, tknotmatch -- fix ~ (/re/)

// Forward declarations -- for mutually recursive parsing functions
static int expr(int rbp);
static void lvalue(void);
static int primary(void);
static void stmt(void);
static void action(int action_type);

#define CURTOK() (TT.scs->tok)
#define ISTOK(toknum) (TT.scs->tok == (toknum))

static int havetok(int tk)
{
  if (!ISTOK(tk)) return 0;
  scan();
  return 1;
}

//// code and "literal" emitters
static void gencd(int op)
{
  TT.zcode_last = zlist_append(&TT.zcode, &op);
}

static void gen2cd(int op, int n)
{
  gencd(op);
  gencd(n);
}

static int make_literal_str_val(char *s)
{
  // Only if no nul inside string!
  struct zvalue v = new_str_val(s);
  return zlist_append(&TT.literals, &v);
}

static int make_literal_regex_val(char *s)
{
  regex_t *rx;
  rx = xmalloc(sizeof(*rx));
  xregcomp(rx, s, REG_EXTENDED);
  struct zvalue v = ZVINIT(ZF_RX, 0, 0);
  v.rx = rx;
  // Flag empty rx to make it easy to identify for split() special case
  if (!*s) v.flags |= ZF_EMPTY_RX;
  return zlist_append(&TT.literals, &v);
}

static int make_literal_num_val(double num)
{
  struct zvalue v = ZVINIT(ZF_NUM, num, 0);
  return zlist_append(&TT.literals, &v);
}

static int make_uninit_val(void)
{
  return zlist_append(&TT.literals, &uninit_zvalue);
}
//// END code and "literal" emitters

//// Symbol tables functions
static int find_func_def_entry(char *s)
{
  for (int k = 1; k < zlist_len(&TT.func_def_table); k++)
    if (!strcmp(s, FUNC_DEF[k].name)) return k;
  return 0;
}

static int add_func_def_entry(char *s)
{
  struct functab_slot ent = {0, 0, {0, 0, 0, 0}, 0};
  ent.name = xstrdup(s);
  int slotnum = zlist_append(&TT.func_def_table, &ent);
  return slotnum;
}

static int find_global(char *s)
{
  for (int k = 1; k < zlist_len(&TT.globals_table); k++)
    if (!strcmp(s, GLOBAL[k].name)) return k;
  return 0;
}

static int add_global(char *s)
{
  struct symtab_slot ent = {0, 0};
  ent.name = xstrdup(s);
  int slotnum = zlist_append(&TT.globals_table, &ent);
  return slotnum;
}

static int find_local_entry(char *s)
{
  for (int k = 1; k < zlist_len(&TT.locals_table); k++)
    if (!strcmp(s, LOCAL[k].name)) return k;
  return 0;
}

static int add_local_entry(char *s)
{
  struct symtab_slot ent = {0, 0};
  ent.name = xstrdup(s);
  int slotnum = zlist_append(&TT.locals_table, &ent);
  return slotnum;
}

static int find_or_add_var_name(void)
{
  int slotnum = 0;    // + means global; - means local to function
  int globals_ent = 0;
  int locals_ent = find_local_entry(TT.tokstr);   // in local symbol table?
  if (locals_ent) {
    slotnum = -locals_ent;
  } else {
    globals_ent = find_global(TT.tokstr);
    if (!globals_ent) globals_ent = add_global(TT.tokstr);
    slotnum = globals_ent;
    if (find_func_def_entry(TT.tokstr))
      // POSIX: The same name shall not be used both as a variable name
      // with global scope and as the name of a function.
      XERR("var '%s' used as function name\n", TT.tokstr);
  }
  return slotnum;
}

//// END Symbol tables functions

//// Initialization
static void init_locals_table(void)
{
  static struct symtab_slot locals_ent;
  zlist_init(&TT.locals_table, sizeof(struct symtab_slot));
  zlist_append(&TT.locals_table, &locals_ent);
}

static void init_tables(void)
{
  static struct symtab_slot global_ent;
  static struct functab_slot func_ent;

  // Append dummy elements in lists to force valid offsets nonzero.
  zlist_init(&TT.globals_table, sizeof(struct symtab_slot));
  zlist_append(&TT.globals_table, &global_ent);
  zlist_init(&TT.func_def_table, sizeof(struct functab_slot));
  zlist_append(&TT.func_def_table, &func_ent);
  init_locals_table();
  zlist_init(&TT.zcode, sizeof(int));
  gencd(tkeof);   // to ensure zcode offsets are non-zero
  zlist_init(&TT.literals, sizeof(struct zvalue));
  // Init stack size at twice MIN_STACK_LEFT. MIN_STACK_LEFT is at least as
  // many entries as any statement may ever take.  Currently there is no diag
  // if this is exceeded; prog. will probably crash. 1024 should be plenty?
  zlist_initx(&TT.stack, sizeof(struct zvalue), 2 * MIN_STACK_LEFT);
  TT.stackp = (struct zvalue *)TT.stack.base;
  zlist_init(&TT.fields, sizeof(struct zvalue));
  zlist_append(&TT.literals, &uninit_zvalue);
  zlist_append(&TT.stack, &uninit_zvalue);
  zlist_append(&TT.fields, &uninit_zvalue);
  FIELD[0].vst = new_zstring("", 0);
}

static void init_compiler(void)
{
  // Special variables (POSIX). Must align with enum spec_var_names
  static char *spec_vars[] = { "ARGC", "ARGV", "CONVFMT", "ENVIRON", "FILENAME",
      "FNR", "FS", "NF", "NR", "OFMT", "OFS", "ORS", "RLENGTH", "RS", "RSTART",
      "SUBSEP", 0};

  init_tables();
  for (int k = 0; spec_vars[k]; k++) {
    TT.spec_var_limit = add_global(spec_vars[k]);
    GLOBAL[TT.spec_var_limit++].flags |= (k == 1 || k == 3) ? ZF_MAP : ZF_SCALAR;
    push_val(&uninit_zvalue);
  }
}
//// END Initialization

//// Parsing and compiling to TT.zcode
// Left binding powers
static int lbp_table[] = {  // Must align with enum Toks
  0, 0, 0, 0,     // tkunusedtoken, tkeof, tkerr, tknl,
  250, 250, 250,  // tkvar, tknumber, tkstring,
  250, 250, 250,  // tkregex, tkfunc, tkbuiltin,
  0, 0, 210, 0, // tksemi, tkcomma, tklbracket, tkrbracket,
  200, 0, 0, 0, // tklparen, tkrparen, tklbrace, tkrbrace,
  190, 180, 180, 170, 160, // tkfield, tkincr, tkdecr, tkpow, tknot,
  150, 150, 150, 140, 140, // tkmul, tkdiv, tkmod, tkplus, tkminus,
  130, // tkcat, // FAKE (?) optor for concatenation (adjacent string exprs)
  110, 110, 110, 110, 110, 110, // tklt, tkle, tkne, tkeq, tkgt, tkge,
  100, 100, // tkmatchop, tknotmatch,
  80, 70, // tkand, tkor,
  60, 0, // tkternif, tkternelse,
  50, 50, 50, 50,   // tkpowasgn, tkmodasgn, tkmulasgn, tkdivasgn,
  50, 50, 50, // tkaddasgn, tksubasgn, tkasgn,
  0, 120, // tkappend, tkpipe,
  90 // tkin
};

static int getlbp(int tok)
{
  // FIXME: should tkappend be here too? is tkpipe needed?
  // In print statement outside parens: make '>' end an expression
  if (TT.cgl.in_print_stmt && ! TT.cgl.paren_level && (tok == tkgt || tok == tkpipe))
    return 0;
  return (0 <= tok && tok <= tkin) ? lbp_table[tok] :
    // getline is special, not a normal builtin.
    // close, index, match, split, sub, gsub, sprintf, substr
    // are really builtin functions though bwk treats them as keywords.
    (tkgetline <= tok && tok <= tksubstr) ? 240 : 0;     // FIXME 240 is temp?
}

// Get right binding power. Same as left except for right associative optors
static int getrbp(int tok)
{
  int lbp = getlbp(tok);
  // ternary (?:), assignment, power ops are right associative
  return (lbp <= 60 || lbp == 170) ? lbp - 1 : lbp;
}

static void unexpected_eof(void)
{
  error_exit("terminated with error(s)");
}

//// syntax error diagnostic and recovery (Turner's method)
// D.A. Turner, Error diagnosis and recovery in one pass compilers,
// Information Processing Letters, Volume 6, Issue 4, 1977, Pages 113-115
static int recovering = 0;

static void complain(int tk)
{
  char op[3], tkstr[10];
  if (recovering) return;
  recovering = 1;
  if (!strcmp(TT.tokstr, "\n")) TT.tokstr = "<newline>";
  if (tksemi <= tk && tk <= tkpipe) {
    get_token_text(op, tk);
    XERR("syntax near '%s' -- '%s' expected\n", TT.tokstr, op);
  } else if (tk >= tkin && tk <= tksubstr) {
    if (tk < tkatan2) memmove(tkstr, keywords + 1 + 10 * (tk - tkin), 10);
    else memmove(tkstr, builtins + 1 + 10 * (tk - tkatan2), 10);
    *strchr(tkstr, ' ') = 0;
    XERR("syntax near '%s' -- '%s' expected\n", TT.tokstr, tkstr);
  } else XERR("syntax near '%s'\n", TT.tokstr);
}

static void expect(int tk)
{
  if (recovering) {
    while (!ISTOK(tkeof) && !ISTOK(tk))
      scan();
    if (ISTOK(tkeof)) unexpected_eof();
    scan(); // consume expected token
    recovering = 0;
  } else if (!havetok(tk)) complain(tk);
}

static void skip_to(char *tklist)
{
  do scan(); while (!ISTOK(tkeof) && !strchr(tklist, CURTOK()));
  if (ISTOK(tkeof)) unexpected_eof();
}

//// END syntax error diagnostic and recovery (Turner's method)

static void optional_nl_or_semi(void)
{
  while (havetok(tknl) || havetok(tksemi))
    ;
}

static void optional_nl(void)
{
  while (havetok(tknl))
    ;
}

static void rparen(void)
{
  expect(tkrparen);
  optional_nl();
}

static int have_comma(void)
{
  if (!havetok(tkcomma)) return 0;
  optional_nl();
  return 1;
}

static void check_set_map(int slotnum)
{
  // POSIX: The same name shall not be used within the same scope both as
  // a scalar variable and as an array.
  if (slotnum < 0 && LOCAL[-slotnum].flags & ZF_SCALAR)
    XERR("scalar param '%s' used as array\n", LOCAL[-slotnum].name);
  if (slotnum > 0 && GLOBAL[slotnum].flags & ZF_SCALAR)
    XERR("scalar var '%s' used as array\n", GLOBAL[slotnum].name);
  if (slotnum < 0) LOCAL[-slotnum].flags |= ZF_MAP;
  if (slotnum > 0) GLOBAL[slotnum].flags |= ZF_MAP;
}

static void check_set_scalar(int slotnum)
{
  if (slotnum < 0 && LOCAL[-slotnum].flags & ZF_MAP)
    XERR("array param '%s' used as scalar\n", LOCAL[-slotnum].name);
  if (slotnum > 0 && GLOBAL[slotnum].flags & ZF_MAP)
    XERR("array var '%s' used as scalar\n", GLOBAL[slotnum].name);
  if (slotnum < 0) LOCAL[-slotnum].flags |= ZF_SCALAR;
  if (slotnum > 0) GLOBAL[slotnum].flags |= ZF_SCALAR;
}

static void map_name(void)
{
  int slotnum;
  check_set_map(slotnum = find_or_add_var_name());
  gen2cd(tkvar, slotnum);
}

static void check_builtin_arg_counts(int tk, int num_args, char *fname)
{
  static char builtin_1_arg[] = { tkcos, tksin, tkexp, tklog, tksqrt, tkint,
                                  tktolower, tktoupper, tkclose, tksystem, 0};
  static char builtin_2_arg[] = { tkatan2, tkmatch, tkindex, tklshift, tkrshift, 0};
  static char builtin_al_2_arg[] = { tkband, tkbor, tkbxor, 0};
  static char builtin_2_3_arg[] = { tksub, tkgsub, tksplit, tksubstr, 0};
  static char builtin_0_1_arg[] = { tksrand, tklength, tkfflush, 0};

  if (tk == tkrand && num_args)
    XERR("function '%s' expected no args, got %d\n", fname, num_args);
  else if (strchr(builtin_1_arg, tk) && num_args != 1)
    XERR("function '%s' expected 1 arg, got %d\n", fname, num_args);
  else if (strchr(builtin_2_arg, tk) && num_args != 2)
    XERR("function '%s' expected 2 args, got %d\n", fname, num_args);
  else if (strchr(builtin_al_2_arg, tk) && num_args < 2)
    XERR("function '%s' expected at least 2 args, got %d\n", fname, num_args);
  else if (strchr(builtin_2_3_arg, tk) && num_args != 2 && num_args != 3)
    XERR("function '%s' expected 2 or 3 args, got %d\n", fname, num_args);
  else if (strchr(builtin_0_1_arg, tk) && num_args != 0 && num_args != 1)
    XERR("function '%s' expected no arg or 1 arg, got %d\n", fname, num_args);
}

static void builtin_call(int tk, char *builtin_name)
{
  int num_args = 0;
  expect(tklparen);
  TT.cgl.paren_level++;
  switch (tk) {
    case tksub:
    case tkgsub:
      if (ISTOK(tkregex)) {
        gen2cd(tkregex, make_literal_regex_val(TT.tokstr));
        scan();
      } else expr(0);
      expect(tkcomma);
      optional_nl();
      expr(0);
      if (have_comma()) {
        lvalue();
      } else {
        gen2cd(tknumber, make_literal_num_val(0));
        gen2cd(opfldref, tkeof);
      }
      num_args = 3;
      break;

    case tkmatch:
      expr(0);
      expect(tkcomma);
      optional_nl();
      if (ISTOK(tkregex)) {
        gen2cd(tkregex, make_literal_regex_val(TT.tokstr));
        scan();
      } else expr(0);
      num_args = 2;
      break;

    case tksplit:
      expr(0);
      expect(tkcomma);
      optional_nl();
      if (ISTOK(tkvar) && (TT.scs->ch == ',' || TT.scs->ch == ')')) {
        map_name();
        scan();
      } else {
        XERR("%s\n", "expected array name as split() 2nd arg");
        expr(0);
      }
      // FIXME some recovery needed here!?
      num_args = 2;
      if (have_comma()) {
        if (ISTOK(tkregex)) {
          gen2cd(tkregex, make_literal_regex_val(TT.tokstr));
          scan();
        } else expr(0);
        num_args++;
      }
      break;

    case tklength:
      if (ISTOK(tkvar) && (TT.scs->ch == ',' || TT.scs->ch == ')')) {
        gen2cd(tkvar, find_or_add_var_name());
        scan();
        num_args++;
      }
      ATTR_FALLTHROUGH_INTENDED;

    default:
      if (ISTOK(tkrparen)) break;
      do {
        expr(0);
        num_args++;
      } while (have_comma());
      break;
  }
  expect(tkrparen);
  TT.cgl.paren_level--;

  check_builtin_arg_counts(tk, num_args, builtin_name);

  gen2cd(tk, num_args);
}

static void function_call(void)
{
  // Function call: generate TT.zcode to:
  //  push placeholder for return value, push placeholder for return addr,
  //  push args, then push number of args, then:
  //      for builtins: gen opcode (e.g. tkgsub)
  //      for user func: gen (tkfunc, number-of-args)
  int functk = 0, funcnum = 0;
  char builtin_name[16];  // be sure it's long enough for all builtins
  if (ISTOK(tkbuiltin)) {
    functk = TT.scs->tokbuiltin;
    strcpy(builtin_name, TT.tokstr);
  } else if (ISTOK(tkfunc)) { // user function
    funcnum = find_func_def_entry(TT.tokstr);
    if (!funcnum) funcnum = add_func_def_entry(TT.tokstr);
    FUNC_DEF[funcnum].flags |= FUNC_CALLED;
    gen2cd(opprepcall, funcnum);
  } else error_exit("bad function %s!", TT.tokstr);
  scan();
  // length() can appear without parens
  int num_args = 0;
  if (functk == tklength && !ISTOK(tklparen)) {
    gen2cd(functk, 0);
    return;
  }
  if (functk) {   // builtin
    builtin_call(functk, builtin_name);
    return;
  }
  expect(tklparen);
  TT.cgl.paren_level++;
  if (ISTOK(tkrparen)) {
    scan();
  } else {
    do {
      if (ISTOK(tkvar) && (TT.scs->ch == ',' || TT.scs->ch == ')')) {
        // Function call arg that is a lone variable. Cannot tell in this
        // context if it is a scalar or map. Just add it to symbol table.
        gen2cd(tkvar, find_or_add_var_name());
        scan();
      } else expr(0);
      num_args++;
    } while (have_comma());
    expect(tkrparen);
  }
  TT.cgl.paren_level--;
  gen2cd(tkfunc, num_args);
}

static void var(void)
{
  // var name is in TT.tokstr
  // slotnum: + means global; - means local to function
  int slotnum = find_or_add_var_name();
  scan();
  if (havetok(tklbracket)) {
    check_set_map(slotnum);
    int num_subscripts = 0;
    do {
      expr(0);
      num_subscripts++;
    } while (have_comma());
    expect(tkrbracket);
    if (num_subscripts > 1) gen2cd(tkrbracket, num_subscripts);
    gen2cd(opmap, slotnum);
  } else {
    check_set_scalar(slotnum);
    gen2cd(tkvar, slotnum);
  }
}

//   Dollar $ tkfield can be followed by "any" expresson, but
//   the way it binds varies.
//   The following are valid lvalues:
//   $ ( expr )
//   $ tkvar $ tknumber $ tkstring $ tkregex
//   $ tkfunc(...)
//   $ tkbuiltin(...)
//   $ length   # with no parens after
//   $ tkclose(), ... $ tksubstr
//   $ tkgetline FIXME TODO TEST THIS
//   $ ++ lvalue
//   $ -- lvalue
//   $ + expression_up_to_exponentiation (also -, ! prefix ops)
//   $ $ whatever_can_follow_and_bind_to_dollar
//
//     tkvar, tknumber, tkstring, tkregex, tkfunc, tkbuiltin, tkfield, tkminus,
//     tkplus, tknot, tkincr, tkdecr, tklparen, tkgetline,
//     tkclose, tkindex, tkmatch, tksplit, tksub, tkgsub, tksprintf, tksubstr
//
// ray@radon:~$ awk 'BEGIN { $0 = "7 9 5 8"; k=2; print $k*k }'
// 18
// ray@radon:~$ awk 'BEGIN { $0 = "7 9 5 8"; k=2; print $+k*k }'
// 18
// ray@radon:~$ awk 'BEGIN { $0 = "7 9 5 8"; k=2; print $k^k }'
// 81
// ray@radon:~$ awk 'BEGIN { $0 = "7 9 5 8"; k=2; print $+k^k }'
// 8

static void field_op(void)
{
  // CURTOK() must be $ here.
  expect(tkfield);
  // tkvar, tknumber, tkstring, tkregex, tkfunc, tkbuiltin, tkfield, tkminus,
  // tkplus, tknot, tkincr, tkdecr, tklparen, tkgetline, tkclose, tkindex,
  // tkmatch, tksplit, tksub, tkgsub, tksprintf, tksubstr
  if (ISTOK(tkfield)) field_op();
  else if (ISTOK(tkvar)) var();
  else primary();
  // tkfield op has "dummy" 2nd word so that convert_push_to_reference(void)
  // can find either tkfield or tkvar at same place (ZCODE[TT.zcode_last-1]).
  gen2cd(tkfield, tkeof);
}

// Tokens that can start expression
static char exprstartsy[] = {tkvar, tknumber, tkstring, tkregex, tkfunc,
  tkbuiltin, tkfield, tkminus, tkplus, tknot, tkincr, tkdecr, tklparen,
  tkgetline, tkclose, tkindex, tkmatch, tksplit, tksub, tkgsub, tksprintf,
  tksubstr, tkband, tkbor, tkbxor, tkrshift, tklshift, 0};

// Tokens that can end statement
static char stmtendsy[] = {tknl, tksemi, tkrbrace, 0};

// Tokens that can follow expressions of a print statement
static char printexprendsy[] = {tkgt, tkappend, tkpipe, tknl, tksemi, tkrbrace, 0};

// !! Ensure this:
// ternary op is right associative, so
// a ? b : c ? d : e        evaluates as
// a ? b : (c ? d : e)      not as
// (a ? b : c) ? d : e

static void convert_push_to_reference(void)
{
  if (ZCODE[TT.zcode_last - 1] == tkvar) ZCODE[TT.zcode_last-1] = opvarref;
  else if (ZCODE[TT.zcode_last - 1] == opmap) ZCODE[TT.zcode_last - 1] = opmapref;
  else if (ZCODE[TT.zcode_last - 1] == tkfield) ZCODE[TT.zcode_last - 1] = opfldref;
  else error_exit("bad lvalue?");
}

static void lvalue(void)
{
  if (ISTOK(tkfield)) {
    field_op();
    convert_push_to_reference();
  } else if (ISTOK(tkvar)) {
    var();
    convert_push_to_reference();
  } else {
    XERR("syntax near '%s' (bad lvalue)\n", TT.tokstr);
  }
}

static int primary(void)
{
  //  On entry: CURTOK() is first token of expression
  //  On exit: CURTOK() is infix operator (for binary_op() to handle) or next
  //   token after end of expression.
  //  return -1 for field or var (potential lvalue);
  //      2 or more for comma-separated expr list
  //          as in "multiple subscript expression in array"
  //          e.g. (1, 2) in array_name, or a print/printf list;
  //      otherwise return 0
  //
  //  expr can start with:
  //      tkvar, tknumber, tkstring, tkregex, tkfunc, tkbuiltin, tkfield, tkminus,
  //      tkplus, tknot, tkincr, tkdecr, tklparen, tkgetline, tkclose, tkindex,
  //      tkmatch, tksplit, tksub, tkgsub, tksprintf, tksubstr
  //
  //  bwk treats these as keywords, not builtins: close index match split sub gsub
  //      sprintf substr
  //
  //  bwk builtins are: atan2 cos sin exp log sqrt int rand srand length tolower
  //      toupper system fflush
  //  NOTE: fflush() is NOT in POSIX awk
  //
  //  primary() must consume prefix and postfix operators as well as
  //      num, string, regex, var, var with subscripts, and function calls

  int num_exprs = 0;
  int nargs, modifier;
  int tok = CURTOK();
  switch (tok) {
    case tkvar:
    case tkfield:
      if (ISTOK(tkvar)) var();
      else field_op();
      if (ISTOK(tkincr) || ISTOK(tkdecr)) {
        convert_push_to_reference();
        gencd(CURTOK());
        scan();
      } else return -1;
      break;

    case tknumber:
      gen2cd(tknumber, make_literal_num_val(TT.scs->numval));
      scan();
      break;

    case tkstring:
      gen2cd(tkstring, make_literal_str_val(TT.tokstr));
      scan();
      break;

    case tkregex:
      // When an ERE token appears as an expression in any context other
      // than as the right-hand of the '~' or "!~" operator or as one of
      // the built-in function arguments described below, the value of
      // the resulting expression shall be the equivalent of: $0 ~ /ere/
      // FIXME TODO
      gen2cd(opmatchrec, make_literal_regex_val(TT.tokstr));
      scan();
      break;

    case tkbuiltin: // various builtins
    case tkfunc:    // user-defined function
      function_call();
      break;

    // Unary prefix ! + -
    case tknot:
    case tkminus:
    case tkplus:
      scan();
      expr(getlbp(tknot));   // unary +/- same precedence as !
      if (tok == tknot) gencd(tknot);
      else gencd(opnegate);               // forces to number
      if (tok == tkplus) gencd(opnegate); // forces to number
      break;

      // Unary prefix ++ -- MUST take lvalue
    case tkincr:
    case tkdecr:
      scan();
      lvalue();
      if (tok == tkincr) gencd(oppreincr);
      else gencd(oppredecr);
      break;

    case tklparen:
      scan();
      TT.cgl.paren_level++;
      num_exprs = 0;
      do {
        expr(0);
        num_exprs++;
      } while (have_comma());
      expect(tkrparen);
      TT.cgl.paren_level--;
      if (num_exprs > 1) return num_exprs;
      break;

    case tkgetline:
      // getline may be (according to awk book):
      // getline [var [<file]]
      // getline <file
      // cmd | getline [var]
      // var must be lvalue (can be any lvalue?)
      scan();
      nargs = 0;
      modifier = tkeof;
      if (ISTOK(tkfield) || ISTOK(tkvar)) {
        lvalue();
        nargs++;
      }
      if (havetok(tklt)) {
        expr(getrbp(tkcat));   // bwk "historical practice" precedence
        nargs++;
        modifier = tklt;
      }
      gen2cd(tkgetline, nargs);
      gencd(modifier);
      break;

    default:
      XERR("syntax near '%s'\n", TT.tokstr[0] == '\n' ? "\\n" : TT.tokstr);
      skip_to(stmtendsy);
      break;
  }
  return 0;
}

static void binary_op(int optor)  // Also for ternary ?: optor.
{
  int nargs, cdx = 0;  // index in TT.zcode list
  int rbp = getrbp(optor);
  if (optor != tkcat) scan();
  // CURTOK() holds first token of right operand.
  switch (optor) {
    case tkin:
      // right side of 'in' must be (only) an array name
      map_name();
      gencd(tkin);
      scan();
      // FIXME TODO 20230109 x = y in a && 2 works OK?
      // x = y in a + 2 does not; it's parsed as x = (y in a) + 2
      // The +2 is not cat'ed with (y in a) as in bwk's OTA.
      // Other awks see y in a + 2 as a syntax error. They (may)
      // not want anything after y in a except a lower binding operator
      // (&& || ?:) or end of expression, i.e. ')' ';' '}'
      break;

  case tkpipe:
      expect(tkgetline);
      nargs = 1;
      if (ISTOK(tkfield) || ISTOK(tkvar)) {
        lvalue();
        nargs++;
      }
      gen2cd(tkgetline, nargs);
      gencd(tkpipe);
      break;

  case tkand:
  case tkor:
      optional_nl();
      gen2cd(optor, -1);  // tkand: jump if false, else drop
      cdx = TT.zcode_last;   // tkor:  jump if true, else drop
      expr(rbp);
      gencd(opnotnot);    // replace TT.stack top with truth value
      ZCODE[cdx] = TT.zcode_last - cdx;
      break;

  case tkternif:
      gen2cd(optor, -1);
      cdx = TT.zcode_last;
      expr(0);
      expect(tkternelse);
      gen2cd(tkternelse, -1);
      ZCODE[cdx] = TT.zcode_last - cdx;
      cdx = TT.zcode_last;
      expr(rbp);
      ZCODE[cdx] = TT.zcode_last - cdx;
      break;

  case tkmatchop:
  case tknotmatch:
      expr(rbp);
      if (ZCODE[TT.zcode_last - 1] == opmatchrec) ZCODE[TT.zcode_last - 1] = tkregex;
      gencd(optor);
      break;

  default:
      expr(rbp);
      gencd(optor);
  }
}

static int cat_start_concated_expr(int tok)
{
  // concat'ed expr can start w/ var number string func builtin $ ! ( (or ++ if prev was not lvalue)
  static char exprstarttermsy[] = {tkvar, tknumber, tkstring, tkregex, tkfunc, tkbuiltin,
    tkfield, tknot, tkincr, tkdecr, tklparen, tkgetline, 0};

  // NOTE this depends on builtins (close etc) being >= tkgetline
  return !! strchr(exprstarttermsy, tok) || tok >= tkgetline;
}

#define CALLED_BY_PRINT 99987 // Arbitrary, different from any real rbp value

static int expr(int rbp)
{
  // On entry: TT.scs has first symbol of expression, e.g. var, number, string,
  // regex, func, getline, left paren, prefix op ($ ++ -- ! unary + or -) etc.
  static char asgnops[] = {tkpowasgn, tkmodasgn, tkmulasgn, tkdivasgn,
    tkaddasgn, tksubasgn, tkasgn, 0};
  int prim_st = primary();
  // If called directly by print_stmt(), and found a parenthesized expression list
  //    followed by an end of print statement: any of > >> | ; } <newline>
  //    Then: return the count of expressions in list
  //    Else: continue parsing an expression
  if (rbp == CALLED_BY_PRINT) {
    if (prim_st > 0 && strchr(printexprendsy, CURTOK())) return prim_st;
    else rbp = 0;
  }

  // mult_expr_list in parens must be followed by 'in' unless it
  // immediately follows print or printf, where it may still be followed
  // by 'in' ... unless at end of statement
  if (prim_st > 0 && ! ISTOK(tkin))
    XERR("syntax near '%s'; expected 'in'\n", TT.tokstr);
  if (prim_st > 0) gen2cd(tkrbracket, prim_st);
  // primary() has eaten subscripts, function args, postfix ops.
  // CURTOK() should be a binary op.
  int optor = CURTOK();
  if (strchr(asgnops, optor)) {

    // TODO FIXME ?  NOT SURE IF THIS WORKS RIGHT!
    // awk does not parse according to POSIX spec in some odd cases.
    // When an assignment (lvalue =) is on the right of certain operators,
    // it is not treated as a bad lvalue (as it is in C).
    // Example: (1 && a=2) # no error; the assignment is performed.
    // This happens for ?: || && ~ !~ < <= ~= == > >=
    //
    static char odd_assignment_rbp[] = {59, 60, 70, 80, 100, 110, 0};
    if (prim_st < 0 && (rbp <= getrbp(optor) || strchr(odd_assignment_rbp, rbp))) {
      convert_push_to_reference();
      scan();
      expr(getrbp(optor));
      gencd(optor);
      return 0;
    }
    XERR("syntax near '%s'\n", TT.tokstr[0] == '\n' ? "\\n" : TT.tokstr);
    skip_to(stmtendsy);
  }
  if (cat_start_concated_expr(optor)) optor = tkcat;
  while (rbp < getlbp(optor)) {
    binary_op(optor);
    // HERE tok s/b an operator or expression terminator ( ; etc.).
    optor = CURTOK();
    if (cat_start_concated_expr(optor)) optor = tkcat;
  }
  return 0;
}

static void print_stmt(int tk)
{
  static char outmodes[] = {tkgt, tkappend, tkpipe, 0};
  int num_exprs = 0, outmode;
  TT.cgl.in_print_stmt = 1;
  expect(tk); // tkprint or tkprintf
  if ((tk == tkprintf) || !strchr(printexprendsy, CURTOK())) {
    // printf always needs expression
    // print non-empty statement needs expression
    num_exprs = expr(CALLED_BY_PRINT);
    if (num_exprs > 0 && !strchr(printexprendsy, CURTOK())) FATAL("print stmt bug");
    if (!num_exprs) {
      for (num_exprs++; have_comma(); num_exprs++)
        expr(0);
    }
  }
  outmode = CURTOK();
  if (strchr(outmodes, outmode)) {
    scan();
    expr(0); // FIXME s/b only bwk term? check POSIX
    num_exprs++;
  } else outmode = 0;
  gen2cd(tk, num_exprs);
  gencd(outmode);
  TT.cgl.in_print_stmt = 0;
}

static void delete_stmt(void)
{
  expect(tkdelete);
  if (ISTOK(tkvar)) {
    int slotnum = find_or_add_var_name();
    check_set_map(slotnum);
    scan();
    if (havetok(tklbracket)) {
      int num_subscripts = 0;
      do {
        expr(0);
        num_subscripts++;
      } while (have_comma());
      expect(tkrbracket);
      if (num_subscripts > 1) gen2cd(tkrbracket, num_subscripts);
      gen2cd(opmapref, slotnum);
      gencd(tkdelete);
    } else {
      // delete entire map (elements only; var is still a map)
      gen2cd(opmapref, slotnum);
      gencd(opmapdelete);
    }
  } else expect(tkvar);
}

static void simple_stmt(void)
{
  if (strchr(exprstartsy, CURTOK())) {
    expr(0);
    gencd(opdrop);
    return;
  }
  switch (CURTOK()) {
    case tkprint:
    case tkprintf:
      print_stmt(CURTOK());
      break;

    case tkdelete:
      delete_stmt();
      break;

    default:
      XERR("syntax near '%s'\n", TT.tokstr[0] == '\n' ? "\\n" : TT.tokstr);
      skip_to(stmtendsy);
  }
}

static int prev_was_terminated(void)
{
  return !!strchr(stmtendsy, TT.prevtok);
}

static int is_nl_semi(void)
{
  return ISTOK(tknl) || ISTOK(tksemi);
}

static void if_stmt(void)
{
  expect(tkif);
  expect(tklparen);
  expr(0);
  rparen();
  gen2cd(tkif, -1);
  int cdx = TT.zcode_last;
  stmt();
  if (!prev_was_terminated() && is_nl_semi()) {
    scan();
    optional_nl();
  }
  if (prev_was_terminated()) {
    optional_nl();
    if (havetok(tkelse)) {
      gen2cd(tkelse, -1);
      ZCODE[cdx] = TT.zcode_last - cdx;
      cdx = TT.zcode_last;
      optional_nl();
      stmt();
    }
  }
  ZCODE[cdx] = TT.zcode_last - cdx;
}

static void save_break_continue(int *brk, int *cont)
{
  *brk = TT.cgl.break_dest;
  *cont = TT.cgl.continue_dest;
}

static void restore_break_continue(int *brk, int *cont)
{
  TT.cgl.break_dest = *brk;
  TT.cgl.continue_dest = *cont;
}

static void while_stmt(void)
{
  int brk, cont;
  save_break_continue(&brk, &cont);
  expect(tkwhile);
  expect(tklparen);
  TT.cgl.continue_dest = TT.zcode_last + 1;
  expr(0);
  rparen();
  gen2cd(tkwhile, 2);    // drop, jump if true
  TT.cgl.break_dest = TT.zcode_last + 1;
  gen2cd(opjump, -1);     // jump here to break
  stmt();
  gen2cd(opjump, -1);     // jump to continue
  ZCODE[TT.zcode_last] = TT.cgl.continue_dest - TT.zcode_last - 1;
  ZCODE[TT.cgl.break_dest + 1] = TT.zcode_last - TT.cgl.break_dest - 1;
  restore_break_continue(&brk, &cont);
}

static void do_stmt(void)
{
  int brk, cont;
  save_break_continue(&brk, &cont);
  expect(tkdo);
  optional_nl();
  gen2cd(opjump, 4);   // jump over jumps, to statement
  TT.cgl.continue_dest = TT.zcode_last + 1;
  gen2cd(opjump, -1);   // here on continue
  TT.cgl.break_dest = TT.zcode_last + 1;
  gen2cd(opjump, -1);   // here on break
  stmt();
  if (!prev_was_terminated()) {
    if (is_nl_semi()) {
      scan();
      optional_nl();
    } else {
      XERR("syntax near '%s' -- ';' or newline expected\n", TT.tokstr);
      // FIXME
    }
  }
  ZCODE[TT.cgl.continue_dest + 1] = TT.zcode_last - TT.cgl.continue_dest - 1;
  optional_nl();
  expect(tkwhile);
  expect(tklparen);
  expr(0);
  rparen();
  gen2cd(tkwhile, TT.cgl.break_dest - TT.zcode_last - 1);
  ZCODE[TT.cgl.break_dest + 1] = TT.zcode_last - TT.cgl.break_dest - 1;
  restore_break_continue(&brk, &cont);
}

static void for_not_map_iter(void)
{
  // Here after loop initialization, if any; loop condition
  int condition_loc = TT.zcode_last + 1;
  if (havetok(tksemi)) {
    // "endless" loop variant; no condition
    // no NL allowed here in OTA
    gen2cd(opjump, -1);     // jump to statement
  } else {
    optional_nl();                // NOT posix or awk book; in OTA
    expr(0);                 // loop while true
    expect(tksemi);
    gen2cd(tkwhile, -1);    // drop, jump to statement if true
  }
  optional_nl();                    // NOT posix or awk book; in OTA
  TT.cgl.break_dest = TT.zcode_last + 1;
  gen2cd(opjump, -1);
  TT.cgl.continue_dest = TT.zcode_last + 1;
  if (!ISTOK(tkrparen)) simple_stmt();  // "increment"
  gen2cd(opjump, condition_loc - TT.zcode_last - 3);
  rparen();
  ZCODE[TT.cgl.break_dest - 1] = TT.zcode_last - TT.cgl.break_dest + 1;
  stmt();
  gen2cd(opjump, TT.cgl.continue_dest - TT.zcode_last - 3);
  ZCODE[TT.cgl.break_dest + 1] = TT.zcode_last - TT.cgl.break_dest - 1;
}

static int valid_for_array_iteration(int first, int last)
{
  return ZCODE[first] == tkvar && ZCODE[first + 2] == tkvar
      && ZCODE[first + 4] == tkin && ZCODE[first + 5] == opdrop
      && first + 5 == last;
}

static void for_stmt(void)
{
  int brk, cont;
  save_break_continue(&brk, &cont);
  expect(tkfor);
  expect(tklparen);
  if (havetok(tksemi)) {
    // No "initialization" part
    for_not_map_iter();
  } else {
    int loop_start_loc = TT.zcode_last + 1;
    simple_stmt();  // initializaton part, OR varname in arrayname form
    if (!havetok(tkrparen)) {
      expect(tksemi);
      for_not_map_iter();
    } else {
      // Must be map iteration
      // Check here for varname in varname!
      // FIXME TODO must examine generated TT.zcode for var in array?
      if (!valid_for_array_iteration(loop_start_loc, TT.zcode_last))
        XERR("%s", "bad 'for (var in array)' loop\n");
      else {
        ZCODE[TT.zcode_last-5] = opvarref;
        ZCODE[TT.zcode_last-1] = tknumber;
        ZCODE[TT.zcode_last] = make_literal_num_val(-1);
        TT.cgl.continue_dest = TT.zcode_last + 1;
        gen2cd(opmapiternext, 2);
        TT.cgl.break_dest = TT.zcode_last + 1;
        gen2cd(opjump, -1);   // fill in with loc after stmt
      }
      optional_nl();
      // fixup TT.stack if return or exit inside for (var in array)
      TT.cgl.stack_offset_to_fix += 3;
      stmt();
      TT.cgl.stack_offset_to_fix -= 3;
      gen2cd(opjump, TT.cgl.continue_dest - TT.zcode_last - 3);
      ZCODE[TT.cgl.break_dest + 1] = TT.zcode_last - TT.cgl.break_dest - 1;
      gencd(opdrop);
      gencd(opdrop);
      gencd(opdrop);
    }
  }
  restore_break_continue(&brk, &cont);
}

static void stmt(void)
{
  switch (CURTOK()) {
    case tkeof:
      break;     // FIXME ERROR?

    case tkbreak:
      scan();
      if (TT.cgl.break_dest) gen2cd(tkbreak, TT.cgl.break_dest - TT.zcode_last - 3);
      else XERR("%s", "break not in a loop\n");
      break;

    case tkcontinue:
      scan();
      if (TT.cgl.continue_dest)
        gen2cd(tkcontinue, TT.cgl.continue_dest - TT.zcode_last - 3);
      else XERR("%s", "continue not in a loop\n");
      break;

    case tknext:
      scan();
      gencd(tknext);
      if (TT.cgl.rule_type) XERR("%s", "next inside BEGIN or END\n");
      if (TT.cgl.in_function_body) XERR("%s", "next inside function def\n");
      break;

    case tknextfile:
      scan();
      gencd(tknextfile);
      if (TT.cgl.rule_type) XERR("%s", "nextfile inside BEGIN or END\n");
      if (TT.cgl.in_function_body) XERR("%s", "nextfile inside function def\n");
      break;

    case tkexit:
      scan();
      if (strchr(exprstartsy, CURTOK())) {
        expr(0);
      } else gen2cd(tknumber, make_literal_num_val(NO_EXIT_STATUS));
      gencd(tkexit);
      break;

    case tkreturn:
      scan();
      if (TT.cgl.stack_offset_to_fix) gen2cd(opdrop_n, TT.cgl.stack_offset_to_fix);
      if (strchr(exprstartsy, CURTOK())) {
        expr(0);
      } else gen2cd(tknumber, make_literal_num_val(0.0));
      gen2cd(tkreturn, TT.cgl.nparms);
      if (!TT.cgl.in_function_body) XERR("%s", "return outside function def\n");
      break;

    case tklbrace:
      action(tklbrace);
      break;

    case tkif:
      if_stmt();
      break;

    case tkwhile:
      while_stmt();
      break;

    case tkdo:
      do_stmt();
      break;

    case tkfor:
      for_stmt();
      break;

    case tksemi:
      scan();
      break;
    default:
      simple_stmt();      // expression print printf delete
  }
}

static void add_param(int funcnum, char *s)
{
  if (!find_local_entry(s)) add_local_entry(s);
  else XERR("function '%s' dup param '%s'\n", FUNC_DEF[funcnum].name, s);
  TT.cgl.nparms++;

  // POSIX: The same name shall not be used as both a function parameter name
  // and as the name of a function or a special awk variable.
  // !!! NOTE seems implementations exc. mawk only compare param names with
  // builtin funcs; use same name as userfunc is OK!
  if (!strcmp(s, FUNC_DEF[funcnum].name))
    XERR("function '%s' param '%s' matches func name\n",
        FUNC_DEF[funcnum].name, s);
  if (find_global(s) && find_global(s) < TT.spec_var_limit)
    XERR("function '%s' param '%s' matches special var\n",
        FUNC_DEF[funcnum].name, s);
}

static void function_def(void)
{
  expect(tkfunction);
  int funcnum = find_func_def_entry(TT.tokstr);
  if (!funcnum) {
    funcnum = add_func_def_entry(TT.tokstr);
  } else if (FUNC_DEF[funcnum].flags & FUNC_DEFINED) {
    XERR("dup defined function '%s'\n", TT.tokstr);
  }
  FUNC_DEF[funcnum].flags |= FUNC_DEFINED;
  if (find_global(TT.tokstr)) {
    // POSIX: The same name shall not be used both as a variable name with
    // global scope and as the name of a function.
    XERR("function name '%s' previously defined\n", TT.tokstr);
  }

  gen2cd(tkfunction, funcnum);
  FUNC_DEF[funcnum].zcode_addr = TT.zcode_last - 1;
  TT.cgl.funcnum = funcnum;
  TT.cgl.nparms = 0;
  if (ISTOK(tkfunc)) expect(tkfunc); // func name with no space before (
  else expect(tkvar);  // func name with space before (
  expect(tklparen);
  if (ISTOK(tkvar)) {
    add_param(funcnum, TT.tokstr);
    scan();
    // FIXME is the the best way? what if TT.tokstr not a tkvar?
    while (have_comma()) {
      add_param(funcnum, TT.tokstr);
      expect(tkvar);
    }
  }
  rparen();
  if (ISTOK(tklbrace)) {
    TT.cgl.in_function_body = 1;
    action(tkfunc);
    TT.cgl.in_function_body = 0;
    // Need to return uninit value if falling off end of function.
    gen2cd(tknumber, make_uninit_val());
    gen2cd(tkreturn, TT.cgl.nparms);
  } else {
    XERR("syntax near '%s'\n", TT.tokstr);
    // FIXME some recovery needed here!?
  }
  // Do not re-init locals table for dup function.
  // Avoids memory leak detected by LeakSanitizer.
  if (!FUNC_DEF[funcnum].function_locals.base) {
    FUNC_DEF[funcnum].function_locals = TT.locals_table;
    init_locals_table();
  }
}

static void action(int action_type)
{
(void)action_type;
  // action_type is tkbegin, tkend, tkdo (every line), tkif (if pattern),
  //                  tkfunc (function body), tklbrace (compound statement)
  // Should have lbrace on entry.
  expect(tklbrace);
  for (;;) {
    if (ISTOK(tkeof)) unexpected_eof();
    optional_nl_or_semi();
    if (havetok(tkrbrace)) {
      break;
    }
    stmt();
    // stmt() is normally unterminated here, but may be terminated if we
    // have if with no else (had to consume terminator looking for else)
    //   !!!   if (ISTOK(tkrbrace) || prev_was_terminated())
    if (prev_was_terminated()) continue;
    if (!is_nl_semi() && !ISTOK(tkrbrace)) {
      XERR("syntax near '%s' -- newline, ';', or '}' expected\n", TT.tokstr);
      while (!is_nl_semi() && !ISTOK(tkrbrace) && !ISTOK(tkeof)) scan();
      if (ISTOK(tkeof)) unexpected_eof();
    }
    if (havetok(tkrbrace)) break;
    // Must be semicolon or newline
    scan();
  }
}

static void rule(void)
{
  //       pa_pat
  //     | pa_pat lbrace stmtlist '}'
  //     | pa_pat ',' opt_nl pa_pat
  //     | pa_pat ',' opt_nl pa_pat lbrace stmtlist '}'
  //     | lbrace stmtlist '}'
  //     | XBEGIN lbrace stmtlist '}'
  //     | XEND lbrace stmtlist '}'
  //     | FUNC funcname '(' varlist rparen  lbrace stmtlist '}'

  switch (CURTOK()) {
    case tkbegin:
      scan();
      if (TT.cgl.last_begin) ZCODE[TT.cgl.last_begin] = TT.zcode_last - TT.cgl.last_begin;
      else TT.cgl.first_begin = TT.zcode_last + 1;

      TT.cgl.rule_type = tkbegin;
      action(tkbegin);
      TT.cgl.rule_type = 0;
      gen2cd(opjump, -1);
      TT.cgl.last_begin = TT.zcode_last;
      break;

    case tkend:
      scan();
      if (TT.cgl.last_end) ZCODE[TT.cgl.last_end] = TT.zcode_last - TT.cgl.last_end;
      else TT.cgl.first_end = TT.zcode_last + 1;

      TT.cgl.rule_type = tkbegin;
      action(tkend);
      TT.cgl.rule_type = 0;
      gen2cd(opjump, -1);
      TT.cgl.last_end = TT.zcode_last;
      break;

    case tklbrace:
      if (TT.cgl.last_recrule)
        ZCODE[TT.cgl.last_recrule] = TT.zcode_last - TT.cgl.last_recrule;
      else TT.cgl.first_recrule = TT.zcode_last + 1;
      action(tkdo);
      gen2cd(opjump, -1);
      TT.cgl.last_recrule = TT.zcode_last;
      break;

    case tkfunction:
      function_def();
      break;
    default:
      if (TT.cgl.last_recrule)
        ZCODE[TT.cgl.last_recrule] = TT.zcode_last - TT.cgl.last_recrule;
      else TT.cgl.first_recrule = TT.zcode_last + 1;
      gen2cd(opjump, 1);
      gencd(tkeof);
      int cdx = 0, saveloc = TT.zcode_last;
      expr(0);
      if (!have_comma()) {
        gen2cd(tkif, -1);
        cdx = TT.zcode_last;
      } else {
        gen2cd(oprange2, ++TT.cgl.range_pattern_num);
        gencd(-1);
        cdx = TT.zcode_last;
        ZCODE[saveloc-2] = oprange1;
        ZCODE[saveloc-1] = TT.cgl.range_pattern_num;
        ZCODE[saveloc] = TT.zcode_last - saveloc;
        expr(0);
        gen2cd(oprange3, TT.cgl.range_pattern_num);
      }
      if (ISTOK(tklbrace)) {
        action(tkif);
        ZCODE[cdx] = TT.zcode_last - cdx;
      } else {
        gencd(opprintrec);   // print $0 ?
        ZCODE[cdx] = TT.zcode_last - cdx;
      }
      gen2cd(opjump, -1);
      TT.cgl.last_recrule = TT.zcode_last;
  }
}

static void diag_func_def_ref(void)
{
  int n = zlist_len(&TT.func_def_table);
  for (int k = 1; k < n; k++) {
    if ((FUNC_DEF[k].flags & FUNC_CALLED) &&
            !(FUNC_DEF[k].flags & FUNC_DEFINED)) {
      // Sorry, we can't tell where this was called from, for now at least.
      XERR("Undefined function '%s'", FUNC_DEF[k].name);
    }
  }
}

static void compile(void)
{
  init_compiler();
  init_scanner();
  scan();
  optional_nl_or_semi();        // Does posix allow NL or ; before first rule?
  while (! ISTOK(tkeof)) {
    rule();
    optional_nl_or_semi();        // NOT POSIX
  }


  if (TT.cgl.last_begin) ZCODE[TT.cgl.last_begin-1] = opquit;
  if (TT.cgl.last_end) ZCODE[TT.cgl.last_end-1] = opquit;
  if (TT.cgl.last_recrule) ZCODE[TT.cgl.last_recrule-1] = opquit;

  gen2cd(tknumber, make_literal_num_val(0.0));
  gencd(tkexit);
  gencd(opquit);
  // If there are only BEGIN and END or only END actions, generate actions to
  // read all input before END.
  if (TT.cgl.first_end && !TT.cgl.first_recrule) {
    gencd(opquit);
    TT.cgl.first_recrule = TT.zcode_last;
  }
  gencd(opquit);  // One more opcode to keep ip in bounds in run code.
  diag_func_def_ref();
}

////////////////////
//// runtime
////////////////////

static void check_numeric_string(struct zvalue *v)
{
  if (v->vst) {
    char *end, *s = v->vst->str;
    // Significant speed gain with this test:
    // num string must begin space, +, -, ., or digit.
    if (strchr("+-.1234567890 ", *s)) {
      double num = strtod(s, &end);
      if (s == end || end[strspn(end, " ")]) return;
      v->num = num;
      v->flags |= ZF_NUM | ZF_STR | ZF_NUMSTR;
    }
  }
}

static struct zstring *num_to_zstring(double n, char *fmt)
{
  int k;
  if (n == (long long)n) k = snprintf(TT.pbuf, PBUFSIZE, "%lld", (long long)n);
  else k = snprintf(TT.pbuf, PBUFSIZE, fmt, n);
  if (k < 0 || k >= PBUFSIZE) FFATAL("error encoding %f via '%s'", n, fmt);
  return new_zstring(TT.pbuf, k);
}

////////////////////
//// regex routines
////////////////////

static char *escape_str(char *s, int is_regex)
{
  char *p, *escapes = is_regex ? "abfnrtv\"/" : "\\abfnrtv\"/";
  // FIXME TODO should / be in there?
  char *s0 = s, *to = s;
  while ((*to = *s)) {
    if (*s != '\\') { to++, s++;
    } else if ((p = strchr(escapes, *++s))) {
      // checking char after \ for known escapes
      int c = (is_regex?"\a\b\f\n\r\t\v\"/":"\\\a\b\f\n\r\t\v\"/")[p-escapes];
      if (c) *to = c, s++;  // else final backslash
      to++;
    } else if ('0' <= *s && *s <= '9') {
      int k, c = *s++ - '0';
      for (k = 0; k < 2 && '0' <= *s && *s <= '9'; k++)
        c = c * 8 + *s++ - '0';
      *to++ = c;
    } else if (*s == 'x') {
      if (isxdigit(s[1])) {
        int c = hexval(*++s);
        if (isxdigit(s[1])) c = c * 16 + hexval(*++s);
        *to++ = c, s++;
      }
    } else {
      if (is_regex) *to++ = '\\';
      *to++ = *s++;
    }
  }
  return s0;
}

static void force_maybemap_to_scalar(struct zvalue *v)
{
  if (!(v->flags & ZF_ANYMAP)) return;
  if (v->flags & ZF_MAP || v->map->count)
    FATAL("array in scalar context");
  v->flags = 0;
  v->map = 0; // v->flags = v->map = 0 gets warning
}

static void force_maybemap_to_map(struct zvalue *v)
{
  if (v->flags & ZF_MAYBEMAP) v->flags = ZF_MAP;
}

// fmt_offs is either CONVFMT or OFMT (offset in stack to zvalue)
static struct zvalue *to_str_fmt(struct zvalue *v, int fmt_offs)
{
  force_maybemap_to_scalar(v);
  // TODO: consider handling numstring differently
  if (v->flags & ZF_NUMSTR) v->flags = ZF_STR;
  if (IS_STR(v)) return v;
  else if (!v->flags) { // uninitialized
    v->vst = new_zstring("", 0);
  } else if (IS_NUM(v)) {
    zvalue_release_zstring(v);
    if (!IS_STR(&STACK[fmt_offs])) {
      zstring_release(&STACK[fmt_offs].vst);
      STACK[fmt_offs].vst = num_to_zstring(STACK[fmt_offs].num, "%.6g");
      STACK[fmt_offs].flags = ZF_STR;
    }
    v->vst = num_to_zstring(v->num, STACK[fmt_offs].vst->str);
  } else {
    FATAL("Wrong or unknown type in to_str_fmt\n");
  }
  v->flags = ZF_STR;
  return v;
}

static struct zvalue *to_str(struct zvalue *v)
{
  return to_str_fmt(v, CONVFMT);
}

// TODO FIXME Is this needed? (YES -- investigate) Just use to_str()?
#define ENSURE_STR(v) (IS_STR(v) ? (v) : to_str(v))

static void rx_zvalue_compile(regex_t **rx, struct zvalue *pat)
{
  if (IS_RX(pat)) *rx = pat->rx;
  else {
    zvalue_dup_zstring(to_str(pat));
    escape_str(pat->vst->str, 1);
    xregcomp(*rx, pat->vst->str, REG_EXTENDED);
  }
}

static void rx_zvalue_free(regex_t *rx, struct zvalue *pat)
{
  if (!IS_RX(pat) || rx != pat->rx) regfree(rx);
}

// Used by the match/not match ops (~ !~) and implicit $0 match (/regex/)
static int match(struct zvalue *zvsubject, struct zvalue *zvpat)
{
  int r;
  regex_t rx, *rxp = &rx;
  rx_zvalue_compile(&rxp, zvpat);
  if ((r = regexec(rxp, to_str(zvsubject)->vst->str, 0, 0, 0)) != 0) {
    if (r != REG_NOMATCH) {
      char errbuf[256];
      regerror(r, &rx, errbuf, sizeof(errbuf));
      // FIXME TODO better diagnostic here
      error_exit("regex match error %d: %s", r, errbuf);
    }
    rx_zvalue_free(rxp, zvpat);
    return 1;
  }
  rx_zvalue_free(rxp, zvpat);
  return 0;
}

static int rx_find(regex_t *rx, char *s, regoff_t *start, regoff_t *end, int eflags)
{
  regmatch_t matches[1];
  int r = regexec(rx, s, 1, matches, eflags);
  if (r == REG_NOMATCH) return r;
  if (r) FATAL("regexec error");  // TODO ? use regerr() to meaningful msg
  *start = matches[0].rm_so;
  *end = matches[0].rm_eo;
  return 0;
}

// Differs from rx_find() in that FS cannot match null (empty) string.
// See https://www.austingroupbugs.net/view.php?id=1468.
static int rx_find_FS(regex_t *rx, char *s, regoff_t *start, regoff_t *end, int eflags)
{
  int r = rx_find(rx, s, start, end, eflags);
  if (r || *start != *end) return r;  // not found, or found non-empty match
  // Found empty match, retry starting past the match
  char *p = s + *end;
  if (!*p) return REG_NOMATCH;  // End of string, no non-empty match found
  // Empty match not at EOS, move ahead and try again
  while (!r && *start == *end && *++p)
    r = rx_find(rx, p, start, end, eflags);
  if (r || !*p) return REG_NOMATCH;  // no non-empty match found
  *start += p - s;  // offsets from original string
  *end += p - s;
  return 0;
}

////////////////////
////   fields
////////////////////

#define FIELDS_MAX  102400 // Was 1024; need more for toybox awk test
#define THIS_MEANS_SET_NF 999999999

static int get_int_val(struct zvalue *v)
{
  if (IS_NUM(v)) return (int)v->num;
  if (IS_STR(v) && v->vst) return (int)atof(v->vst->str);
  return 0;
}

// A single-char FS is never a regex, so make it a [<char>] regex to
// match only that one char in case FS is a regex metachar.
// If regex FS is needed, must use > 1 char. If a '.' regex
// is needed, use e.g. '.|.' (unlikely case).
static char *fmt_one_char_fs(char *fs)
{
  if (strlen(fs) != 1) return fs;
  snprintf(TT.one_char_fs, sizeof(TT.one_char_fs), "[%c]", fs[0]);
  return TT.one_char_fs;
}

static regex_t *rx_fs_prep(char *fs)
{
  if (!strcmp(fs, " ")) return &TT.rx_default;
  if (!strcmp(fs, TT.fs_last)) return &TT.rx_last;
  if (strlen(fs) >= FS_MAX) FATAL("FS too long");
  strcpy(TT.fs_last, fs);
  regfree(&TT.rx_last);
  xregcomp(&TT.rx_last, fmt_one_char_fs(fs), REG_EXTENDED);
  return &TT.rx_last;
}

// Only for use by split() builtin
static void set_map_element(struct zmap *m, int k, char *val, size_t len)
{
  // Do not need format here b/c k is integer, uses "%lld" format.
  struct zstring *key = num_to_zstring(k, "");// "" vs 0 format avoids warning
  struct zmap_slot *zs = zmap_find_or_insert_key(m, key);
  zstring_release(&key);
  zs->val.vst = zstring_update(zs->val.vst, 0, val, len);
  zs->val.flags = ZF_STR;
  check_numeric_string(&zs->val);
}

static void set_zvalue_str(struct zvalue *v, char *s, size_t size)
{
  v->vst = zstring_update(v->vst, 0, s, size);
  v->flags = ZF_STR;
}

// All changes to NF go through here!
static void set_nf(int nf)
{
  if (nf < 0) FATAL("NF set negative");
  STACK[NF].num = TT.nf_internal = nf;
  STACK[NF].flags = ZF_NUM;
}

static void set_field(struct zmap *unused, int fnum, char *s, size_t size)
{ (void)unused;
  if (fnum < 0 || fnum > FIELDS_MAX) FFATAL("bad field num %d\n", fnum);
  int nfields = zlist_len(&TT.fields);
  // Need nfields to be > fnum b/c e.g. fnum==1 implies 2 TT.fields
  while (nfields <= fnum)
    nfields = zlist_append(&TT.fields, &uninit_zvalue) + 1;
  set_zvalue_str(&FIELD[fnum], s, size);
  set_nf(fnum);
  check_numeric_string(&FIELD[fnum]);
}

// Split s via fs, using setter; return number of TT.fields.
// This is used to split TT.fields and also for split() builtin.
static int splitter(void (*setter)(struct zmap *, int, char *, size_t), struct zmap *m, char *s, struct zvalue *zvfs)
{
  regex_t *rx;
  regoff_t offs, end;
  int multiline_null_rs = !ENSURE_STR(&STACK[RS])->vst->str[0];
  int nf = 0, r = 0, eflag = 0;
  int one_char_fs = 0;
  char *s0 = s, *fs = "";
  if (!IS_RX(zvfs)) {
    to_str(zvfs);
    fs = zvfs->vst->str;
    one_char_fs = utf8cnt(zvfs->vst->str, zvfs->vst->size) == 1;
  }
  // Empty string or empty fs (regex).
  // Need to include !*s b/c empty string, otherwise
  // split("", a, "x") splits to a 1-element (empty element) array
  if (!*s || (IS_STR(zvfs) && !*fs) || IS_EMPTY_RX(zvfs)) {
    while (*s) {
      if (*s < 128) setter(m, ++nf, s++, 1);
      else {        // Handle UTF-8
        char cbuf[8];
        unsigned wc;
        int nc = utf8towc(&wc, s, strlen(s));
        if (nc < 2) FFATAL("bad string for split: \"%s\"\n", s0);
        s += nc;
        nc = wctoutf8(cbuf, wc);
        setter(m, ++nf, cbuf, nc);
      }
    }
    return nf;
  }
  if (IS_RX(zvfs)) rx = zvfs->rx;
  else rx = rx_fs_prep(fs);
  while (*s) {
    // Find the next occurrence of FS.
    // rx_find_FS() returns 0 if found. If nonzero, the field will
    // be the rest of the record (all of it if first time through).
    if ((r = rx_find_FS(rx, s, &offs, &end, eflag))) offs = end = strlen(s);
    if (setter == set_field && multiline_null_rs && one_char_fs) {
      // Contra POSIX, if RS=="" then newline is always also a
      // field separator only if FS is a single char (see gawk manual)
      int k = strcspn(s, "\n");
      if (k < offs) offs = k, end = k + 1;
    }
    eflag |= REG_NOTBOL;

    // Field will be s up to (not including) the offset. If offset
    // is zero and FS is found and FS is ' ' (TT.rx_default "[ \t]+"),
    // then the find is the leading or trailing spaces and/or tabs.
    // If so, skip this (empty) field, otherwise set field, length is offs.
    if (offs || r || rx != &TT.rx_default) setter(m, ++nf, s, offs);
    s += end;
  }
  if (!r && rx != &TT.rx_default) setter(m, ++nf, "", 0);
  return nf;
}

static void build_fields(void)
{
  char *rec = FIELD[0].vst->str;
  // TODO test this -- why did I not want to split empty $0?
  // Maybe don't split empty $0 b/c non-default FS gets NF==1 with splitter()?
  set_nf(*rec ? splitter(set_field, 0, rec, to_str(&STACK[FS])) : 0);
}

static void rebuild_field0(void)
{
  struct zstring *s = FIELD[0].vst;
  int nf = TT.nf_internal;
  if (!nf) {
    zvalue_copy(&FIELD[0], &uninit_string_zvalue);
    return;
  }
  // uninit value needed for eventual reference to .vst in zstring_release()
  struct zvalue tempv = uninit_zvalue;
  zvalue_copy(&tempv, to_str(&STACK[OFS]));
  for (int i = 1; i <= nf; i++) {
    if (i > 1) {
      s = s ? zstring_extend(s, tempv.vst) : zstring_copy(s, tempv.vst);
    }
    if (FIELD[i].flags) to_str(&FIELD[i]);
    if (FIELD[i].vst) {
      if (i > 1) s = zstring_extend(s, FIELD[i].vst);
      else s = zstring_copy(s, FIELD[i].vst);
    }
  }
  FIELD[0].vst = s;
  FIELD[0].flags |= ZF_STR;
  zvalue_release_zstring(&tempv);
}

// get field ref (lvalue ref) in prep for assignment to field.
// [... assigning to a nonexistent field (for example, $(NF+2)=5) shall
// increase the value of NF; create any intervening TT.fields with the
// uninitialized value; and cause the value of $0 to be recomputed, with the
// TT.fields being separated by the value of OFS.]
// Called by setup_lvalue()
static struct zvalue *get_field_ref(int fnum)
{
  if (fnum < 0 || fnum > FIELDS_MAX) error_exit("bad field num %d", fnum);
  if (fnum > TT.nf_internal) {
    // Ensure TT.fields list is large enough for fnum
    // Need len of TT.fields to be > fnum b/c e.g. fnum==1 implies 2 TT.fields
    for (int i = TT.nf_internal + 1; i <= fnum; i++) {
      if (i == zlist_len(&TT.fields)) zlist_append(&TT.fields, &uninit_zvalue);
      zvalue_copy(&FIELD[i], &uninit_string_zvalue);
    }
    set_nf(fnum);
  }
  return &FIELD[fnum];
}

// Called by tksplit op
static int split(struct zstring *s, struct zvalue *a, struct zvalue *fs)
{
  return splitter(set_map_element, a->map, s->str, fs);
}

// Called by getrec_f0_f() and getrec_f0()
static void copy_to_field0(char *buf, size_t k)
{
  set_zvalue_str(&FIELD[0], buf, k);
  check_numeric_string(&FIELD[0]);
  build_fields();
}

// After changing $0, must rebuild TT.fields & reset NF
// Changing other field must rebuild $0
// Called by gsub() and assignment ops.
static void fixup_fields(int fnum)
{
  if (fnum == THIS_MEANS_SET_NF) {  // NF was assigned to
    int new_nf = get_int_val(&STACK[NF]);
    // Ensure TT.fields list is large enough for fnum
    // Need len of TT.fields to be > fnum b/c e.g. fnum==1 implies 2 TT.fields
    for (int i = TT.nf_internal + 1; i <= new_nf; i++) {
      if (i == zlist_len(&TT.fields)) zlist_append(&TT.fields, &uninit_zvalue);
      zvalue_copy(&FIELD[i], &uninit_string_zvalue);
    }
    set_nf(TT.nf_internal = STACK[NF].num);
    rebuild_field0();
    return;
  }
  // fnum is # of field that was just updated.
  // If it's 0, need to rebuild the TT.fields 1... n.
  // If it's non-0, need to rebuild field 0.
  to_str(&FIELD[fnum]);
  if (fnum) check_numeric_string(&FIELD[fnum]);
  if (fnum) rebuild_field0();
  else build_fields();
}

// Fetching non-existent field gets uninit string value; no change to NF!
// Called by tkfield op       // TODO inline it?
static void push_field(int fnum)
{
  if (fnum < 0 || fnum > FIELDS_MAX) error_exit("bad field num %d", fnum);
  // Contrary to posix, awk evaluates TT.fields beyond $NF as empty strings.
  if (fnum > TT.nf_internal) push_val(&uninit_string_zvalue);
  else push_val(&FIELD[fnum]);
}

////////////////////
////   END fields
////////////////////

#define STKP    TT.stackp   // pointer to top of stack

static double seedrand(double seed)
{
  static double prev_seed;
  double r = prev_seed;
  srandom(trunc(prev_seed = seed));
  return r;
}

static int popnumval(void)
{
  return STKP-- -> num;
}

static void drop(void)
{
  if (!(STKP->flags & (ZF_ANYMAP | ZF_RX))) zstring_release(&STKP->vst);
  STKP--;
}

static void drop_n(int n)
{
  while (n--) drop();
}

static void swap(void)
{
  struct zvalue tmp = STKP[-1];
  STKP[-1] = STKP[0];
  STKP[0] = tmp;
}

// Set and return logical (0/1) val of top TT.stack value; flag value as NUM.
static int get_set_logical(void)
{
  struct zvalue *v = STKP;
  force_maybemap_to_scalar(v);
  int r = 0;
  if (IS_NUM(v)) r = !! v->num;
  else if (IS_STR(v)) r = (v->vst && v->vst->str[0]);
  zvalue_release_zstring(v);
  v->num = r;
  v->flags = ZF_NUM;
  return r;
}


static double to_num(struct zvalue *v)
{
  force_maybemap_to_scalar(v);
  if (v->flags & ZF_NUMSTR) zvalue_release_zstring(v);
  else if (!IS_NUM(v)) {
    v->num = 0.0;
    if (IS_STR(v) && v->vst) v->num = atof(v->vst->str);
    zvalue_release_zstring(v);
  }
  v->flags = ZF_NUM;
  return v->num;
}

static void set_num(struct zvalue *v, double n)
{
  zstring_release(&v->vst);
  v->num = n;
  v->flags = ZF_NUM;
}

static void incr_zvalue(struct zvalue *v)
{
  v->num = trunc(to_num(v)) + 1;
}

static void push_int_val(ptrdiff_t n)
{
  struct zvalue v = ZVINIT(ZF_NUM, n, 0);
  push_val(&v);
}

static struct zvalue *get_map_val(struct zvalue *v, struct zvalue *key)
{
  struct zmap_slot *x = zmap_find_or_insert_key(v->map, to_str(key)->vst);
  return &x->val;
}

static struct zvalue *setup_lvalue(int ref_stack_ptr, int parmbase, int *field_num)
{
  // ref_stack_ptr is number of slots down in stack the ref is
  // for +=, *=, etc
  // Stack is: ... scalar_ref value_to_op_by
  // or ... subscript_val map_ref value_to_op_by
  // or ... fieldref value_to_op_by
  // for =, ++, --
  // Stack is: ... scalar_ref
  // or ... subscript_val map_ref
  // or ... fieldnum fieldref
  int k;
  struct zvalue *ref, *v = 0; // init v to mute "may be uninit" warning
  *field_num = -1;
  ref = STKP - ref_stack_ptr;
  if (ref->flags & ZF_FIELDREF) return get_field_ref(*field_num = ref->num);
  k = ref->num >= 0 ? ref->num : parmbase - ref->num;
  if (k == NF) *field_num = THIS_MEANS_SET_NF;
  v = &STACK[k];
  if (ref->flags & ZF_REF) {
    force_maybemap_to_scalar(v);
  } else if (ref->flags & ZF_MAPREF) {
    force_maybemap_to_map(v);
    if (!IS_MAP(v)) FATAL("scalar in array context");
    v = get_map_val(v, STKP - ref_stack_ptr - 1);
    swap();
    drop();
  } else FATAL("assignment to bad lvalue");
  return v; // order FATAL() and return to mute warning
}

static struct zfile *new_file(char *fn, FILE *fp, char mode, char file_or_pipe,
                              char is_std_file)
{
  struct zfile *f = xzalloc(sizeof(struct zfile));
  *f = (struct zfile){TT.zfiles, xstrdup(fn), fp, mode, file_or_pipe,
                isatty(fileno(fp)), is_std_file, 0, 0, 0, 0, 0};
  return TT.zfiles = f;
}

static int fflush_all(void)
{
  int ret = 0;
  for (struct zfile *p = TT.zfiles; p; p = p->next)
    if (fflush(p->fp)) ret = -1;
  return ret;
}

static int fflush_file(int nargs)
{
  if (!nargs) return fflush_all();

  to_str(STKP);   // filename at top of TT.stack
  // Null string means flush all
  if (!STKP[0].vst->str[0]) return fflush_all();

  // is it open in file table?
  for (struct zfile *p = TT.zfiles; p; p = p->next)
    if (!strcmp(STKP[0].vst->str, p->fn))
      if (!fflush(p->fp)) return 0;
  return -1;    // error, or file not found in table
}
static int close_file(char *fn)
{
  // !fn (null ptr) means close all (exc. stdin/stdout/stderr)
  int r = 0;
  struct zfile *np, **pp = &TT.zfiles;
  for (struct zfile *p = TT.zfiles; p; p = np) {
    np = p->next;   // save in case unlinking file (invalidates p->next)
    // Don't close std files -- wrecks print/printf (can be fixed though TODO)
    if ((!p->is_std_file) && (!fn || !strcmp(fn, p->fn))) {
      xfree(p->buf);
      xfree(p->fn);
      r = (p->fp) ? (p->file_or_pipe ? fclose : pclose)(p->fp) : -1;
      *pp = p->next;
      xfree(p);
      if (fn) return r;
    } else pp = &p->next; // only if not unlinking zfile
  }
  return -1;  // file not in table, or closed all files
}

static struct zfile badfile_obj, *badfile = &badfile_obj;

// FIXME TODO check if file/pipe/mode matches what's in the table already.
// Apparently gawk/mawk/nawk are OK with different mode, but just use the file
// in whatever mode it's already in; i.e. > after >> still appends.
static struct zfile *setup_file(char file_or_pipe, char *mode)
{
  to_str(STKP);   // filename at top of TT.stack
  char *fn = STKP[0].vst->str;
  // is it already open in file table?
  for (struct zfile *p = TT.zfiles; p; p = p->next)
    if (!strcmp(fn, p->fn)) {
      drop();
      return p;   // open; return it
    }
  FILE *fp = (file_or_pipe ? fopen : popen)(fn, mode);
  if (fp) {
    struct zfile *p = new_file(fn, fp, *mode, file_or_pipe, 0);
    drop();
    return p;
  }
  if (*mode != 'r') FFATAL("cannot open '%s'\n", fn);
  drop();
  return badfile;
}

// TODO FIXME should be a function?
#define stkn(n) ((int)(TT.stackp - (n) - (struct zvalue *)TT.stack.base))

static int getcnt(int k)
{
  if (k >= stkn(0)) FATAL("too few args for printf\n");
  return (int)to_num(&STACK[k]);
}

static int fsprintf(FILE *ignored, const char *fmt, ...)
{
  (void)ignored;
  va_list args, args2;
  va_start(args, fmt);
  va_copy(args2, args);
  int len = vsnprintf(0, 0, fmt, args); // size needed
  va_end(args);
  if (len < 0) FATAL("Bad sprintf format");
  // Unfortunately we have to mess with zstring internals here.
  if (TT.rgl.zspr->size + len + 1 > TT.rgl.zspr->capacity) {
      // This should always work b/c capacity > size
      unsigned cap = 2 * TT.rgl.zspr->capacity + len;
      TT.rgl.zspr = xrealloc(TT.rgl.zspr, sizeof(*TT.rgl.zspr) + cap);
      TT.rgl.zspr->capacity = cap;
    }
  vsnprintf(TT.rgl.zspr->str + TT.rgl.zspr->size, len+1, fmt, args2);
  TT.rgl.zspr->size += len;
  TT.rgl.zspr->str[TT.rgl.zspr->size] = 0;
  va_end(args2);
  return 0;
}

static void varprint(int(*fpvar)(FILE *, const char *, ...), FILE *outfp, int nargs)
{
  int k, nn, nnc, fmtc, holdc, cnt1 = 0, cnt2 = 0;
  char *s = 0;  // to shut up spurious warning
  regoff_t offs = -1, e = -1;
  char *pfmt, *fmt = to_str(STKP-nargs+1)->vst->str;
  k = stkn(nargs - 2);
  while (*fmt) {
    double n = 0;
    nn = strcspn(fmt, "%");
    if (nn) {
      holdc = fmt[nn];
      fmt[nn] = 0;
      fpvar(outfp, "%s", fmt);
      fmt[nn] = holdc;
    }
    fmt += nn;
    if (!*(pfmt = fmt)) break;
    nnc = strcspn(fmt+1, "aAdiouxXfFeEgGcs%");
    fmtc = fmt[nnc+1];
    if (!fmtc) FFATAL("bad printf format '%s'", fmt);
    holdc = fmt[nnc+2];
    fmt[nnc+2] = 0;
    if (rx_find(&TT.rx_printf_fmt, fmt, &offs, &e, 0))
      FFATAL("bad printf format <%s>\n", fmt);
    int nargsneeded = 1;
    for (char *p = strchr(fmt, '*'); p; p = strchr(p+1, '*'))
      nargsneeded++;
    nargsneeded -= fmtc == '%';

    switch (nargsneeded) {
      case 0:
        fpvar(outfp, fmt);
        break;
      case 3:
        cnt1 = getcnt(k++);
        ATTR_FALLTHROUGH_INTENDED;
      case 2:
        cnt2 = getcnt(k++);
        ATTR_FALLTHROUGH_INTENDED;
      case 1:
        if (k > stkn(0)) FATAL("too few args for printf\n");
        if (fmtc == 's') {
          s = to_str(&STACK[k++])->vst->str;
        } else if (fmtc == 'c' && !IS_NUM(&STACK[k])) {
          unsigned wch;
          struct zvalue *z = &STACK[k++];
          if (z->vst && z->vst->str[0])
            n = utf8towc(&wch, z->vst->str, z->vst->size) < 1 ? 0xfffd : wch;
        } else {
          n = to_num(&STACK[k++]);
        }
        if (strchr("cdiouxX", fmtc)) {
          pfmt = strcpy(TT.pbuf, fmt);
          if (pfmt[nnc] != 'l') {
            strcpy(pfmt+nnc+1, "l_");
            pfmt[nnc+2] = fmtc;
          }
        }
        if (fmtc == 'c' && n > 0x10ffff) n = 0xfffd;  // musl won't take larger "wchar"
        switch (nargsneeded) {
          case 1:
            if (fmtc == 's') fpvar(outfp, pfmt, s);
            else if (fmtc == 'c') fpvar(outfp, pfmt, (wint_t)n);
            else if (strchr("di", fmtc)) fpvar(outfp, pfmt, (long)n);
            else if (strchr("ouxX", fmtc)) fpvar(outfp, pfmt, (unsigned long)n);
            else fpvar(outfp, pfmt, n);
            break;
          case 2:
            if (fmtc == 's') fpvar(outfp, pfmt, cnt2, s);
            else if (fmtc == 'c') fpvar(outfp, pfmt, cnt2, (wint_t)n);
            else if (strchr("di", fmtc)) fpvar(outfp, pfmt, cnt2, (long)n);
            else if (strchr("ouxX", fmtc)) fpvar(outfp, pfmt, cnt2, (unsigned long)n);
            else fpvar(outfp, pfmt, cnt2, n);
            break;
          case 3:
            if (fmtc == 's') fpvar(outfp, pfmt, cnt1, cnt2, s);
            else if (fmtc == 'c') fpvar(outfp, pfmt, cnt1, cnt2, (wint_t)n);
            else if (strchr("di", fmtc)) fpvar(outfp, pfmt, cnt1, cnt2, (long)n);
            else if (strchr("ouxX", fmtc)) fpvar(outfp, pfmt, cnt1, cnt2, (unsigned long)n);
            else fpvar(outfp, pfmt, cnt1, cnt2, n);
            break;
        }
        break;
      default:
        FATAL("bad printf format\n");
    }
    fmt += nnc + 2;
    *fmt = holdc;
  }
}

static int is_ok_varname(char *v)
{
  char *ok = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
  if (!*v) return 0;
  for (int i = 0; v[i]; i++)
    if (i ? !strchr(ok, v[i]) : !strchr(ok + 10, v[i])) return 0;
  return 1;
}

// FIXME TODO return value never used. What if assign to var not in globals?
static int assign_global(char *var, char *value)
{
  if (!is_ok_varname(var)) FFATAL("Invalid variable name '%s'\n", var);
  int globals_ent = find_global(var);
  if (globals_ent) {
    struct zvalue *v = &STACK[globals_ent];
    if (IS_MAP(v)) error_exit("-v assignment to array");  // Maybe not needed?

// The compile phase may insert a var in global table with flag of zero.  Then
// init_globals() will assign a ZF_MAYBEMAP flag to it. If it is then assigned
// via -v option or by assignment_arg() it will here be assigned a string value.
// So first, remove all map data to prevent memory leak. BUG FIX // 2024-02-13.
    if (v->flags & ZF_ANYMAP) {
      zmap_delete_map_incl_slotdata(v->map);
      xfree(v->map);
      v->map = 0;
      v->flags &= ~ZF_ANYMAP;
    }

    zvalue_release_zstring(v);
    value = xstrdup(value);
    *v = new_str_val(escape_str(value, 0));
    xfree(value);
    check_numeric_string(v);
    return 1;
  }
  return 0;
}

// If valid assignment arg, assign the global and return 1;
// otherwise return 0.
// TODO FIXME This does not check the format of the variable per posix.
// Needs to start w/ _A-Za-z then _A-Za-z0-9
// If not valid assignment form, then nextfilearg needs to treat as filename.
static int assignment_arg(char *arg)
{
  char *val = strchr(arg, '=');
  if (val) {
    *val++ = 0;
    if (!is_ok_varname(arg)) {
      *--val = '=';
      return 0;
    }
    assign_global(arg, val);
    *--val = '=';
    return 1;
  } else return 0;
}

static char *nextfilearg(void)
{
  char *arg;
  do {
    if (++TT.rgl.narg >= (int)to_num(&STACK[ARGC])) return 0;
    struct zvalue *v = &STACK[ARGV];
    struct zvalue zkey = ZVINIT(ZF_STR, 0,
        num_to_zstring(TT.rgl.narg, to_str(&STACK[CONVFMT])->vst->str));
    arg = "";
    if (zmap_find(v->map, zkey.vst)) {
      zvalue_copy(&TT.rgl.cur_arg, to_str(get_map_val(v, &zkey)));
      arg = TT.rgl.cur_arg.vst->str;
    }
    zvalue_release_zstring(&zkey);
  } while (!*arg || assignment_arg(arg));
  TT.rgl.nfiles++;
  return arg;
}

static int next_fp(void)
{
  char *fn = nextfilearg();
  if (TT.cfile->fp && TT.cfile->fp != stdin) fclose(TT.cfile->fp);
  if ((!fn && !TT.rgl.nfiles && TT.cfile->fp != stdin) || (fn && !strcmp(fn, "-"))) {
    xfree(TT.cfile->buf);
    *TT.cfile = (struct zfile){0};
    TT.cfile->fp = stdin;
    TT.cfile->fn = "-";
    zvalue_release_zstring(&STACK[FILENAME]);
    STACK[FILENAME].vst = new_zstring("-", 1);
  } else if (fn) {
    xfree(TT.cfile->buf);
    *TT.cfile = (struct zfile){0};
    if (!(TT.cfile->fp = fopen(fn, "r"))) FFATAL("can't open %s\n", fn);
    TT.cfile->fn = fn;
    zvalue_copy(&STACK[FILENAME], &TT.rgl.cur_arg);
  } else {
    TT.rgl.eof = 1;
    return 0;
  }
  set_num(&STACK[FNR], 0);
  TT.cfile->is_tty = isatty(fileno(TT.cfile->fp));
  return 1;
}

static int rx_find_rs(regex_t *rx, char *s, long len,
                      regoff_t *start, regoff_t *end, int one_byte_rs)
{
  regmatch_t matches[1];
  if (one_byte_rs) {
    char *p = memchr(s, one_byte_rs, len);
    if (!p) return REG_NOMATCH;
    *start = p - s;
    *end = *start + 1;
  } else {
    int r = regexec0(rx, s, len, 1, matches, 0);
    if (r == REG_NOMATCH) return r;
    if (r) FATAL("regexec error");  // TODO ? use regerr() to meaningful msg
    *start = matches[0].rm_so;
    *end = matches[0].rm_eo;
  }
  return 0;
}

// get a record; return length, or -1 at EOF
// Does work for getrec_f() for regular RS or multiline
static ssize_t getr(struct zfile *zfp, int rs_mode)
{
  // zfp->buf (initially null) points to record buffer
  // zfp->buflen -- size of allocated buf
  // TT.rgl.recptr -- points to where record is being / has been read into
  // zfp->ro -- offset in buf to record data
  // zfp->lim -- offset to 1+last byte read in buffer
  // rs_mode nonzero iff multiline mode; reused for one-byte RS

  regex_t rsrx; // FIXME Need to cache and avoid rx compile on every record?
  long ret = -1;
  int r = -REG_NOMATCH;   // r cannot have this value after rx_findx() below
  regoff_t so = 0, eo = 0;
  size_t m = 0, n = 0;

  xregcomp(&rsrx, rs_mode ? "\n\n+" : fmt_one_char_fs(STACK[RS].vst->str),
      REG_EXTENDED);
  rs_mode = strlen(STACK[RS].vst->str) == 1 ? STACK[RS].vst->str[0] : 0;
  for ( ;; ) {
    if (zfp->ro == zfp->lim && zfp->eof) break; // EOF & last record; return -1

    // Allocate initial buffer, and expand iff buffer holds one
    //   possibly (probably) incomplete record.
    if (zfp->ro == 0 && zfp->lim == zfp->buflen)
      zfp->buf = xrealloc(zfp->buf,
          (zfp->buflen = maxof(512, zfp->buflen * 2)) + 1);

    if ((m = zfp->buflen - zfp->lim) && !zfp->eof) {
      // Read iff space left in buffer
      if (zfp->is_tty) m = 1;
      n = fread(zfp->buf + zfp->lim, 1, m, zfp->fp);
      if (n < m) {
        if (ferror(zfp->fp)) FFATAL("i/o error %d on %s!", errno, zfp->fn);
        zfp->eof = 1;
        if (!n && r == -REG_NOMATCH) break; // catch empty file here
      }
      zfp->lim += n;
      zfp->buf[zfp->lim] = 0;
    }
    TT.rgl.recptr = zfp->buf + zfp->ro;
    r = rx_find_rs(&rsrx, TT.rgl.recptr, zfp->lim - zfp->ro, &so, &eo, rs_mode);
    if (!r && so == eo) r = 1;  // RS was empty, so fake not found

    if (!zfp->eof && (r
          || (zfp->lim - (zfp->ro + eo)) < zfp->buflen / 4) && !zfp->is_tty) {
      // RS not found, or found near lim. Slide up and try to get more data
      // If recptr at start of buf and RS not found then expand buffer
      memmove(zfp->buf, TT.rgl.recptr, zfp->lim - zfp->ro);
      zfp->lim -= zfp->ro;
      zfp->ro = 0;
      continue;
    }
    ret = so;   // If RS found, then 'so' is rec length
    if (zfp->eof) {
      if (r) {  // EOF and RS not found; rec is all data left in buf
        ret = zfp->lim - zfp->ro;
        zfp->ro = zfp->lim; // set ro for -1 return on next call
      } else zfp->ro += eo; // RS found; advance ro
    } else zfp->ro += eo; // Here only if RS found not near lim

    if (!r || !zfp->is_tty) {
      // If is_tty then RS found; reset buffer pointers;
      // is_tty uses one rec per buffer load
      if (zfp->is_tty) zfp->ro = zfp->lim = 0;
      break;
    } // RS not found AND is_tty; loop to keep reading
  }
  regfree(&rsrx);
  return ret;
}

// get a record; return length, or -1 at EOF
static ssize_t getrec_f(struct zfile *zfp)
{
  int k;
  if (ENSURE_STR(&STACK[RS])->vst->str[0]) return getr(zfp, 0);
  // RS == "" so multiline read
  // Passing 1 to getr() forces multiline mode, which uses regex "\n\n+" to
  // split on sequences of 2 or more newlines. But that's not the same as
  // multiline mode, which never returns empty records or records with leading
  // or trailing newlines, which can occur with RS="\n\n+". So here we loop and
  // strip leading/trailing newlines and discard empty lines. See gawk manual,
  // "4.9 Multiple-Line Records" for info on this difference.
  do {
    k = getr(zfp, 1);
    if (k < 0) break;
    while (k && TT.rgl.recptr[k-1] == '\n') k--;
    while (k && TT.rgl.recptr[0] == '\n') k--, TT.rgl.recptr++;
  } while (!k);
  return k;
}

static ssize_t getrec(void)
{
  ssize_t k;
  if (TT.rgl.eof) return -1;
  if (!TT.cfile->fp) next_fp();
  do {
    if ((k = getrec_f(TT.cfile)) >= 0) return k;
  } while (next_fp());
  return -1;
}

static ssize_t getrec_f0_f(struct zfile *zfp)
{
  ssize_t k = getrec_f(zfp);
  if (k >= 0) {
    copy_to_field0(TT.rgl.recptr, k);
  }
  return k;
}

static ssize_t getrec_f0(void)
{
  ssize_t k = getrec();
  if (k >= 0) {
    copy_to_field0(TT.rgl.recptr, k);
    incr_zvalue(&STACK[NR]);
    incr_zvalue(&STACK[FNR]);
  }
  return k;
}

// source is tkeof (no pipe/file), tklt (file), or tkpipe (pipe)
// fp is file or pipe (is NULL if file/pipe could not be opened)
// FIXME TODO should -1 return be replaced by test at caller?
// v is NULL or an lvalue ref
static int awk_getline(int source, struct zfile *zfp, struct zvalue *v)
{
  ssize_t k;
  int is_stream = source != tkeof;
  if (is_stream && !zfp->fp) return -1;
  if (v) {
    if ((k = is_stream ? getrec_f(zfp) : getrec()) < 0) return 0;
    zstring_release(&v->vst);
    v->vst = new_zstring(TT.rgl.recptr, k);
    v->flags = ZF_STR;
    check_numeric_string(v);    // bug fix 20240514
    if (!is_stream) {
      incr_zvalue(&STACK[NR]);
      incr_zvalue(&STACK[FNR]);
    }
  } else k = is_stream ? getrec_f0_f(zfp) : getrec_f0();
  return k < 0 ? 0 : 1;
}

// Define GAWK_SUB to get the same behavior with sub()/gsub() replacement text
// as with gawk, goawk, and recent bwk awk (nawk) versions. Undefine GAWK_SUB
// to get the simpler POSIX behavior, but I think most users will prefer the
// gawk behavior. See the gawk (GNU Awk) manual,
// sec. 9.1.4.1 // More about '\' and '&' with sub(), gsub(), and gensub()
// for details on the differences.
//
#undef GAWK_SUB
#define GAWK_SUB

// sub(ere, repl[, in]) Substitute the string repl in place of the
// first instance of the extended regular expression ERE in string 'in'
// and return the number of substitutions.  An <ampersand> ( '&' )
// appearing in the string repl shall be replaced by the string from in
// that matches the ERE. (partial spec... there's more)
static void gsub(int opcode, int nargs, int parmbase)
{ (void)nargs;
  int field_num = -1;
  // compile ensures 3 args
  struct zvalue *v = setup_lvalue(0, parmbase, &field_num);
  struct zvalue *ere = STKP-2;
  struct zvalue *repl = STKP-1;
  regex_t rx, *rxp = &rx;
  rx_zvalue_compile(&rxp, ere);
  to_str(repl);
  to_str(v);

#define SLEN(zvalp) ((zvalp)->vst->size)
  char *p, *rp0 = repl->vst->str, *rp = rp0, *s = v->vst->str;
  int namps = 0, nhits = 0, is_sub = (opcode == tksub), eflags = 0;
  regoff_t so = -1, eo;
  // Count ampersands in repl string; may be overcount due to \& escapes.
  for (rp = rp0; *rp; rp++) namps += *rp == '&';
  p = s;
  regoff_t need = SLEN(v) + 1;  // capacity needed for result string
  // A pass just to determine needed destination (result) string size.
  while(!rx_find(rxp, p, &so, &eo, eflags)) {
    need += SLEN(repl) + (eo - so) * (namps - 1);
    if (!*p) break;
    p += eo ? eo : 1; // ensure progress if empty hit at start
    if (is_sub) break;
    eflags |= REG_NOTBOL;
  }

  if (so >= 0) {  // at least one hit
    struct zstring *z = xzalloc(sizeof(*z) + need);
    z->capacity = need;

    char *e = z->str; // result destination pointer
    p = s;
    eflags = 0;
    char *ep0 = p, *sp, *ep;
    while(!rx_find(rxp, p, &so, &eo, eflags)) {
      sp = p + so;
      ep = p + eo;
      memmove(e, ep0, sp - ep0);  // copy unchanged part
      e += sp - ep0;
      // Skip match if not at start and just after prev match and this is empty
      if (p == s || sp - ep0 || eo - so) {
        nhits++;
        for (rp = rp0; *rp; rp++) { // copy replacement
          if (*rp == '&') {
            memmove(e, sp, eo - so);  //copy match
            e += eo - so;
          } else if (*rp == '\\') {
            if (rp[1] == '&') *e++ = *++rp;
            else if (rp[1] != '\\') *e++ = *rp;
            else {
#ifdef GAWK_SUB
              if (rp[2] == '\\' && rp[3] == '&') {
                rp += 2;
                *e++ = *rp;
              } else if (rp[2] != '&') *e++ = '\\';
#endif
              *e++ = *++rp;
            }
          } else *e++ = *rp;
        }
      }
      ep0 = ep;
      if (!*p) break;
      p += eo ? eo : 1; // ensure progress if empty hit at start
      if (is_sub) break;
      eflags |= REG_NOTBOL;
    }
    // copy remaining subject string
    memmove(e, ep0, s + SLEN(v) - ep0);
    e += s + SLEN(v) - ep0;
    *e = 0;
    z->size = e - z->str;
    zstring_release(&v->vst);
    v->vst = z;
  }
  rx_zvalue_free(rxp, ere);
  if (!IS_RX(STKP-2)) zstring_release(&STKP[-2].vst);
  drop_n(3);
  push_int_val(nhits);
  if (field_num >= 0) fixup_fields(field_num);
}

// Initially set stackp_needmore at MIN_STACK_LEFT before limit.
// When stackp > stackp_needmore, then expand and reset stackp_needmore
static void add_stack(struct zvalue **stackp_needmore)
{
  int k = stkn(0);  // stack elements in use
  zlist_expand(&TT.stack);
  STKP = (struct zvalue *)TT.stack.base + k;
  *stackp_needmore = (struct zvalue *)TT.stack.limit - MIN_STACK_LEFT;
}

#define CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))

// Main loop of interpreter. Run this once for all BEGIN rules (which
// have had their instructions chained in compile), all END rules (also
// chained in compile), and once for each record of the data file(s).
static int interpx(int start, int *status)
{
  int *ip = &ZCODE[start];
  int opcode, op2, k, r, nargs, nsubscrs, range_num, parmbase = 0;
  int field_num;
  double nleft, nright, d;
  double (*mathfunc[])(double) = {cos, sin, exp, log, sqrt, trunc};
  struct zvalue *v, vv,
        *stackp_needmore = (struct zvalue*)TT.stack.limit - MIN_STACK_LEFT;
  while ((opcode = *ip++)) {

    switch (opcode) {
      case opquit:
        return opquit;

      case tknot:
        (STKP)->num = ! get_set_logical();
        break;

      case opnotnot:
        get_set_logical();
        break;

      case opnegate:
        STKP->num = -to_num(STKP);
        break;

      case tkpow:         // FALLTHROUGH intentional here
      case tkmul:         // FALLTHROUGH intentional here
      case tkdiv:         // FALLTHROUGH intentional here
      case tkmod:         // FALLTHROUGH intentional here
      case tkplus:        // FALLTHROUGH intentional here
      case tkminus:
        nleft = to_num(STKP-1);
        nright = to_num(STKP);
        switch (opcode) {
          case tkpow: nleft = pow(nleft, nright); break;
          case tkmul: nleft *= nright; break;
          case tkdiv: nleft /= nright; break;
          case tkmod: nleft = fmod(nleft, nright); break;
          case tkplus: nleft += nright; break;
          case tkminus: nleft -= nright; break;
        }
        drop();
        STKP->num = nleft;
        break;

      // FIXME REDO REDO ?
      case tkcat:
        to_str(STKP-1);
        to_str(STKP);
        STKP[-1].vst = zstring_extend(STKP[-1].vst, STKP[0].vst);
        drop();
        break;

        // Comparisons (with the '<', "<=", "!=", "==", '>', and ">="
        // operators) shall be made numerically:
        // * if both operands are numeric,
        // * if one is numeric and the other has a string value that is a
        //   numeric string,
        // * if both have string values that are numeric strings, or
        // * if one is numeric and the other has the uninitialized value.
        //
        // Otherwise, operands shall be converted to strings as required and a
        // string comparison shall be made as follows:
        // * For the "!=" and "==" operators, the strings shall be compared to
        //   check if they are identical (not to check if they collate equally).
        // * For the other operators, the strings shall be compared using the
        //   locale-specific collation sequence.
        //
        // The value of the comparison expression shall be 1 if the relation is
        // true, or 0 if the relation is false.
      case tklt:          // FALLTHROUGH intentional here
      case tkle:          // FALLTHROUGH intentional here
      case tkne:          // FALLTHROUGH intentional here
      case tkeq:          // FALLTHROUGH intentional here
      case tkgt:          // FALLTHROUGH intentional here
      case tkge:
        ; int cmp = 31416;

        if (  (IS_NUM(&STKP[-1]) &&
              (STKP[0].flags & (ZF_NUM | ZF_NUMSTR) || !STKP[0].flags)) ||
              (IS_NUM(&STKP[0]) &&
              (STKP[-1].flags & (ZF_NUM | ZF_NUMSTR) || !STKP[-1].flags))) {
          switch (opcode) {
            case tklt: cmp = STKP[-1].num < STKP[0].num; break;
            case tkle: cmp = STKP[-1].num <= STKP[0].num; break;
            case tkne: cmp = STKP[-1].num != STKP[0].num; break;
            case tkeq: cmp = STKP[-1].num == STKP[0].num; break;
            case tkgt: cmp = STKP[-1].num > STKP[0].num; break;
            case tkge: cmp = STKP[-1].num >= STKP[0].num; break;
          }
        } else {
          cmp = strcmp(to_str(STKP-1)->vst->str, to_str(STKP)->vst->str);
          switch (opcode) {
            case tklt: cmp = cmp < 0; break;
            case tkle: cmp = cmp <= 0; break;
            case tkne: cmp = cmp != 0; break;
            case tkeq: cmp = cmp == 0; break;
            case tkgt: cmp = cmp > 0; break;
            case tkge: cmp = cmp >= 0; break;
          }
        }
        drop();
        drop();
        push_int_val(cmp);
        break;

      case opmatchrec:
        op2 = *ip++;
        int mret = match(&FIELD[0], &LITERAL[op2]);
        push_int_val(!mret);
        break;

      case tkmatchop:
      case tknotmatch:
        mret = match(STKP-1, STKP); // mret == 0 if match
        drop();
        drop();
        push_int_val(!mret == (opcode == tkmatchop));
        break;

      case tkpowasgn:     // FALLTHROUGH intentional here
      case tkmodasgn:     // FALLTHROUGH intentional here
      case tkmulasgn:     // FALLTHROUGH intentional here
      case tkdivasgn:     // FALLTHROUGH intentional here
      case tkaddasgn:     // FALLTHROUGH intentional here
      case tksubasgn:
        // Stack is: ... scalar_ref value_to_op_by
        // or ... subscript_val map_ref value_to_op_by
        // or ... fieldref value_to_op_by
        v = setup_lvalue(1, parmbase, &field_num);
        to_num(v);
        to_num(STKP);
        switch (opcode) {
          case tkpowasgn:
            // TODO
            v->num = pow(v->num, STKP->num);
            break;
          case tkmodasgn:
            // TODO
            v->num = fmod(v->num, STKP->num);
            break;
          case tkmulasgn:
            v->num *= STKP->num;
            break;
          case tkdivasgn:
            v->num /= STKP->num;
            break;
          case tkaddasgn:
            v->num += STKP->num;
            break;
          case tksubasgn:
            v->num -= STKP->num;
            break;
        }

        drop_n(2);
        v->flags = ZF_NUM;
        push_val(v);
        if (field_num >= 0) fixup_fields(field_num);
        break;

      case tkasgn:
        // Stack is: ... scalar_ref value_to_assign
        // or ... subscript_val map_ref value_to_assign
        // or ... fieldref value_to_assign
        v = setup_lvalue(1, parmbase, &field_num);
        force_maybemap_to_scalar(STKP);
        zvalue_copy(v, STKP);
        swap();
        drop();
        if (field_num >= 0) fixup_fields(field_num);
        break;

      case tkincr:        // FALLTHROUGH intentional here
      case tkdecr:        // FALLTHROUGH intentional here
      case oppreincr:     // FALLTHROUGH intentional here
      case oppredecr:
        // Stack is: ... scalar_ref
        // or ... subscript_val map_ref
        // or ... fieldnum fieldref
        v = setup_lvalue(0, parmbase, &field_num);
        to_num(v);
        switch (opcode) {
          case tkincr: case tkdecr:
            // Must be done in this order because push_val(v) may move v,
            // invalidating the pointer.
            v->num += (opcode == tkincr) ? 1 : -1;
            push_val(v);
            // Now reverse the incr/decr on the top TT.stack val.
            STKP->num -= (opcode == tkincr) ? 1 : -1;
            break;
          case oppreincr: case oppredecr:
            v->num += (opcode == oppreincr) ? 1 : -1;
            push_val(v);
            break;
        }
        swap();
        drop();
        if (field_num >= 0) fixup_fields(field_num);
        break;

      case tknumber:      // FALLTHROUGH intentional here
      case tkstring:      // FALLTHROUGH intentional here
      case tkregex:
        push_val(&LITERAL[*ip++]);
        break;

      case tkprint:
      case tkprintf:
        nargs = *ip++;
        int outmode = *ip++;
        struct zfile *outfp = TT.zstdout;
        switch (outmode) {
          case tkgt: outfp = setup_file(1, "w"); break;     // file
          case tkappend: outfp = setup_file(1, "a"); break; // file
          case tkpipe: outfp = setup_file(0, "w"); break;   // pipe
          default: nargs++; break;
        }
        nargs--;
        if (opcode == tkprintf) {
          varprint(fprintf, outfp->fp, nargs);
          drop_n(nargs);
          break;
        }
        if (!nargs) {
          fprintf(outfp->fp, "%s", to_str(&FIELD[0])->vst->str);
        } else {
          struct zvalue tempv = uninit_zvalue;
          zvalue_copy(&tempv, &STACK[OFS]);
          to_str(&tempv);
          for (int k = 0; k < nargs; k++) {
            if (k) fprintf(outfp->fp, "%s", tempv.vst->str);
            int sp = stkn(nargs - 1 - k);
            ////// FIXME refcnt -- prob. don't need to copy from TT.stack?
            v = &STACK[sp];
            to_str_fmt(v, OFMT);
            struct zstring *zs = v->vst;
            fprintf(outfp->fp, "%s", zs ? zs->str : "");
          }
          zvalue_release_zstring(&tempv);
          drop_n(nargs);
        }
        fputs(ENSURE_STR(&STACK[ORS])->vst->str, outfp->fp);
        break;

      case opdrop:
        drop();
        break;

      case opdrop_n:
        drop_n(*ip++);
        break;

        // Stack frame layout relative to parmbase:
#define RETURN_VALUE    -4
#define RETURN_ADDR     -3
#define PREV_PARMBASE   -2
#define ARG_CNT         -1
#define FUNCTION_NUM    0
        // Actual args follow, starting at parmbase + 1
      case tkfunction:    // function definition
        op2 = *ip++;    // func table num
        struct functab_slot *pfdef = &FUNC_DEF[op2];
        struct zlist *loctab = &pfdef->function_locals;
        int nparms = zlist_len(loctab)-1;

        nargs = popnumval();
        int newparmbase = stkn(nargs);
        STACK[newparmbase + PREV_PARMBASE].num = parmbase;
        parmbase = newparmbase;
        for ( ;nargs > nparms; nargs--)
          drop();
        for ( ;nargs < nparms; nargs++) {
          // Push additional "args" that were not passed by the caller, to
          // match the formal parameters (parms) defined in the function
          // definition. In the local var table we may have the type as scalar
          // or map if it is used as such within the function. In that case we
          // init the pushed arg from the type of the locals table.
          // But if a var appears only as a bare arg in a function call it will
          // not be typed in the locals table. In that case we can only say it
          // "may be" a map, but we have to assume the possibility and attach a
          // map to the var. When/if the var is used as a map or scalar in the
          // called function it will be converted to a map or scalar as
          // required.
          // See force_maybemap_to_scalar().
          struct symtab_slot *q = &((struct symtab_slot *)loctab->base)[nargs+1];
          vv = (struct zvalue)ZVINIT(q->flags, 0, 0);
          if (vv.flags == 0) {
            zvalue_map_init(&vv);
            vv.flags = ZF_MAYBEMAP;
          } else if (IS_MAP(&vv)) {
            zvalue_map_init(&vv);
          } else {
            vv.flags = 0;
          }
          push_val(&vv);
        }
        break;

      case tkreturn:
        nparms = *ip++;
        nargs = STACK[parmbase+ARG_CNT].num;
        force_maybemap_to_scalar(STKP); // Unneeded?
        zvalue_copy(&STACK[parmbase+RETURN_VALUE], STKP);
        drop();
        // Remove the local args (not supplied by caller) from TT.stack, check to
        // release any map data created.
        while (stkn(0) > parmbase + nargs) {
          if ((STKP)->flags & ZF_ANYMAP) {
            zmap_delete_map_incl_slotdata((STKP)->map);
            xfree((STKP)->map);
          }
          drop();
        }
        while (stkn(0) > parmbase + RETURN_VALUE)
          drop();
        ip = &ZCODE[(int)STACK[parmbase+RETURN_ADDR].num];
        parmbase = STACK[parmbase+PREV_PARMBASE].num;
        break;

      case opprepcall:    // function call prep
        if (STKP > stackp_needmore) add_stack(&stackp_needmore);
        push_int_val(0);      // return value placeholder
        push_int_val(0);      // return addr
        push_int_val(0);      // parmbase
        push_int_val(0);      // arg count
        push_int_val(*ip++);  // function tbl ref
        break;

      case tkfunc:        // function call
        nargs = *ip++;
        newparmbase = stkn(nargs);
        STACK[newparmbase+RETURN_ADDR].num = ip - &ZCODE[0];
        STACK[newparmbase+ARG_CNT].num = nargs;
        push_int_val(nargs);      // FIXME TODO pass this in a zregister?
        ip = &ZCODE[FUNC_DEF[(int)STACK[newparmbase+FUNCTION_NUM].num].zcode_addr];
        break;

      case tkrbracket:    // concat multiple map subscripts
        nsubscrs = *ip++;
        while (--nsubscrs) {
          swap();
          to_str(STKP);
          push_val(&STACK[SUBSEP]);
          to_str(STKP);
          STKP[-1].vst = zstring_extend(STKP[-1].vst, STKP->vst);
          drop();
          swap();
          to_str(STKP);
          STKP[-1].vst = zstring_extend(STKP[-1].vst, STKP->vst);
          drop();
        }
        break;

      case opmapdelete:
      case tkdelete:
        k = STKP->num;
        if (k < 0) k = parmbase - k;    // loc of var on TT.stack
        v = &STACK[k];
        force_maybemap_to_map(v);
        if (opcode == opmapdelete) {
          zmap_delete_map(v->map);
        } else {
          drop();
          zmap_delete(v->map, to_str(STKP)->vst);
        }
        drop();
        break;

      case opmap:
        op2 = *ip++;
        k = op2 < 0 ? parmbase - op2 : op2;
        v = &STACK[k];
        force_maybemap_to_map(v);
        if (!IS_MAP(v)) FATAL("scalar in array context");
        v = get_map_val(v, STKP);
        drop();     // drop subscript
        push_val(v);
        break;

      case tkin:
        if (!(STKP->flags & ZF_ANYMAP)) FATAL("scalar in array context");
        v = zmap_find(STKP->map, to_str(STKP-1)->vst);
        drop();
        drop();
        push_int_val(v ? 1 : 0);
        break;

      case opmapiternext:
        op2 = *ip++;
        v = STKP-1;
        force_maybemap_to_map(v);
        if (!IS_MAP(v)) FATAL("scalar in array context");
        struct zmap *m = v->map;   // Need for MAPSLOT macro
        int zlen = zlist_len(&m->slot);
        int kk = STKP->num + 1;
        while (kk < zlen && !(MAPSLOT[kk].key)) // skip deleted slots
          kk++;
        STKP->num = kk; // save index for next iteration
        if (kk < zlen) {
          struct zvalue *var = setup_lvalue(2, parmbase, &field_num);
          var->flags = ZF_STR;
          zstring_release(&var->vst);
          var->vst = MAPSLOT[kk].key;
          zstring_incr_refcnt(var->vst);
          ip += op2;
        }
        break;

      case tkvar:
        op2 = *ip++;
        k = op2 < 0 ? parmbase - op2 : op2;
        v = &STACK[k];
        push_val(v);
        break;

      case tkfield:
        // tkfield op has "dummy" 2nd word so that convert_push_to_reference(void)
        // can find either tkfield or tkvar at same place (ZCODE[TT.zcode_last-1]).
        ip++; // skip dummy "operand" instruction field
        push_field((int)(to_num(STKP)));

        swap();
        drop();
        break;

      case oppush:
        push_int_val(*ip++);
        break;

      case tkand:
        op2 = *ip++;
        if (get_set_logical()) drop();
        else ip += op2;
        break;

      case tkor:
        op2 = *ip++;
        if (!get_set_logical()) drop();
        else ip += op2;
        break;

      case tkwhile:
        (STKP)->num = ! get_set_logical();
        ATTR_FALLTHROUGH_INTENDED;
        // FALLTHROUGH to tkternif
      case tkif:
        // FALLTHROUGH to tkternif
      case tkternif:
        op2 = *ip++;
        int t = get_set_logical();  // FIXME only need to get, not set
        drop();
        if (!t) ip += op2;
        break;

      case tkelse:        // FALLTHROUGH intentional here
      case tkternelse:    // FALLTHROUGH intentional here
      case tkbreak:       // FALLTHROUGH intentional here
      case tkcontinue:    // FALLTHROUGH intentional here
      case opjump:
        op2 = *ip++;
        ip += op2;
        break;

      case opvarref:
        op2 = *ip++;
        vv = (struct zvalue)ZVINIT(ZF_REF, op2, 0);
        push_val(&vv);
        break;

      case opmapref:
        op2 = *ip++;
        vv = (struct zvalue)ZVINIT(ZF_MAPREF, op2, 0);
        push_val(&vv);
        break;

      case opfldref:
        to_num(STKP);
        (STKP)->flags |= ZF_FIELDREF;
        ip++; // skip dummy "operand" instruction field
        break;

      case opprintrec:
        puts(to_str(&FIELD[0])->vst->str);
        break;

      case oprange1:
        range_num = *ip++;
        op2 = *ip++;
        if (TT.range_sw[range_num]) ip += op2;
        break;

      case oprange2:
        range_num = *ip++;
        op2 = *ip++;
        t = get_set_logical();  // FIXME only need to get, not set
        drop();
        if (t) TT.range_sw[range_num] = 1;
        else ip += op2;
        break;

      case oprange3:
        range_num = *ip++;
        t = get_set_logical();  // FIXME only need to get, not set
        drop();
        if (t) TT.range_sw[range_num] = 0;
        break;

      case tkexit:
        r = popnumval();
        if (r != NO_EXIT_STATUS) *status = (int)r & 255;
        // TODO FIXME do we need NO_EXIT_STATUS at all? Just use 0?
        ATTR_FALLTHROUGH_INTENDED;
      case tknext:
      case tknextfile:
        return opcode;

      case tkgetline:
        nargs = *ip++;
        int source = *ip++;
        // TT.stack is:
        // if tkgetline 0 tkeof:   (nothing stacked; plain getline)
        // if tkgetline 1 tkeof:   (lvalue)
        // if tkgetline 1 tklt:    (filename_string)
        // if tkgetline 2 tklt:    (lvalue) (filename_string)
        // if tkgetline 1 tkpipe:  (pipe_command_string)
        // if tkgetline 2 tkpipe:  (pipe_command_string) (lvalue)
        // effect is to set:
        // if tkgetline 0 tkeof:   $0 NF NR FNR
        // if tkgetline 1 tkeof:   var NR FNR
        // if tkgetline 1 tklt:    $0 NF
        // if tkgetline 2 tklt:    var
        // if tkgetline 1 tkpipe:  $0 NF
        // if tkgetline 2 tkpipe:  var
        // Ensure pipe cmd on top
        if (nargs == 2 && source == tkpipe) swap();
        struct zfile *zfp = 0;
        if (source == tklt || source == tkpipe) {
          zfp = setup_file(source == tklt, "r");
          nargs--;
        }
        // now cases are:
        // nargs source  TT.stack
        //  0 tkeof:   (nothing; plain getline) from current data file
        //  1 tkeof:   (lvalue)  from current data file
        //  0 tklt:    (nothing) from named file in 'stream'
        //  1 tklt:    (lvalue)  from  named file in 'stream'
        //  0 tkpipe:  (nothing) from piped command in 'stream'
        //  1 tkpipe:  (lvalue)  from piped command in 'stream'
        v = nargs ? setup_lvalue(0, parmbase, &field_num) : 0;
        if (v) drop();
        // source is tkeof (no pipe/file), tklt (file), or tkpipe (pipe)
        // stream is name of file or pipe
        // v is NULL or an lvalue ref
        if (zfp != badfile) push_int_val(awk_getline(source, zfp, v));
        else push_int_val(-1);

        // fake return value for now
        break;

        ////// builtin functions ///////

      case tksplit:
        nargs = *ip++;
        if (nargs == 2) push_val(&STACK[FS]);
        struct zstring *s = to_str(STKP-2)->vst;
        force_maybemap_to_map(STKP-1);
        struct zvalue *a = STKP-1;
        struct zvalue *fs = STKP;
        zmap_delete_map(a->map);
        k = split(s, a, fs);
        drop_n(3);
        push_int_val(k);
        break;

      case tkmatch:
        nargs = *ip++;
        if (!IS_RX(STKP)) to_str(STKP);
        regex_t rx_pat, *rxp = &rx_pat;
        rx_zvalue_compile(&rxp, STKP);
        regoff_t rso = 0, reo = 0;  // shut up warning (may be uninit)
        k = rx_find(rxp, to_str(STKP-1)->vst->str, &rso, &reo, 0);
        rx_zvalue_free(rxp, STKP);
        // Force these to num before setting.
        to_num(&STACK[RSTART]);
        to_num(&STACK[RLENGTH]);
        if (k) STACK[RSTART].num = 0, STACK[RLENGTH].num = -1;
        else {
          reo = utf8cnt(STKP[-1].vst->str, reo);
          rso = utf8cnt(STKP[-1].vst->str, rso);
          STACK[RSTART].num = rso + 1, STACK[RLENGTH].num = reo - rso;
        }
        drop();
        drop();
        push_int_val(k ? 0 : rso + 1);
        break;

      case tksub:
      case tkgsub:
        gsub(opcode, *ip++, parmbase);  // tksub/tkgsub, args
        break;

      case tksubstr:
        nargs = *ip++;
        struct zstring *zz = to_str(STKP - nargs + 1)->vst;
        int nchars = utf8cnt(zz->str, zz->size);  // number of utf8 codepoints
        // Offset of start of string (in chars not bytes); convert 1-based to 0-based
        ssize_t mm = CLAMP(trunc(to_num(STKP - nargs + 2)) - 1, 0, nchars);
        ssize_t nn = nchars - mm;   // max possible substring length (chars)
        if (nargs == 3) nn = CLAMP(trunc(to_num(STKP)), 0, nn);
        mm = bytesinutf8(zz->str, zz->size, mm);
        nn = bytesinutf8(zz->str + mm, zz->size - mm, nn);
        struct zstring *zzz = new_zstring(zz->str + mm, nn);
        zstring_release(&(STKP - nargs + 1)->vst);
        (STKP - nargs + 1)->vst = zzz;
        drop_n(nargs - 1);
        break;

      case tkindex:
        nargs = *ip++;
        char *s1 = to_str(STKP-1)->vst->str;
        char *s3 = strstr(s1, to_str(STKP)->vst->str);
        ptrdiff_t offs = s3 ? utf8cnt(s1, s3 - s1) + 1 : 0;
        drop();
        drop();
        push_int_val(offs);
        break;

      case tkband:
      case tkbor:
      case tkbxor:
      case tklshift:
      case tkrshift:
        ; size_t acc = to_num(STKP);
        nargs = *ip++;
        for (int i = 1; i < nargs; i++) switch (opcode) {
          case tkband: acc &= (size_t)to_num(STKP-i); break;
          case tkbor:  acc |= (size_t)to_num(STKP-i); break;
          case tkbxor: acc ^= (size_t)to_num(STKP-i); break;
          case tklshift: acc = (size_t)to_num(STKP-i) << acc; break;
          case tkrshift: acc = (size_t)to_num(STKP-i) >> acc; break;
        }
        drop_n(nargs);
        push_int_val(acc);
        break;

      case tktolower:
      case tktoupper:
        nargs = *ip++;
        struct zstring *z = to_str(STKP)->vst;
        unsigned zzlen = z->size + 4; // Allow for expansion
        zz = zstring_update(0, zzlen, "", 0);
        char *p = z->str, *e = z->str + z->size, *q = zz->str;
        // Similar logic to toybox strlower(), but fixed.
        while (p < e) {
          unsigned wch;
          int len = utf8towc(&wch, p, e-p);
          if (len < 1) {  // nul byte, error, or truncated code
            *q++ = *p++;
            continue;
          }
          p += len;
          wch = (opcode == tktolower ? towlower : towupper)(wch);
          len = wctoutf8(q, wch);
          q += len;
          // Need realloc here if overflow possible
          if ((len = q - zz->str) + 4 < (int)zzlen) continue;
          zz = zstring_update(zz, zzlen = len + 16, "", 0);
          q = zz->str + len;
        }
        *q = 0;
        zz->size = q - zz->str;
        zstring_release(&z);
        STKP->vst = zz;
        break;

      case tklength:
        nargs = *ip++;
        v = nargs ? STKP : &FIELD[0];
        force_maybemap_to_map(v);
        if (IS_MAP(v)) k = v->map->count - v->map->deleted;
        else {
          to_str(v);
          k = utf8cnt(v->vst->str, v->vst->size);
        }
        if (nargs) drop();
        push_int_val(k);
        break;

      case tksystem:
        nargs = *ip++;
        fflush(stdout);
        fflush(stderr);
        r = system(to_str(STKP)->vst->str);
#ifdef WEXITSTATUS
        // WEXITSTATUS is in sys/wait.h, but I'm not including that.
        // It seems to also be in stdlib.h in gcc and musl-gcc.
        // No idea how portable this is!
        if (WIFEXITED(r)) r = WEXITSTATUS(r);
#endif
        drop();
        push_int_val(r);
        break;

      case tkfflush:
        nargs = *ip++;
        r = fflush_file(nargs);
        if (nargs) drop();
        push_int_val(r);
        break;

      case tkclose:
        nargs = *ip++;
        r = close_file(to_str(STKP)->vst->str);
        drop();
        push_int_val(r);
        break;

      case tksprintf:
        nargs = *ip++;
        zstring_release(&TT.rgl.zspr);
        TT.rgl.zspr = new_zstring("", 0);
        varprint(fsprintf, 0, nargs);
        drop_n(nargs);
        vv = (struct zvalue)ZVINIT(ZF_STR, 0, TT.rgl.zspr);
        push_val(&vv);
        break;

      // Math builtins -- move here (per Oliver Webb suggestion)
      case tkatan2:
        nargs = *ip++;
        d = atan2(to_num(STKP-1), to_num(STKP));
        drop();
        STKP->num = d;
        break;
      case tkrand:
        nargs = *ip++;
        push_int_val(0);
        // Get all 53 mantissa bits in play:
        // (upper 26 bits * 2^27 + upper 27 bits) / 2^53
        STKP->num =
          ((random() >> 5) * 134217728.0 + (random() >> 4)) / 9007199254740992.0;
        break;
      case tksrand:
        nargs = *ip++;
        if (nargs == 1) {
          STKP->num = seedrand(to_num(STKP));
        } else push_int_val(seedrand(time(0)));
        break;
      case tkcos: case tksin: case tkexp: case tklog: case tksqrt: case tkint:
        nargs = *ip++;
        STKP->num = mathfunc[opcode-tkcos](to_num(STKP));
        break;

      default:
        // This should never happen:
        error_exit("!!! Unimplemented opcode %d", opcode);
    }
  }
  return opquit;
}

// interp() wraps the main interpreter loop interpx(). The main purpose
// is to allow the TT.stack to be readjusted after an 'exit' from a function.
// Also catches errors, as the normal operation should leave the TT.stack
// depth unchanged after each run through the rules.
static int interp(int start, int *status)
{
  int stkptrbefore = stkn(0);
  int r = interpx(start, status);
  // If exit from function, TT.stack will be loaded with args etc. Clean it.
  if (r == tkexit) {
    // TODO FIXME is this safe? Just remove extra entries?
    STKP = &STACK[stkptrbefore];
  }
  if (stkn(0) - stkptrbefore)
    error_exit("!!AWK BUG stack pointer offset: %d", stkn(0) - stkptrbefore);
  return r;
}

static void insert_argv_map(struct zvalue *map, int key, char *value)
{
  struct zvalue zkey = ZVINIT(ZF_STR, 0, num_to_zstring(key, ENSURE_STR(&STACK[CONVFMT])->vst->str));
  struct zvalue *v = get_map_val(map, &zkey);
  zvalue_release_zstring(&zkey);
  zvalue_release_zstring(v);
  *v = new_str_val(value);
  check_numeric_string(v);
}

static void init_globals(int optind, int argc, char **argv, char *sepstring,
    struct arg_list *assign_args)
{
  // Global variables reside at the bottom of the TT.stack. Start with the awk
  // "special variables":  ARGC, ARGV, CONVFMT, ENVIRON, FILENAME, FNR, FS, NF,
  // NR, OFMT, OFS, ORS, RLENGTH, RS, RSTART, SUBSEP

  STACK[CONVFMT] = new_str_val("%.6g");
  // Init ENVIRON map.
  struct zvalue m = ZVINIT(ZF_MAP, 0, 0);
  zvalue_map_init(&m);
  STACK[ENVIRON] = m;
  for (char **pkey = environ; *pkey; pkey++) {
    char *pval = strchr(*pkey, '=');
    if (!pval) continue;
    struct zvalue zkey = ZVINIT(ZF_STR, 0, new_zstring(*pkey, pval - *pkey));
    struct zvalue *v = get_map_val(&m, &zkey);
    zstring_release(&zkey.vst);
    if (v->vst) FFATAL("env var dup? (%s)", pkey);
    *v = new_str_val(++pval);    // FIXME refcnt
    check_numeric_string(v);
  }

  // Init ARGV map.
  m = (struct zvalue)ZVINIT(ZF_MAP, 0, 0);
  zvalue_map_init(&m);
  STACK[ARGV] = m;
  insert_argv_map(&m, 0, TT.progname);
  int nargc = 1;
  for (int k = optind; k < argc; k++) {
    insert_argv_map(&m, nargc, argv[k]);
    nargc++;
  }

  // Init rest of the awk special variables.
  STACK[ARGC] = (struct zvalue)ZVINIT(ZF_NUM, nargc, 0);
  STACK[FILENAME] = new_str_val("");
  STACK[FNR] = (struct zvalue)ZVINIT(ZF_NUM, 0, 0);
  STACK[FS] = new_str_val(sepstring);
  STACK[NF] = (struct zvalue)ZVINIT(ZF_NUM, 0, 0);
  STACK[NR] = (struct zvalue)ZVINIT(ZF_NUM, 0, 0);
  STACK[OFMT] = new_str_val("%.6g");
  STACK[OFS] = new_str_val(" ");
  STACK[ORS] = new_str_val("\n");
  STACK[RLENGTH] = (struct zvalue)ZVINIT(ZF_NUM, 0, 0);
  STACK[RS] = new_str_val("\n");
  STACK[RSTART] = (struct zvalue)ZVINIT(ZF_NUM, 0, 0);
  STACK[SUBSEP] = new_str_val("\034");

  // Init program globals.
  //
  // Push global variables on the TT.stack at offsets matching their index in the
  // global var table.  In the global var table we may have the type as scalar
  // or map if it is used as such in the program. In that case we init the
  // pushed arg from the type of the globals table.
  // But if a global var appears only as a bare arg in a function call it will
  // not be typed in the globals table. In that case we can only say it "may be"
  // a map, but we have to assume the possibility and attach a map to the
  // var. When/if the var is used as a map or scalar in the called function it
  // will be converted to a map or scalar as required.
  // See force_maybemap_to_scalar(), and the similar comment in
  // 'case tkfunction:' above.
  //
  int gstx, len = zlist_len(&TT.globals_table);
  for (gstx = TT.spec_var_limit; gstx < len; gstx++) {
    struct symtab_slot gs = GLOBAL[gstx];
    struct zvalue v = ZVINIT(gs.flags, 0, 0);
    if (v.flags == 0) {
      zvalue_map_init(&v);
      v.flags = ZF_MAYBEMAP;
    } else if (IS_MAP(&v)) {
      zvalue_map_init(&v);
    } else {
      // Set SCALAR flag 0 to create "uninitialized" scalar.
      v.flags = 0;
    }
    push_val(&v);
  }

  // Init -v assignment options.
  for (struct arg_list *p = assign_args; p; p = p->next) {
    char *asgn = p->arg;
    char *val = strchr(asgn, '=');
    if (!val) error_exit("bad -v assignment format");
    *val++ = 0;
    assign_global(asgn, val);
  }

  TT.rgl.cur_arg = new_str_val("<cmdline>");
  uninit_string_zvalue = new_str_val("");
  zvalue_copy(&FIELD[0], &uninit_string_zvalue);
}

static void run_files(int *status)
{
  int r = 0;
  while (r != tkexit && *status < 0 && getrec_f0() >= 0)
    if ((r = interp(TT.cgl.first_recrule, status)) == tknextfile) next_fp();
}

static void free_literal_regex(void)
{
  int len = zlist_len(&TT.literals);
  for (int k = 1; k < len; k++)
    if (IS_RX(&LITERAL[k])) regfree(LITERAL[k].rx);
}

static void run(int optind, int argc, char **argv, char *sepstring,
    struct arg_list *assign_args)
{
  char *printf_fmt_rx = "%[-+ #0']*([*]|[0-9]*)([.]([*]|[0-9]*))?l?[aAdiouxXfFeEgGcs%]";
  init_globals(optind, argc, argv, sepstring, assign_args);
  TT.cfile = xzalloc(sizeof(struct zfile));
  xregcomp(&TT.rx_default, "[ \t\n]+", REG_EXTENDED);
  xregcomp(&TT.rx_last, "[ \t\n]+", REG_EXTENDED);
  xregcomp(&TT.rx_printf_fmt, printf_fmt_rx, REG_EXTENDED);
  new_file("-", stdin, 'r', 1, 1);
  new_file("/dev/stdin", stdin, 'r', 1, 1);
  new_file("/dev/stdout", stdout, 'w', 1, 1);
  TT.zstdout = TT.zfiles;
  new_file("/dev/stderr", stderr, 'w', 1, 1);
  seedrand(1);
  int status = -1, r = 0;
  if (TT.cgl.first_begin) r = interp(TT.cgl.first_begin, &status);
  if (r != tkexit)
    if (TT.cgl.first_recrule) run_files(&status);
  if (TT.cgl.first_end) r = interp(TT.cgl.first_end, &status);
  regfree(&TT.rx_printf_fmt);
  regfree(&TT.rx_default);
  regfree(&TT.rx_last);
  free_literal_regex();
  close_file(0);    // close all files
  if (status >= 0) awk_exit(status);
}

////////////////////
//// main
////////////////////

static void progfiles_init(char *progstring, struct arg_list *prog_args)
{
  TT.scs->p = progstring ? progstring : "  " + 2;
  TT.scs->progstring = progstring;
  TT.scs->prog_args = prog_args;
  TT.scs->filename = "(cmdline)";
  TT.scs->maxtok = 256;
  TT.scs->tokstr = xzalloc(TT.scs->maxtok);
}

static int awk(char *sepstring, char *progstring, struct arg_list *prog_args,
    struct arg_list *assign_args, int optind, int argc, char **argv,
    int opt_run_prog)
{
  struct scanner_state ss = {0};
  TT.scs = &ss;

  setlocale(LC_NUMERIC, "");
  progfiles_init(progstring, prog_args);
  compile();

  if (TT.cgl.compile_error_count)
    error_exit("%d syntax error(s)", TT.cgl.compile_error_count);
  else {
    if (opt_run_prog)
      run(optind, argc, argv, sepstring, assign_args);
  }

  return TT.cgl.compile_error_count;
}

void awk_main(void)
{
  char *sepstring = TT.F ? escape_str(TT.F, 0) : " ";
  int optind = 0;
  char *progstring = NULL;

  TT.pbuf = toybuf;
  toys.exitval = 2;
  if (!TT.f) {
    if (*toys.optargs) progstring = toys.optargs[optind++];
    else error_exit("No program string\n");
  }
  TT.progname = toys.which->name;
  toys.exitval = awk(sepstring, progstring, TT.f, TT.v,
      optind, toys.optc, toys.optargs, !FLAG(c));
}
