/* Facilities for FUSE-backed file system tests.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <support/fuse.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/sysmacros.h>
#include <sys/uio.h>
#include <unistd.h>

#include <array_length.h>
#include <support/check.h>
#include <support/namespace.h>
#include <support/support.h>
#include <support/test-driver.h>
#include <support/xdirent.h>
#include <support/xthread.h>
#include <support/xunistd.h>

#ifdef __linux__
# include <sys/mount.h>
#else
/* Fallback definitions that mark the test as unsupported.  */
# define mount(...) ({ FAIL_UNSUPPORTED ("mount"); -1; })
# define umount(...) ({ FAIL_UNSUPPORTED ("mount"); -1; })
#endif

struct support_fuse
{
  char *mountpoint;
  void *buffer_start;           /* Begin of allocation.  */
  void *buffer_next;            /* Next read position.  */
  void *buffer_limit;           /* End of buffered data.  */
  void *buffer_end;             /* End of allocation.  */
  struct fuse_in_header *inh;   /* Most recent request (support_fuse_next).  */
  union                         /* Space for prepared responses.  */
  {
    struct fuse_attr_out attr;
    struct fuse_entry_out entry;
    struct
    {
      struct fuse_entry_out entry;
      struct fuse_open_out open;
    } create;
  } prepared;
  void *prepared_pointer;       /* NULL if inactive.  */
  size_t prepared_size;         /* 0 if inactive.  */

  /* Used for preparing readdir responses.  Already used-up area for
     the current request is counted by prepared_size.  */
  void *readdir_buffer;
  size_t readdir_buffer_size;

  pthread_t handler;            /* Thread handling requests.  */
  uid_t uid;                    /* Cached value for the current process.  */
  uid_t gid;                    /* Cached value for the current process.  */
  int fd;                       /* FUSE file descriptor.  */
  int connection;               /* Entry under /sys/fs/fuse/connections.  */
  bool filter_forget;           /* Controls FUSE_FORGET event dropping.  */
  _Atomic bool disconnected;
};

struct fuse_thread_wrapper_args
{
  struct support_fuse *f;
  support_fuse_callback callback;
  void *closure;
};

/* Set by support_fuse_init to indicate that support_fuse_mount may be
   called.  */
static bool support_fuse_init_called;

/* Allocate the read buffer in F with SIZE bytes capacity.  Does not
   free the previously allocated buffer.  */
static void support_fuse_allocate (struct support_fuse *f, size_t size)
  __nonnull ((1));

/* Internal mkdtemp replacement */
static char * support_fuse_mkdir (const char *prefix) __nonnull ((1));

/* Low-level allocation function for support_fuse_mount.  Does not
   perform the mount.  */
static struct support_fuse *support_fuse_open (void);

/* Thread wrapper function for use with pthread_create.  Uses struct
   fuse_thread_wrapper_args.  */
static void *support_fuse_thread_wrapper (void *closure)  __nonnull ((1));

/* Initial step before preparing a reply.  SIZE must be the size of
   the F->prepared member that is going to be used.  */
static void support_fuse_prepare_1 (struct support_fuse *f, size_t size);

/* Similar to support_fuse_reply_error, but not check that ERROR is
   not zero.  */
static void support_fuse_reply_error_1 (struct support_fuse *f,
                                        uint32_t error) __nonnull ((1));

/* Path to the directory containing mount points.  Initialized by an
   ELF constructor.  All mountpoints are collected there so that the
   test wrapper can clean them up without keeping track of them
   individually.  */
static char *support_fuse_mountpoints;

/* PID of the process that should clean up the mount points in the ELF
   destructor.  */
static pid_t support_fuse_cleanup_pid;

static void
support_fuse_allocate (struct support_fuse *f, size_t size)
{
  f->buffer_start = xmalloc (size);
  f->buffer_end = f->buffer_start + size;
  f->buffer_limit = f->buffer_start;
  f->buffer_next = f->buffer_limit;
}

void
support_fuse_filter_forget (struct support_fuse *f, bool filter)
{
  f->filter_forget = filter;
}

void *
support_fuse_cast_internal (struct fuse_in_header *p, uint32_t expected)
{
  if (expected != p->opcode
      && !(expected == FUSE_READ && p->opcode == FUSE_READDIR))
    {
      char *expected1 = support_fuse_opcode (expected);
      char *actual = support_fuse_opcode (p->opcode);
      FAIL_EXIT1 ("attempt to cast %s to %s", actual, expected1);
    }
  return p + 1;
}

void *
support_fuse_cast_name_internal (struct fuse_in_header *p, uint32_t expected,
                                 size_t skip, char **name)
{
  char *result = support_fuse_cast_internal (p, expected);
  *name = result + skip;
  return result;
}

bool
support_fuse_dirstream_add (struct support_fuse_dirstream *d,
                            uint64_t d_ino, uint64_t d_off,
                            uint32_t d_type, const char *d_name)
{
  struct support_fuse *f = (struct support_fuse *) d;
  size_t structlen = offsetof (struct fuse_dirent, name);
  size_t namelen = strlen (d_name); /* No null termination.  */
  size_t required_size = FUSE_DIRENT_ALIGN (structlen + namelen);
  if (f->readdir_buffer_size - f->prepared_size < required_size)
    return false;
  struct fuse_dirent entry =
    {
      .ino = d_ino,
      .off = d_off,
      .type = d_type,
      .namelen = namelen,
    };
  memcpy (f->readdir_buffer + f->prepared_size, &entry, structlen);
  /* Use strncpy to write padding and avoid passing uninitialized
     bytes to the read system call.  */
  strncpy (f->readdir_buffer + f->prepared_size + structlen, d_name,
           required_size - structlen);
  f->prepared_size += required_size;
  return true;
}

bool
support_fuse_handle_directory (struct support_fuse *f)
{
  TEST_VERIFY (f->inh != NULL);
  switch (f->inh->opcode)
    {
    case FUSE_OPENDIR:
      {
        struct fuse_open_out out =
          {
          };
        support_fuse_reply (f, &out, sizeof (out));
      }
      return true;
    case FUSE_RELEASEDIR:
      support_fuse_reply_empty (f);
      return true;
    case FUSE_GETATTR:
      {
        struct fuse_attr_out *out = support_fuse_prepare_attr (f);
        out->attr.mode = S_IFDIR | 0700;
        support_fuse_reply_prepared (f);
      }
      return true;
    case FUSE_GETXATTR:
      support_fuse_reply_error (f, ENOSYS);
      return true;
    default:
      return false;
    }
}

bool
support_fuse_handle_mountpoint (struct support_fuse *f)
{
  TEST_VERIFY (f->inh != NULL);
  /* 1 is the root node.  */
  if ((f->inh->opcode == FUSE_GETATTR || f->inh->opcode == FUSE_GETXATTR)
      && f->inh->nodeid == 1)
    return support_fuse_handle_directory (f);
  return false;
}

void
support_fuse_init (void)
{
  support_fuse_init_called = true;

  support_become_root ();
  if (!support_enter_mount_namespace ())
    FAIL_UNSUPPORTED ("mount namespaces not supported");
}

void
support_fuse_init_no_namespace (void)
{
  support_fuse_init_called = true;
}

static char *
support_fuse_mkdir (const char *prefix)
{
  /* Do not use mkdtemp to avoid interfering with its tests.  */
  unsigned int counter = 1;
  unsigned int pid = getpid ();
  while (true)
    {
      char *path = xasprintf ("%s%u.%u/", prefix, pid, counter);
      if (mkdir (path, 0700) == 0)
        return path;
      if (errno != EEXIST)
        FAIL_EXIT1 ("mkdir (\"%s\"): %m", path);
      free (path);
      ++counter;
    }
}

struct support_fuse *
support_fuse_mount (support_fuse_callback callback, void *closure)
{
  TEST_VERIFY_EXIT (support_fuse_init_called);

  /* Request at least minor version 12 because it changed struct sizes.  */
  enum { min_version = 12 };

  struct support_fuse *f = support_fuse_open ();
  char *mount_options
    = xasprintf ("fd=%d,rootmode=040700,user_id=%u,group_id=%u",
                 f->fd, f->uid, f->gid);
  if (mount ("fuse", f->mountpoint, "fuse.glibc",
             MS_NOSUID|MS_NODEV, mount_options)
      != 0)
    FAIL_EXIT1 ("FUSE mount on %s: %m", f->mountpoint);
  free (mount_options);

  /* Retry with an older FUSE version.  */
  while (true)
    {
      struct fuse_in_header *inh = support_fuse_next (f);
      struct fuse_init_in *init_in = support_fuse_cast (INIT, inh);
      if (init_in->major < 7
          || (init_in->major == 7 && init_in->minor < min_version))
        FAIL_UNSUPPORTED ("kernel FUSE version is %u.%u, too old",
                          init_in->major, init_in->minor);
      if (init_in->major > 7)
        {
          uint32_t major = 7;
          support_fuse_reply (f, &major, sizeof (major));
          continue;
        }
      TEST_VERIFY (init_in->flags & FUSE_DONT_MASK);
      struct fuse_init_out out =
        {
          .major = 7,
          .minor = min_version,
          /* Request that the kernel does not apply umask.  */
          .flags = FUSE_DONT_MASK,
        };
      support_fuse_reply (f, &out, sizeof (out));

      {
        struct fuse_thread_wrapper_args args =
          {
            .f = f,
            .callback = callback,
            .closure = closure,
          };
        f->handler = xpthread_create (NULL,
                                      support_fuse_thread_wrapper, &args);
        struct stat64 st;
        xstat64 (f->mountpoint, &st);
        f->connection = minor (st.st_dev);
        /* Got a reply from the thread,  safe to deallocate args.  */
      }

      return f;
    }
}

const char *
support_fuse_mountpoint (struct support_fuse *f)
{
  return f->mountpoint;
}

void
support_fuse_no_reply (struct support_fuse *f)
{
  TEST_VERIFY (f->inh != NULL);
  TEST_COMPARE (f->inh->opcode, FUSE_FORGET);
  f->inh = NULL;
}

char *
support_fuse_opcode (uint32_t op)
{
  const char *result;
  switch (op)
    {
#define X(n) case n: result = #n; break
      X(FUSE_LOOKUP);
      X(FUSE_FORGET);
      X(FUSE_GETATTR);
      X(FUSE_SETATTR);
      X(FUSE_READLINK);
      X(FUSE_SYMLINK);
      X(FUSE_MKNOD);
      X(FUSE_MKDIR);
      X(FUSE_UNLINK);
      X(FUSE_RMDIR);
      X(FUSE_RENAME);
      X(FUSE_LINK);
      X(FUSE_OPEN);
      X(FUSE_READ);
      X(FUSE_WRITE);
      X(FUSE_STATFS);
      X(FUSE_RELEASE);
      X(FUSE_FSYNC);
      X(FUSE_SETXATTR);
      X(FUSE_GETXATTR);
      X(FUSE_LISTXATTR);
      X(FUSE_REMOVEXATTR);
      X(FUSE_FLUSH);
      X(FUSE_INIT);
      X(FUSE_OPENDIR);
      X(FUSE_READDIR);
      X(FUSE_RELEASEDIR);
      X(FUSE_FSYNCDIR);
      X(FUSE_GETLK);
      X(FUSE_SETLK);
      X(FUSE_SETLKW);
      X(FUSE_ACCESS);
      X(FUSE_CREATE);
      X(FUSE_INTERRUPT);
      X(FUSE_BMAP);
      X(FUSE_DESTROY);
      X(FUSE_IOCTL);
      X(FUSE_POLL);
      X(FUSE_NOTIFY_REPLY);
      X(FUSE_BATCH_FORGET);
      X(FUSE_FALLOCATE);
      X(FUSE_READDIRPLUS);
      X(FUSE_RENAME2);
      X(FUSE_LSEEK);
      X(FUSE_COPY_FILE_RANGE);
      X(FUSE_SETUPMAPPING);
      X(FUSE_REMOVEMAPPING);
      X(FUSE_SYNCFS);
      X(FUSE_TMPFILE);
      X(FUSE_STATX);
#undef X
    default:
      return xasprintf ("FUSE_unknown_%u", op);
    }
  return xstrdup (result);
}

static struct support_fuse *
support_fuse_open (void)
{
  struct support_fuse *result = xmalloc (sizeof (*result));
  result->mountpoint = support_fuse_mkdir (support_fuse_mountpoints);
  result->inh = NULL;
  result->prepared_pointer = NULL;
  result->prepared_size = 0;
  result->readdir_buffer = NULL;
  result->readdir_buffer_size = 0;
  result->uid = getuid ();
  result->gid = getgid ();
  result->fd = open ("/dev/fuse", O_RDWR, 0);
  if (result->fd < 0)
    {
      if (errno == ENOENT || errno == ENODEV || errno == EPERM
          || errno == EACCES)
        FAIL_UNSUPPORTED ("cannot open /dev/fuse: %m");
      else
        FAIL_EXIT1 ("cannot open /dev/fuse: %m");
    }
  result->connection = -1;
  result->filter_forget = true;
  result->disconnected = false;
  support_fuse_allocate (result, FUSE_MIN_READ_BUFFER);
  return result;
}

static void
support_fuse_prepare_1 (struct support_fuse *f, size_t size)
{
  TEST_VERIFY (f->prepared_pointer == NULL);
  f->prepared_size = size;
  memset (&f->prepared, 0, size);
  f->prepared_pointer = &f->prepared;
}

struct fuse_attr_out *
support_fuse_prepare_attr (struct support_fuse *f)
{
  support_fuse_prepare_1 (f, sizeof (f->prepared.attr));
  f->prepared.attr.attr.uid = f->uid;
  f->prepared.attr.attr.gid = f->gid;
  f->prepared.attr.attr.ino = f->inh->nodeid;
  return &f->prepared.attr;
}

void
support_fuse_prepare_create (struct support_fuse *f,
                             uint64_t nodeid,
                             struct fuse_entry_out **out_entry,
                             struct fuse_open_out **out_open)
{
  support_fuse_prepare_1 (f, sizeof (f->prepared.create));
  f->prepared.create.entry.nodeid = nodeid;
  f->prepared.create.entry.attr.uid = f->uid;
  f->prepared.create.entry.attr.gid = f->gid;
  f->prepared.create.entry.attr.ino = nodeid;
  *out_entry = &f->prepared.create.entry;
  *out_open = &f->prepared.create.open;
}

struct fuse_entry_out *
support_fuse_prepare_entry (struct support_fuse *f, uint64_t nodeid)
{
  support_fuse_prepare_1 (f, sizeof (f->prepared.entry));
  f->prepared.entry.nodeid = nodeid;
  f->prepared.entry.attr.uid = f->uid;
  f->prepared.entry.attr.gid = f->gid;
  f->prepared.entry.attr.ino = nodeid;
  return &f->prepared.entry;
}

struct support_fuse_dirstream *
support_fuse_prepare_readdir (struct support_fuse *f)
{
  support_fuse_prepare_1 (f, 0);
  struct fuse_read_in *p = support_fuse_cast (READ, f->inh);
  if (p->size > f->readdir_buffer_size)
    {
      free (f->readdir_buffer);
      f->readdir_buffer = xmalloc (p->size);
      f->readdir_buffer_size = p->size;
    }
  f->prepared_pointer = f->readdir_buffer;
  return (struct support_fuse_dirstream *) f;
}

struct fuse_in_header *
support_fuse_next (struct support_fuse *f)
{
  TEST_VERIFY (f->inh == NULL);
  while (true)
    {
      if (f->buffer_next < f->buffer_limit)
        {
          f->inh = f->buffer_next;
          f->buffer_next = (void *) f->buffer_next + f->inh->len;
          /* Suppress FUSE_FORGET responses if requested.  */
          if (f->filter_forget && f->inh->opcode == FUSE_FORGET)
            {
              f->inh = NULL;
              continue;
            }
          return f->inh;
        }
      ssize_t ret = read (f->fd, f->buffer_start,
                          f->buffer_end - f->buffer_start);
      if (ret == 0)
        FAIL_EXIT (1, "unexpected EOF on FUSE device");
      if (ret < 0 && errno == EINVAL)
        {
          /* Increase buffer size.  */
          size_t new_size = 2 * (size_t) (f->buffer_end - f->buffer_start);
          free (f->buffer_start);
          support_fuse_allocate (f, new_size);
          continue;
        }
      if (ret < 0)
        {
          if (f->disconnected)
            /* Unmount detected.  */
            return NULL;
          FAIL_EXIT1 ("read error on FUSE device: %m");
        }
      /* Read was successful, make [next, limit) the active buffer area.  */
      f->buffer_next = f->buffer_start;
      f->buffer_limit = (void *) f->buffer_start + ret;
    }
}

void
support_fuse_reply (struct support_fuse *f,
                    const void *payload, size_t payload_size)
{
  TEST_VERIFY_EXIT (f->inh != NULL);
  TEST_VERIFY (f->prepared_pointer == NULL);
  struct fuse_out_header outh =
    {
      .len = sizeof (outh) + payload_size,
      .unique = f->inh->unique,
    };
  struct iovec iov[] =
    {
      { &outh, sizeof (outh) },
      { (void *) payload, payload_size },
    };
  ssize_t ret = writev (f->fd, iov, array_length (iov));
  if (ret < 0)
    {
      if (!f->disconnected)
        /* Some kernels produce write errors upon disconnect.  */
        FAIL_EXIT1 ("FUSE write failed for %s response"
                    " (%zu bytes payload): %m",
                    support_fuse_opcode (f->inh->opcode), payload_size);
    }
  else if (ret != sizeof (outh) + payload_size)
    FAIL_EXIT1 ("FUSE write short for %s response (%zu bytes payload):"
                " %zd bytes",
                support_fuse_opcode (f->inh->opcode), payload_size, ret);
  f->inh = NULL;
}

void
support_fuse_reply_empty (struct support_fuse *f)
{
  support_fuse_reply_error_1 (f, 0);
}

static void
support_fuse_reply_error_1 (struct support_fuse *f, uint32_t error)
{
  TEST_VERIFY_EXIT (f->inh != NULL);
  struct fuse_out_header outh =
    {
      .len = sizeof (outh),
      .error = -error,
      .unique = f->inh->unique,
    };
  ssize_t ret = write (f->fd, &outh, sizeof (outh));
  if (ret < 0)
    {
      /* Some kernels produce write errors upon disconnect.  */
      if (!f->disconnected)
        FAIL_EXIT1 ("FUSE write failed for %s error response: %m",
                    support_fuse_opcode (f->inh->opcode));
    }
  else if (ret != sizeof (outh))
    FAIL_EXIT1 ("FUSE write short for %s error response: %zd bytes",
                support_fuse_opcode (f->inh->opcode), ret);
  f->inh = NULL;
  f->prepared_pointer = NULL;
  f->prepared_size = 0;
}

void
support_fuse_reply_error (struct support_fuse *f, uint32_t error)
{
  TEST_VERIFY (error > 0);
  support_fuse_reply_error_1 (f, error);
}

void
support_fuse_reply_prepared (struct support_fuse *f)
{
  TEST_VERIFY_EXIT (f->prepared_pointer != NULL);
  /* Re-use the non-prepared reply function.  It requires
     f->prepared_* to be non-null, so reset the fields before the call.  */
  void *prepared_pointer = f->prepared_pointer;
  size_t prepared_size = f->prepared_size;
  f->prepared_pointer = NULL;
  f->prepared_size = 0;
  support_fuse_reply (f, prepared_pointer, prepared_size);
}

static void *
support_fuse_thread_wrapper (void *closure)
{
  struct fuse_thread_wrapper_args args
    = *(struct fuse_thread_wrapper_args *) closure;

  /* Handle the initial stat call.  */
  struct fuse_in_header *inh = support_fuse_next (args.f);
  if (inh == NULL || !support_fuse_handle_mountpoint (args.f))
    {
      support_fuse_reply_error (args.f, EIO);
      return NULL;
    }

  args.callback  (args.f, args.closure);
  return NULL;
}

void
support_fuse_unmount (struct support_fuse *f)
{
  /* Signal the unmount to the handler thread.  Some kernels report
     not just ENODEV errors on read.  */
  f->disconnected = true;

  {
    char *path = xasprintf ("/sys/fs/fuse/connections/%d/abort",
                            f->connection);
    /* Some kernels do not support these files under /sys.  */
    int fd = open (path, O_RDWR | O_TRUNC);
    if (fd >= 0)
      {
        TEST_COMPARE (write (fd, "1", 1), 1);
        xclose (fd);
      }
    free (path);
  }
  if (umount (f->mountpoint) != 0)
    FAIL ("FUSE: umount (\"%s\"): %m", f->mountpoint);
  xpthread_join (f->handler);
  if (rmdir (f->mountpoint) != 0)
    FAIL ("FUSE: rmdir (\"%s\"): %m", f->mountpoint);
  xclose (f->fd);
  free (f->buffer_start);
  free (f->mountpoint);
  free (f->readdir_buffer);
  free (f);
}

static void __attribute__ ((constructor))
init (void)
{
  /* The test_dir test driver variable is not yet set at this point.  */
  const char *tmpdir = getenv ("TMPDIR");
  if (tmpdir == NULL || tmpdir[0] == '\0')
    tmpdir = "/tmp";

  char *prefix = xasprintf ("%s/glibc-tst-fuse.", tmpdir);
  support_fuse_mountpoints = support_fuse_mkdir (prefix);
  free (prefix);
  support_fuse_cleanup_pid = getpid ();
}

static void __attribute__ ((destructor))
fini (void)
{
  if (support_fuse_cleanup_pid != getpid ()
      || support_fuse_mountpoints == NULL)
    return;
  DIR *dir = xopendir (support_fuse_mountpoints);
  while (true)
    {
      struct dirent64 *e = readdir64 (dir);
      if (e == NULL)
        /* Ignore errors.  */
        break;
      if (*e->d_name == '.')
        /* Skip "." and "..".  No hidden files expected.  */
        continue;
      if (unlinkat (dirfd (dir), e->d_name, AT_REMOVEDIR) != 0)
        break;
      rewinddir (dir);
    }
  xclosedir (dir);
  rmdir (support_fuse_mountpoints);
  free (support_fuse_mountpoints);
  support_fuse_mountpoints = NULL;
}
