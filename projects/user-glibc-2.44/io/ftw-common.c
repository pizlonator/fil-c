/* File tree walker functions.
   Copyright (C) 1996-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */


#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <ftw.h>
#include <not-cancel.h>
#include <search.h>
#include <unistd.h>
#include <sys/param.h>

#define NAMLEN(dirent) _D_EXACT_NAMLEN (dirent)

/* Support for the LFS API version.  */
#ifndef FTW_NAME
# define FTW_NAME ftw
# define NFTW_NAME nftw
# define NFTW_OLD_NAME __old_nftw
# define NFTW_NEW_NAME __new_nftw
# define INO_T ino_t
# define STRUCT_STAT stat
# define LSTAT __lstat
# define STAT __stat
# define FSTATAT __fstatat
# define FTW_FUNC_T __ftw_func_t
# define NFTW_FUNC_T __nftw_func_t
#endif

/* We define PATH_MAX if the system does not provide a definition.
   This does not artificially limit any operation.  PATH_MAX is simply
   used as a guesstimate for the expected maximal path length.
   Buffers will be enlarged if necessary.  */
#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

struct dir_data
{
  DIR *stream;
  int streamfd;
  char *content;
};

struct known_object
{
  dev_t dev;
  INO_T ino;
};

/* Represents the execution state of a directory processing frame within the
   iterative file tree walk loop.

   Because the tree traversal is implemented iteratively using a custom stack
   rather than standard recursion, this state machine tracks the progress
   of each directory currently being visited.  */
enum ftw_frame_state
{
  /* The initial state of a newly pushed directory frame.  Attempts to open
     the directory stream.  If successful, transitions to
     FTW_STATE_STREAM_LOOP.  */
  FTW_STATE_INIT = 0,

  /* Iterating over the directory entries directly from the open DIR stream
     (using readdir).  If a subdirectory is encountered and needs to be
     descended into, a new frame is added to the stack and execution pauses
     here.  Transitions to FTW_STATE_CONTENT_LOOP if the stream was closed
     and cached to free up file descriptors, or FTW_STATE_CLEANUP when
     done.  */
  FTW_STATE_STREAM_LOOP,

  /* Iterating over directory entries from a cached memory buffer.  This state
     is used as a fallback when the original DIR stream had to be closed
     prematurely to prevent file descriptor exhaustion while descending into
     deeply nested child directories.  Transitions to FTW_STATE_CLEANUP when
     all cached entries are processed.  */
  FTW_STATE_CONTENT_LOOP,

  /* The final state, handles resource deallocation (closing remaining
     streams, freeing cached content buffers), triggering post-traversal
     callbacks (like FTW_DP for FTW_DEPTH walks), and restoring the
     previous working directory if FTW_CHDIR was used.  */
  FTW_STATE_CLEANUP
};

/* Keep track of visited directories.  */
struct ftw_frame
{
  struct dir_data dir;
  struct STRUCT_STAT st;
  int previous_base;
  char *runp;
  enum ftw_frame_state state;
};

struct ftw_stack
{
  struct ftw_frame **stack;
  size_t num_blocks;
  ssize_t top;
};

typedef union
{
  NFTW_FUNC_T nftw_func;
  FTW_FUNC_T ftw_func;
} func_callback_t;

struct ftw_data
{
  /* Array with pointers to open directory streams.  */
  struct dir_data **dirstreams;
  size_t actdir;
  size_t maxdir;

  /* Buffer containing name of currently processed object.  */
  char *dirbuf;
  size_t dirbufsize;

  /* Passed as fourth argument to `nftw' callback.  The `base' member
     tracks the content of the `dirbuf'.  */
  struct FTW ftw;

  /* Flags passed to `nftw' function.  0 for `ftw'.  */
  int flags;

  /* Conversion array for flag values.  It is the identity mapping for
     `nftw' calls, otherwise it maps the values to those known by
     `ftw'.  */
  const int *cvt_arr;

  /* Callback function.  We always use the `nftw' form.  */
  bool is_nftw;
  func_callback_t func;

  /* Device of starting point.  Needed for FTW_MOUNT.  */
  dev_t dev;

  /* Data structure for keeping fingerprints of already processed
     object.  This is needed when not using FTW_PHYS.  */
  void *known_objects;
};
#define CALL_FUNC(__ftw_data, __fp, __sb, __f, __ftw)                            \
  ((__ftw_data)->is_nftw ? (__ftw_data)->func.nftw_func (__fp, __sb, __f, __ftw) \
                         : (__ftw_data)->func.ftw_func (__fp, __sb, __f))

static bool
ftw_allocate (struct ftw_data *data, size_t newsize)
{
  void *newp = realloc (data->dirstreams, data->maxdir
					  * sizeof (struct dir_data *)
					  + newsize);
  if (newp == NULL)
    return false;
  data->dirstreams = newp;
  data->dirbufsize = newsize;
  data->dirbuf = (char *) data->dirstreams
		 + data->maxdir * sizeof (struct dir_data *);
  return true;
}

/* Internally we use the FTW_* constants used for `nftw'.  When invoked
   as `ftw', map each flag to the subset of values used by `ftw'.  */
static const int nftw_arr[] =
{
  FTW_F, FTW_D, FTW_DNR, FTW_NS, FTW_SL, FTW_DP, FTW_SLN
};

static const int ftw_arr[] =
{
  FTW_F, FTW_D, FTW_DNR, FTW_NS, FTW_F, FTW_D, FTW_NS
};


static int
object_compare (const void *p1, const void *p2)
{
  /* We don't need a sophisticated and useful comparison.  We are only
     interested in equality.  However, we must be careful not to
     accidentally compare `holes' in the structure.  */
  const struct known_object *kp1 = p1, *kp2 = p2;
  int cmp1;
  cmp1 = (kp1->ino > kp2->ino) - (kp1->ino < kp2->ino);
  if (cmp1 != 0)
    return cmp1;
  return (kp1->dev > kp2->dev) - (kp1->dev < kp2->dev);
}


static int
add_object (struct ftw_data *data, struct STRUCT_STAT *st)
{
  struct known_object *newp = malloc (sizeof (struct known_object));
  if (newp == NULL)
    return -1;
  newp->dev = st->st_dev;
  newp->ino = st->st_ino;
  return __tsearch (newp, &data->known_objects, object_compare) ? 0 : -1;
}


static inline int
find_object (struct ftw_data *data, struct STRUCT_STAT *st)
{
  struct known_object obj;
  obj.dev = st->st_dev;
  obj.ino = st->st_ino;
  return __tfind (&obj, &data->known_objects, object_compare) != NULL;
}


static inline int
open_dir_stream (int *dfdp, struct ftw_data *data, struct dir_data *dirp)
{
  int result = 0;

  if (data->dirstreams[data->actdir] != NULL)
    {
      /* Oh, oh.  We must close this stream.  Get all remaining
	 entries and store them as a list in the `content' member of
	 the `struct dir_data' variable.  */
      size_t bufsize = 1024;
      char *buf = malloc (bufsize);

      if (buf == NULL)
	result = -1;
      else
	{
	  DIR *st = data->dirstreams[data->actdir]->stream;
	  struct dirent64 *d;
	  size_t actsize = 0;

	  while ((d = __readdir64 (st)) != NULL)
	    {
	      size_t this_len = NAMLEN (d);
	      if (actsize + this_len + 2 >= bufsize)
		{
		  char *newp;
		  bufsize += MAX (1024, 2 * this_len);
		  newp = (char *) realloc (buf, bufsize);
		  if (newp == NULL)
		    {
		      /* No more memory.  */
		      free (buf);
		      return -1;
		    }
		  buf = newp;
		}

	      *((char *) __mempcpy (buf + actsize, d->d_name, this_len))
		= '\0';
	      actsize += this_len + 1;
	    }

	  /* Terminate the list with an additional NUL byte.  */
	  buf[actsize++] = '\0';

	  /* Shrink the buffer to what we actually need.  */
	  void *content = realloc (buf, actsize);
	  data->dirstreams[data->actdir]->content = content;
	  if (content == NULL)
	    {
	      free (buf);
	      result = -1;
	    }
	  else
	    {
	      __closedir (st);
	      data->dirstreams[data->actdir]->stream = NULL;
	      data->dirstreams[data->actdir]->streamfd = -1;
	      data->dirstreams[data->actdir] = NULL;
	    }
	}
    }

  /* Open the new stream.  */
  if (result == 0)
    {
      assert (data->dirstreams[data->actdir] == NULL);

      if (dfdp != NULL && *dfdp != -1)
	{
	  int fd = __openat64_nocancel (*dfdp, data->dirbuf + data->ftw.base,
					O_RDONLY | O_DIRECTORY | O_NDELAY);
	  dirp->stream = NULL;
	  if (fd != -1 && (dirp->stream = __fdopendir (fd)) == NULL)
	    __close_nocancel_nostatus (fd);
	}
      else
	{
	  const char *name;

	  if (data->flags & FTW_CHDIR)
	    {
	      name = data->dirbuf + data->ftw.base;
	      if (name[0] == '\0')
		name = ".";
	    }
	  else
	    name = data->dirbuf;

	  dirp->stream = __opendir (name);
	}

      if (dirp->stream == NULL)
	result = -1;
      else
	{
	  dirp->streamfd = __dirfd (dirp->stream);
	  dirp->content = NULL;
	  data->dirstreams[data->actdir] = dirp;

	  if (++data->actdir == data->maxdir)
	    data->actdir = 0;
	}
    }

  return result;
}


static int
process_entry (struct ftw_data *data, struct dir_data *dir, const char *name,
	       size_t namlen, struct STRUCT_STAT *out_st, bool *descend)
{
  struct STRUCT_STAT st;
  int result = 0;
  int flag = 0;
  size_t new_buflen;

  *descend = false;

  if (name[0] == '.' && (name[1] == '\0'
			 || (name[1] == '.' && name[2] == '\0')))
    /* Don't process the "." and ".." entries.  */
    return 0;

  new_buflen = data->ftw.base + namlen + 2;
  if (data->dirbufsize < new_buflen
      && !ftw_allocate (data, 2 * new_buflen))
    return -1;

  *((char *) __mempcpy (data->dirbuf + data->ftw.base, name, namlen)) = '\0';

  int statres;
  if (dir->streamfd != -1)
    statres = FSTATAT (dir->streamfd, name, &st,
		       (data->flags & FTW_PHYS) ? AT_SYMLINK_NOFOLLOW : 0);
  else
    {
      if ((data->flags & FTW_CHDIR) == 0)
	name = data->dirbuf;

      statres = ((data->flags & FTW_PHYS)
		 ? LSTAT (name, &st)
		 : STAT (name, &st));
    }

  if (statres < 0)
    {
      if (errno != EACCES && errno != ENOENT)
	result = -1;
      else if (data->flags & FTW_PHYS)
	flag = FTW_NS;
      else
	{
	  /* Old code left ST undefined for dangling DT_LNK without
	     FTW_PHYS set; a clarification at the POSIX level suggests
	     it should contain information about the link (ala lstat).
	     We do our best to fill in what data we can.  */
	  if (dir->streamfd != -1)
	    statres = FSTATAT (dir->streamfd, name, &st,
			       AT_SYMLINK_NOFOLLOW);
	  else
	    statres = LSTAT (name, &st);
	  if (statres == 0 && S_ISLNK (st.st_mode))
	    flag = FTW_SLN;
	  else
	    flag = FTW_NS;
	}
    }
  else
    {
      if (S_ISDIR (st.st_mode))
	flag = FTW_D;
      else if (S_ISLNK (st.st_mode))
	flag = FTW_SL;
      else
	flag = FTW_F;
    }

  if (result == 0
      && (flag == FTW_NS
	  || !(data->flags & FTW_MOUNT) || st.st_dev == data->dev))
    {
      if (flag == FTW_D)
	{
	  if ((data->flags & FTW_PHYS)
	      || (!find_object (data, &st)
		  /* Remember the object.  */
		  && (result = add_object (data, &st)) == 0))
	    {
               *out_st = st;
               *descend = true;
	    }
	}
      else
	result = CALL_FUNC (data, data->dirbuf, &st, data->cvt_arr[flag],
			    &data->ftw);
    }

  if ((data->flags & FTW_ACTIONRETVAL) && result == FTW_SKIP_SUBTREE)
    result = 0;

  return result;
}


/* The ftw_frame are kept as chunked array to minimize the reallocation cost
   when the stack grows (since it contains STRUCT_STAT and extra metadata).
   New chunks of ftw_framw are allocated and only freed when ftw returns.  */
enum
{
  FTW_STACK_CHUNK_BLOCKS  = 1,  /* Number of initial allocated chunks.  */
  FTW_STACK_CHUNK_SIZE    = 32  /* Number of stack frames allocated per
				   chunk.  */
};

static inline struct ftw_frame *
frame_stack_get (struct ftw_stack *ftwst, int adj)
{
  return &ftwst->stack[(ftwst->top + adj) / FTW_STACK_CHUNK_SIZE]
    [(ftwst->top + adj) % FTW_STACK_CHUNK_SIZE];
}

static inline void
frame_stack_reset_top (struct ftw_stack *fwtst, const struct STRUCT_STAT *st)
{
  struct ftw_frame *frame = frame_stack_get (fwtst, 0);
  frame->st = *st;
  frame->state = FTW_STATE_INIT;
  frame->dir.stream = NULL;
  frame->dir.content = NULL;
  frame->dir.streamfd = -1;
}

static bool
frame_stack_init (struct ftw_stack *ftwst, const struct STRUCT_STAT *st)
{
  ftwst->num_blocks = FTW_STACK_CHUNK_BLOCKS;
  ftwst->stack = malloc (FTW_STACK_CHUNK_BLOCKS * sizeof (*ftwst->stack));
  if (ftwst->stack == NULL)
    return false;

  ftwst->stack[0] = malloc (FTW_STACK_CHUNK_SIZE * sizeof (struct ftw_frame));
  if (ftwst->stack[0] == NULL)
    {
      free (ftwst->stack);
      return false;
    }

  ftwst->top = 0;
  frame_stack_reset_top (ftwst, st);
  return true;
}

static void
frame_stack_free (struct ftw_stack *ftwst)
{
  for (size_t i = 0; i < ftwst->num_blocks; i++)
    free (ftwst->stack[i]);
  free (ftwst->stack);
}

static bool
frame_stack_add (struct ftw_stack *ftwst, const struct STRUCT_STAT *st)
{
  if (ftwst->top + 1 >= ftwst->num_blocks * FTW_STACK_CHUNK_SIZE)
    {
      size_t new_blocks = ftwst->num_blocks + 1;
      struct ftw_frame **new_stack = realloc (
	  ftwst->stack, new_blocks * sizeof (*ftwst->stack));

      if (new_stack == NULL)
	return false;
      ftwst->stack = new_stack;
      ftwst->stack[ftwst->num_blocks] = malloc (
	  FTW_STACK_CHUNK_SIZE * sizeof (struct ftw_frame));
      if (ftwst->stack[ftwst->num_blocks] == NULL)
	return false;
      ftwst->num_blocks = new_blocks;
    }
  ftwst->top++;
  frame_stack_reset_top (ftwst, st);
  return true;
}

static void
frame_closedir (struct ftw_data *data, struct ftw_frame *frame)
{
  int save_err = errno;
  assert (frame->dir.content == NULL);
  __closedir (frame->dir.stream);
  frame->dir.streamfd = -1;
  __set_errno (save_err);
  if (data->actdir-- == 0)
    data->actdir = data->maxdir - 1;
  data->dirstreams[data->actdir] = NULL;
  frame->dir.stream = NULL;
}

static int
ftw_dir (struct ftw_data *data, const struct STRUCT_STAT *st)
{
  struct ftw_stack ftwst;
  if (!frame_stack_init (&ftwst, st))
    return -1;

  int result = 0;

  while (ftwst.top >= 0)
    {
      struct ftw_frame *frame = frame_stack_get (&ftwst, 0);
      struct dir_data *old_dir = (ftwst.top > 0)
	? &frame_stack_get (&ftwst, -1)->dir : NULL;

      if (frame->state == FTW_STATE_INIT)
	{
	  frame->previous_base = data->ftw.base;
	  result = open_dir_stream (
	      old_dir == NULL ? NULL : &old_dir->streamfd, data, &frame->dir);
	  if (result != 0)
	    {
	      if (errno == EACCES)
		result = CALL_FUNC (data, data->dirbuf, &frame->st, FTW_DNR,
				    &data->ftw);
	      ftwst.top--;
	      /* Intercept FTW_SKIP_SUBTREE when popping frame */
	      if (ftwst.top >= 0 && (data->flags & FTW_ACTIONRETVAL)
		  && result == FTW_SKIP_SUBTREE)
		result = 0;
	      continue;
	    }

	  if (!(data->flags & FTW_DEPTH))
	    {
	      result = CALL_FUNC (data, data->dirbuf, &frame->st, FTW_D,
				  &data->ftw);
	      if (result != 0)
		goto state0_fail;
	    }

	  if (data->flags & FTW_CHDIR)
	    {
	      if (__fchdir (__dirfd (frame->dir.stream)) < 0)
		{
		  result = -1;
		state0_fail:
		  frame_closedir (data, frame);
		  ftwst.top--;
		  /* Intercept FTW_SKIP_SUBTREE when popping frame.  */
		  if (ftwst.top >= 0 && (data->flags & FTW_ACTIONRETVAL)
		      && result == FTW_SKIP_SUBTREE)
		    result = 0;
		  continue;
		}
	    }

	  ++data->ftw.level;
	  char *startp = strchr (data->dirbuf, '\0');
	  assert (startp != data->dirbuf);
	  if (startp[-1] != '/')
	    *startp++ = '/';
	  data->ftw.base = startp - data->dirbuf;

	  frame->state = FTW_STATE_STREAM_LOOP;
	  frame->runp = frame->dir.content;
	}
      else if (frame->state == FTW_STATE_STREAM_LOOP)
	{
	  if (result != 0)
	    {
	      frame->state = FTW_STATE_CLEANUP;
	      continue;
	    }

	  if (frame->dir.stream == NULL)
	    {
	      frame->state = FTW_STATE_CONTENT_LOOP;
	      frame->runp = frame->dir.content;
	      continue;
	    }

	  struct dirent64 *d = __readdir64 (frame->dir.stream);
	  if (d != NULL)
	    {
	      struct STRUCT_STAT child_st;
	      bool descend = false;
	      result = process_entry (data, &frame->dir, d->d_name, NAMLEN (d),
				      &child_st, &descend);

	      if (result == 0 && descend)
		{
		  if (!frame_stack_add (&ftwst, &child_st))
		    {
		      result = -1;
		      frame->state = FTW_STATE_CLEANUP;
		    }
		  continue;
		}
	      else if (result != 0)
		{
		  frame->state = FTW_STATE_CLEANUP;
		  continue;
		}
	    }
	  else
	    frame->state = FTW_STATE_CLEANUP;
	}
      else if (frame->state == FTW_STATE_CONTENT_LOOP)
	{
	  /* Check if we are safely positioned to process the starting path.
	     The 'result' variable here comes from one of two places:

	     1. Initialization: defaults to 0 at the top of ftw_startup.  If
	        the FTW_CHDIR flag was NOT passed, it remains 0, meaning we
		are good to go.

	     2. Directory Change: If FTW_CHDIR WAS passed, 'result' holds the
	        return value of the preceding __chdir() call (either moving
		to "/" or the parsed base directory).

	     If 'result' is 0, the setup succeeded (or wasn't needed) and we
	     can safely stat the initial object.  Othewise, the chdir failed,
	     so we skip processing and fall through to the cleanup phase.  */
	  if (result != 0)
	    {
	      frame->state = FTW_STATE_CLEANUP;
	      continue;
	    }

	  if (frame->runp != NULL && *frame->runp != '\0')
	    {
	      char *endp = strchr (frame->runp, '\0');
	      struct STRUCT_STAT child_st;
	      bool descend = false;

	      result = process_entry (data, &frame->dir, frame->runp,
				      endp - frame->runp, &child_st,
				      &descend);
	      frame->runp = endp + 1;

	      if (result == 0 && descend)
		{
		  if (!frame_stack_add (&ftwst, &child_st))
		    {
		      result = -1;
		      frame->state = FTW_STATE_CLEANUP;
		    }
		  continue;
		}
	      else if (result != 0)
		{
		  frame->state = FTW_STATE_CLEANUP;
		  continue;
		}
	    }
	  else
	    frame->state = FTW_STATE_CLEANUP;
	}
      else if (frame->state == FTW_STATE_CLEANUP)
	{
	  if (frame->dir.stream != NULL)
	    frame_closedir (data, frame);
	  else if (frame->dir.content != NULL)
	    {
	      free (frame->dir.content);
	      frame->dir.content = NULL;
	    }

	  if ((data->flags & FTW_ACTIONRETVAL) && result == FTW_SKIP_SIBLINGS)
	    result = 0;

	  data->dirbuf[data->ftw.base - 1] = '\0';
	  --data->ftw.level;
	  data->ftw.base = frame->previous_base;

	  if (result == 0 && (data->flags & FTW_DEPTH))
	    result
		= CALL_FUNC (data, data->dirbuf, &frame->st, FTW_DP,
			     &data->ftw);

	  if (old_dir != NULL && (data->flags & FTW_CHDIR)
	      && (result == 0
		  || ((data->flags & FTW_ACTIONRETVAL)
		      && (result != -1 && result != FTW_STOP))))
	    {
	      int done = 0;
	      if (old_dir->stream != NULL)
		if (__fchdir (__dirfd (old_dir->stream)) == 0)
		  done = 1;

	      if (!done)
		{
		  if (data->ftw.base == 1)
		    {
		      if (__chdir ("/") < 0)
			result = -1;
		    }
		  else if (__chdir ("..") < 0)
		    result = -1;
		}
	    }

	  ftwst.top--;
	  /* Intercept FTW_SKIP_SUBTREE when popping frame.  */
	  if (ftwst.top >= 0 && (data->flags & FTW_ACTIONRETVAL)
	      && result == FTW_SKIP_SUBTREE)
	    result = 0;
	}
    }

  frame_stack_free (&ftwst);

  return result;
}


static int
ftw_startup (const char *dir, bool is_nftw, func_callback_t func,
	     int descriptors, int flags)
{
  struct ftw_data data = { .dirstreams = NULL };
  struct STRUCT_STAT st;
  int result = 0;
  int cwdfd = -1;
  char *cwd = NULL;
  char *cp;

  /* First make sure the parameters are reasonable.  */
  if (dir[0] == '\0')
    {
      __set_errno (ENOENT);
      return -1;
    }

  data.maxdir = descriptors < 1 ? 1 : descriptors;
  data.actdir = 0;
  /* PATH_MAX is always defined when we get here.  */
  if (!ftw_allocate (&data, MAX (2 * strlen (dir), PATH_MAX)))
    return -1;
  memset (data.dirstreams, '\0', data.maxdir * sizeof (struct dir_data *));
  cp = __stpcpy (data.dirbuf, dir);
  /* Strip trailing slashes.  */
  while (cp > data.dirbuf + 1 && cp[-1] == '/')
    --cp;
  *cp = '\0';

  data.ftw.level = 0;

  /* Find basename.  */
  while (cp > data.dirbuf && cp[-1] != '/')
    --cp;
  data.ftw.base = cp - data.dirbuf;

  data.flags = flags;

  data.is_nftw = is_nftw;
  data.func = func;

  /* Since we internally use the complete set of FTW_* values we need
     to reduce the value range before calling a `ftw' callback.  */
  data.cvt_arr = is_nftw ? nftw_arr : ftw_arr;

  /* No object known so far.  */
  data.known_objects = NULL;

  /* Now go to the directory containing the initial file/directory.  */
  if (flags & FTW_CHDIR)
    {
      /* We have to be able to go back to the current working
	 directory.  The best way to do this is to use a file
	 descriptor.  */
      cwdfd = __open (".", O_RDONLY | O_DIRECTORY);
      if (cwdfd == -1)
	{
	  /* Try getting the directory name.  This can be needed if
	     the current directory is executable but not readable.  */
	  if (errno == EACCES)
	    /* GNU extension ahead.  */
	    cwd =  __getcwd (NULL, 0);

	  if (cwd == NULL)
	    goto out_fail;
	}
      else if (data.maxdir > 1)
	/* Account for the file descriptor we use here.  */
	--data.maxdir;

      if (data.ftw.base > 0)
	{
	  /* Change to the directory the file is in.  In data.dirbuf
	     we have a writable copy of the file name.  Just NUL
	     terminate it for now and change the directory.  */
	  if (data.ftw.base == 1)
	    /* I.e., the file is in the root directory.  */
	    result = __chdir ("/");
	  else
	    {
	      char ch = data.dirbuf[data.ftw.base - 1];
	      data.dirbuf[data.ftw.base - 1] = '\0';
	      result = __chdir (data.dirbuf);
	      data.dirbuf[data.ftw.base - 1] = ch;
	    }
	}
    }

  /* Get stat info for start directory.  */
  if (result == 0)
    {
      const char *name;

      if (data.flags & FTW_CHDIR)
	{
	  name = data.dirbuf + data.ftw.base;
	  if (name[0] == '\0')
	    name = ".";
	}
      else
	name = data.dirbuf;

      if (((flags & FTW_PHYS)
	   ? LSTAT (name, &st)
	   : STAT (name, &st)) < 0)
	{
	  if (!(flags & FTW_PHYS)
	      && errno == ENOENT
	      && LSTAT (name, &st) == 0
	      && S_ISLNK (st.st_mode))
	    result = CALL_FUNC (&data, data.dirbuf, &st, data.cvt_arr[FTW_SLN],
				&data.ftw);
	  else
	    /* No need to call the callback since we cannot say anything
	       about the object.  */
	    result = -1;
	}
      else
	{
	  if (S_ISDIR (st.st_mode))
	    {
	      /* Remember the device of the initial directory in case
		 FTW_MOUNT is given.  */
	      data.dev = st.st_dev;

	      /* We know this directory now.  */
	      if (!(flags & FTW_PHYS))
		result = add_object (&data, &st);

	      if (result == 0)
		result = ftw_dir (&data, &st);
	    }
	  else
	    {
	      int flag = S_ISLNK (st.st_mode) ? FTW_SL : FTW_F;

	      result = CALL_FUNC (&data, data.dirbuf, &st, data.cvt_arr[flag],
				  &data.ftw);
	    }
	}

      if ((flags & FTW_ACTIONRETVAL)
	  && (result == FTW_SKIP_SUBTREE || result == FTW_SKIP_SIBLINGS))
	result = 0;
    }

  /* Return to the start directory (if necessary).  */
  if (cwdfd != -1)
    {
      int save_err = errno;
      __fchdir (cwdfd);
      __close_nocancel_nostatus (cwdfd);
      __set_errno (save_err);
    }
  else if (cwd != NULL)
    {
      int save_err = errno;
      __chdir (cwd);
      free (cwd);
      __set_errno (save_err);
    }

  /* Free all memory.  */
 out_fail:
  __tdestroy (data.known_objects, free);
  free (data.dirstreams);

  return result;
}



/* Entry points.  */

int
FTW_NAME (const char *path, FTW_FUNC_T func, int descriptors)
{
  return ftw_startup (path, false, (func_callback_t) { .ftw_func = func },
		      descriptors, 0);
}

#ifndef NFTW_OLD_NAME
int
NFTW_NAME (const char *path, NFTW_FUNC_T func, int descriptors, int flags)
{
  return ftw_startup (path, true, (func_callback_t) { .nftw_func = func },
		      descriptors, flags);
}
#else

# include <shlib-compat.h>

int NFTW_NEW_NAME (const char *, NFTW_FUNC_T, int, int);

int
NFTW_NEW_NAME (const char *path, NFTW_FUNC_T func, int descriptors, int flags)
{
  if (flags
      & ~(FTW_PHYS | FTW_MOUNT | FTW_CHDIR | FTW_DEPTH | FTW_ACTIONRETVAL))
    {
      __set_errno (EINVAL);
      return -1;
    }
  return ftw_startup (path, true, (func_callback_t) { .nftw_func = func },
		      descriptors, flags);
}

# if SHLIB_COMPAT(libc, GLIBC_2_1, GLIBC_2_3_3)

/* Older nftw* version just ignored all unknown flags.  */

int NFTW_OLD_NAME (const char *, NFTW_FUNC_T, int, int);

int
attribute_compat_text_section
NFTW_OLD_NAME (const char *path, NFTW_FUNC_T func, int descriptors, int flags)
{
  flags &= (FTW_PHYS | FTW_MOUNT | FTW_CHDIR | FTW_DEPTH);
  return ftw_startup (path, true, (func_callback_t) { .nftw_func = func },
		      descriptors, flags);
}

# endif
#endif /* NFTW_OLD_NAME  */
