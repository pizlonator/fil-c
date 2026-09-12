#ifndef _FTS_H
#include <io/fts.h>

#ifndef _ISOMAC
# if __TIMESIZE != 64
#  include <sys/stat.h>

typedef struct
{
  struct _ftsent64_time64 *fts_cur;
  struct _ftsent64_time64 *fts_child;
  struct _ftsent64_time64 **fts_array;
  dev_t fts_dev;
  char *fts_path;
  int fts_rfd;
  int fts_pathlen;
  int fts_nitems;
  int (*fts_compar) (const void *, const void *);
  int fts_options;
  int fts_cwd_fd;
  struct hash_table *fts_leaf_optimization_works_ht;
  union {
	  struct hash_table *ht;
	  struct cycle_check_state *state;
  } fts_cycle;
  __I_ring fts_fd_ring;
} FTS64_TIME64;

typedef struct _ftsent64_time64
{
  struct _ftsent64_time64 *fts_cycle;
  struct _ftsent64_time64 *fts_parent;
  struct _ftsent64_time64 *fts_link;
  long fts_number;
  void *fts_pointer;
  char *fts_accpath;
  char *fts_path;
  int fts_errno;
  int fts_symfd;
  unsigned short fts_pathlen;
  unsigned short fts_namelen;

  ino64_t fts_ino;
  dev_t fts_dev;
  nlink_t fts_nlink;

  short fts_level;
  unsigned short fts_info;
  unsigned short fts_flags;
  unsigned short fts_instr;

  struct __stat64_t64 *fts_statp;
  char fts_name[1];
} FSTENT64_TIME64;

# endif
#endif

#endif /* _FTS_H  */
