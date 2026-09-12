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
#include <stdio.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xdirent.h>
#include <support/xunistd.h>

static void
fuse_thread (struct support_fuse *f, void *closure)
{
  /* Turn on returning FUSE_FORGET responses.  */
  support_fuse_filter_forget (f, false);

  /* Inode and nodeid for "file" and "new".  */
  enum { NODE_FILE = 2, NODE_NEW, NODE_SUBDIR, NODE_SYMLINK };
  struct fuse_in_header *inh;
  while ((inh = support_fuse_next (f)) != NULL)
    {
      {
        char *opcode = support_fuse_opcode (inh->opcode);
        printf ("info: (T) event %s(%llu) len=%u nodeid=%llu\n",
                opcode, (unsigned long long int) inh->unique, inh->len,
                (unsigned long long int) inh->nodeid);
        free (opcode);
      }

      /* Handle mountpoint and basic directory operation for the root (1).  */
      if (support_fuse_handle_mountpoint (f)
          || (inh->nodeid == 1 && support_fuse_handle_directory (f)))
        continue;

      switch (inh->opcode)
        {
        case FUSE_READDIR:
          /* Implementation of getdents64.  */
          if (inh->nodeid == 1)
            {
              struct support_fuse_dirstream *d
                = support_fuse_prepare_readdir (f);
              TEST_COMPARE (support_fuse_cast (READ, inh)->offset, 0);
              TEST_VERIFY (support_fuse_dirstream_add (d, 1, 1, DT_DIR, "."));
              TEST_VERIFY (support_fuse_dirstream_add (d, 1, 2, DT_DIR, ".."));
              TEST_VERIFY (support_fuse_dirstream_add (d, NODE_FILE, 3, DT_REG,
                                                       "file"));
              support_fuse_reply_prepared (f);
            }
          else
            support_fuse_reply_error (f, EIO);
          break;
        case FUSE_LOOKUP:
          /* Part of the implementation of open.  */
          {
            char *name = support_fuse_cast (LOOKUP, inh);
            printf ("  name: %s\n", name);
            if (inh->nodeid == 1 && strcmp (name, "file") == 0)
              {
                struct fuse_entry_out *out
                  = support_fuse_prepare_entry (f, NODE_FILE);
                out->attr.mode = S_IFREG | 0600;
                support_fuse_reply_prepared (f);
              }
            else if (inh->nodeid == 1 && strcmp (name, "symlink") == 0)
              {
                struct fuse_entry_out *out
                  = support_fuse_prepare_entry (f, NODE_SYMLINK);
                out->attr.mode = S_IFLNK | 0777;
                support_fuse_reply_prepared (f);
              }
            else
              support_fuse_reply_error (f, ENOENT);
          }
          break;
        case FUSE_OPEN:
          /* Implementation of open.  */
          {
            struct fuse_open_in *p = support_fuse_cast (OPEN, inh);
            if (inh->nodeid == NODE_FILE)
              {
                TEST_VERIFY (!(p->flags & O_EXCL));
                struct fuse_open_out out = { 0, };
                support_fuse_reply (f, &out, sizeof (out));
              }
            else
              support_fuse_reply_error (f, ENOENT);
          }
          break;
        case FUSE_GETATTR:
          /* Happens after open.  */
          if (inh->nodeid == NODE_FILE)
            {
              struct fuse_attr_out *out = support_fuse_prepare_attr (f);
              out->attr.mode = S_IFREG | 0600;
              out->attr.size = strlen ("Hello, world!");
              support_fuse_reply_prepared (f);
            }
          else
            support_fuse_reply_error (f, ENOENT);
          break;
        case FUSE_READ:
          /* Implementation of read.  */
          if (inh->nodeid == NODE_FILE)
            {
              struct fuse_read_in *p = support_fuse_cast (READ, inh);
              TEST_COMPARE (p->offset, 0);
              TEST_VERIFY (p->size >= strlen ("Hello, world!"));
              support_fuse_reply (f,
                                  "Hello, world!", strlen ("Hello, world!"));
            }
          else
            support_fuse_reply_error (f, EIO);
          break;
        case FUSE_FLUSH:
          /* Sent in response to close.  */
          support_fuse_reply_empty (f);
          break;
        case FUSE_GETXATTR:
          /* This happens as part of a open-for-write operation.
             Signal no support for extended attributes.  */
          support_fuse_reply_error (f, ENOSYS);
          break;
        case FUSE_SETATTR:
          /* This happens as part of a open-for-write operation to
             implement O_TRUNC.  */
          if (inh->nodeid == NODE_FILE)
            {
              struct fuse_setattr_in *p = support_fuse_cast (SETATTR, inh);
              /* FATTR_LOCKOWNER may also be set.  */
              TEST_COMPARE ((p->valid) & ~ FATTR_LOCKOWNER, FATTR_SIZE);
              TEST_COMPARE (p->size, 0);
              struct fuse_attr_out *out = support_fuse_prepare_attr (f);
              out->attr.mode = S_IFREG | 0600;
              support_fuse_reply_prepared (f);
            }
          else
            support_fuse_reply_error (f, EIO);
          break;
        case FUSE_WRITE:
          /* Implementation of write.  */
          if (inh->nodeid == NODE_FILE)
            {
              struct fuse_write_in *p = support_fuse_cast (WRITE, inh);
              TEST_COMPARE (p->offset, 0);
              /* Write payload follows after struct fuse_write_in.  */
              TEST_COMPARE_BLOB (p + 1, p->size,
                                 "Good day to you too.",
                                 strlen ("Good day to you too."));
              struct fuse_write_out out =
                {
                  .size = p->size,
                };
              support_fuse_reply (f, &out, sizeof (out));
            }
          else
            support_fuse_reply_error (f, EIO);
          break;
        case FUSE_CREATE:
          /* Implementation of O_CREAT.  */
          if (inh->nodeid == 1)
            {
              char *name;
              struct fuse_create_in *p
                = support_fuse_cast_name (CREATE, inh, &name);
              TEST_VERIFY (S_ISREG (p->mode));
              TEST_COMPARE (p->mode & 07777, 0600);
              TEST_COMPARE_STRING (name, "new");
              struct fuse_entry_out *out_entry;
              struct fuse_open_out *out_open;
              support_fuse_prepare_create (f, NODE_NEW, &out_entry, &out_open);
              out_entry->attr.mode = S_IFREG | 0600;
              support_fuse_reply_prepared (f);
            }
          else
            support_fuse_reply_error (f, EIO);
          break;
        case FUSE_MKDIR:
          /* Implementation of mkdir.  */
          {
            if (inh->nodeid == 1)
              {
                char *name;
                struct fuse_mkdir_in *p
                  = support_fuse_cast_name (MKDIR, inh, &name);
                TEST_COMPARE (p->mode, 01234);
                TEST_COMPARE_STRING (name, "subdir");
                struct fuse_entry_out *out
                  = support_fuse_prepare_entry (f, NODE_SUBDIR);
                out->attr.mode = S_IFDIR | p->mode;
                support_fuse_reply_prepared (f);
              }
            else
              support_fuse_reply_error (f, EIO);
          }
          break;
        case FUSE_READLINK:
          /* Implementation of readlink.  */
          TEST_COMPARE (inh->nodeid, NODE_SYMLINK);
          if (inh->nodeid == NODE_SYMLINK)
            support_fuse_reply (f, "target-of-symbolic-link",
                                strlen ("target-of-symbolic-link"));
          else
            support_fuse_reply_error (f, EINVAL);
          break;
        case FUSE_FORGET:
          support_fuse_no_reply (f);
          break;
        default:
          support_fuse_reply_error (f, EIO);
        }
    }
}

static int
do_test (void)
{
  support_fuse_init ();

  struct support_fuse *f = support_fuse_mount (fuse_thread, NULL);

  printf ("info: Attributes of mountpoint/root directory %s\n",
          support_fuse_mountpoint (f));
  {
    struct statx st;
    xstatx (AT_FDCWD, support_fuse_mountpoint (f), 0, STATX_BASIC_STATS, &st);
    TEST_COMPARE (st.stx_uid, getuid ());
    TEST_COMPARE (st.stx_gid, getgid ());
    TEST_VERIFY (S_ISDIR (st.stx_mode));
    TEST_COMPARE (st.stx_mode & 07777, 0700);
  }

  printf ("info: List directory %s\n", support_fuse_mountpoint (f));
  {
    DIR *dir = xopendir (support_fuse_mountpoint (f));

    struct dirent *e = xreaddir (dir);
    TEST_COMPARE (e->d_ino, 1);
#ifdef _DIRENT_HAVE_D_OFF
    TEST_COMPARE (e->d_off, 1);
#endif
    TEST_COMPARE (e->d_type, DT_DIR);
    TEST_COMPARE_STRING (e->d_name, ".");

    e = xreaddir (dir);
    TEST_COMPARE (e->d_ino, 1);
#ifdef _DIRENT_HAVE_D_OFF
    TEST_COMPARE (e->d_off, 2);
#endif
    TEST_COMPARE (e->d_type, DT_DIR);
    TEST_COMPARE_STRING (e->d_name, "..");

    e = xreaddir (dir);
    TEST_COMPARE (e->d_ino, 2);
#ifdef _DIRENT_HAVE_D_OFF
    TEST_COMPARE (e->d_off, 3);
#endif
    TEST_COMPARE (e->d_type, DT_REG);
    TEST_COMPARE_STRING (e->d_name, "file");

    TEST_COMPARE (closedir (dir), 0);
  }

  char *file_path = xasprintf ("%s/file", support_fuse_mountpoint (f));

  printf ("info: Attributes of file %s\n", file_path);
  {
    struct statx st;
    xstatx (AT_FDCWD, file_path, 0, STATX_BASIC_STATS, &st);
    TEST_COMPARE (st.stx_uid, getuid ());
    TEST_COMPARE (st.stx_gid, getgid ());
    TEST_VERIFY (S_ISREG (st.stx_mode));
    TEST_COMPARE (st.stx_mode & 07777, 0600);
    TEST_COMPARE (st.stx_size, strlen ("Hello, world!"));
  }

  printf ("info: Read from %s\n", file_path);
  {
    int fd = xopen (file_path, O_RDONLY, 0);
    char buf[64];
    ssize_t len = read (fd, buf, sizeof (buf));
    if (len < 0)
      FAIL_EXIT1 ("read: %m");
    TEST_COMPARE_BLOB (buf, len, "Hello, world!", strlen ("Hello, world!"));
    xclose (fd);
  }

  printf ("info: Write to %s\n", file_path);
  {
    int fd = xopen (file_path, O_WRONLY | O_TRUNC, 0);
    xwrite (fd, "Good day to you too.", strlen ("Good day to you too."));
    xclose (fd);
  }

  printf ("info: Attempt O_EXCL creation of existing %s\n", file_path);
  /* O_EXCL creation shall fail.  */
  errno = 0;
  TEST_COMPARE (open64 (file_path, O_RDWR | O_EXCL | O_CREAT, 0600), -1);
  TEST_COMPARE (errno, EEXIST);

  free (file_path);

  {
    char *new_path = xasprintf ("%s/new", support_fuse_mountpoint (f));
    printf ("info: Test successful O_EXCL creation at %s\n", new_path);
    int fd = xopen (new_path, O_RDWR | O_EXCL | O_CREAT, 0600);
    xclose (fd);
    free (new_path);
  }

  {
    char *subdir_path = xasprintf ("%s/subdir", support_fuse_mountpoint (f));
    xmkdir (subdir_path, 01234);
    free (subdir_path);
  }

  {
    char *symlink_path = xasprintf ("%s/symlink", support_fuse_mountpoint (f));
    char *target = xreadlink (symlink_path);
    TEST_COMPARE_STRING (target, "target-of-symbolic-link");
    free (target);
    free (symlink_path);
  }

  support_fuse_unmount (f);
  return 0;
}

#include <support/test-driver.c>
