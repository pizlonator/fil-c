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

/* To run FUSE tests under valgrind, pass the
   --sim-hints=fuse-compatible option to valgrind.  This option tells
   valgrind that additional system calls effectively call back into
   the current program.  */

#ifndef SUPPORT_FUSE_H
#define SUPPORT_FUSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <support/bundled/linux/include/uapi/linux/fuse.h>

/* This function must be called furst, before support_fuse_mount, to
   prepare unprivileged mounting.  */
void support_fuse_init (void);

/* This function can be called instead of support_fuse_init.  It does
   not use mount and user namespaces, so it requires root privileges,
   and cleanup after testing may be incomplete.  This is intended only
   for test development.  */
void support_fuse_init_no_namespace (void);

/* Opaque type for tracking FUSE mount state.  */
struct support_fuse;

/* This function disables a mount point created using
   support_fuse_mount.  */
void support_fuse_unmount (struct support_fuse *) __nonnull ((1));

/* This function is called on a separate thread after calling
   support_fuse_mount.  F is the mount state, and CLOSURE the argument
   that was passed to support_fuse_mount.  The callback function is
   expected to call support_fuse_next to read packets from the kernel
   and handle them according to the test's need.  */
typedef void (*support_fuse_callback) (struct support_fuse *f, void *closure);

/* This function creates a new mount point, implemented by CALLBACK.
   CLOSURE is passed to CALLBACK as the second argument.  */
struct support_fuse *support_fuse_mount (support_fuse_callback callback,
                                         void *closure)
  __nonnull ((1)) __attr_dealloc (support_fuse_unmount, 1);

/* This function returns the path to the mount point for F.  The
   returned string is valid until support_fuse_unmount (F) is called.  */
const char * support_fuse_mountpoint (struct support_fuse *f) __nonnull ((1));


/* Renders the OPCODE as a string (FUSE_* constant.  The caller must
   free the returned string.  */
char * support_fuse_opcode (uint32_t opcode) __attr_dealloc_free;

/* Use to provide a checked cast facility.  Use the
   support_fuse_in_cast macro below.  */
void *support_fuse_cast_internal (struct fuse_in_header *, uint32_t)
  __nonnull ((1));
void *support_fuse_cast_name_internal (struct fuse_in_header *, uint32_t,
                                       size_t skip, char **name)
  __nonnull ((1));

/* The macro expansion support_fuse_in_cast (P, TYPE) casts the
   pointer INH to the appropriate type corresponding to the FUSE_TYPE
   opcode.  It fails (terminates the process) if INH->opcode does not
   match FUSE_TYPE.  The type of the returned pointer matches that of
   the FUSE_* constant.

   Maintenance note: Adding support for additional struct fuse_*_in
   types is generally easy, except when there is trailing data after
   the struct (see below for support_fuse_cast_name, for example), and
   the kernel has changed struct sizes over time.  This has happened
   recently with struct fuse_setxattr_in, and would require special
   handling if implemented.  */
#define support_fuse_payload_type_INIT struct fuse_init_in
#define support_fuse_payload_type_LOOKUP char
#define support_fuse_payload_type_OPEN struct fuse_open_in
#define support_fuse_payload_type_READ struct fuse_read_in
#define support_fuse_payload_type_SETATTR struct fuse_setattr_in
#define support_fuse_payload_type_WRITE struct fuse_write_in
#define support_fuse_payload_type_COPY_FILE_RANGE struct fuse_copy_file_range_in
#define support_fuse_payload_type_COPY_FILE_RANGE_64 struct fuse_copy_file_range_in
#define support_fuse_cast(typ, inh)                     \
  ((support_fuse_payload_type_##typ *)                  \
   support_fuse_cast_internal ((inh), FUSE_##typ))

/* Same as support_fuse_cast, but also writes the passed name to *NAMEP.  */
#define support_fuse_payload_name_type_CREATE struct fuse_create_in
#define support_fuse_payload_name_type_MKDIR struct fuse_mkdir_in
#define support_fuse_cast_name(typ, inh, namep)                         \
  ((support_fuse_payload_name_type_##typ *)                             \
   support_fuse_cast_name_internal                                      \
   ((inh), FUSE_##typ, sizeof (support_fuse_payload_name_type_##typ),   \
    (namep)))

/* This function should be called from the callback function.  It
   returns NULL if the mount point has been unmounted.  The result can
   be cast using support_fuse_in_cast.  The pointer is invalidated
   with the next call to support_fuse_next.

   Typical use involves handling some basics using the
   support_fuse_handle_* building blocks, following by a switch
   statement on the result member of the returned struct, to implement
   what a particular test needs.  Casts to payload data should be made
   using support_fuse_in_cast.

   By default, FUSE_FORGET responses are filtered.  See
   support_fuse_filter_forget for turning that off.  */
struct fuse_in_header *support_fuse_next (struct support_fuse *f)
  __nonnull ((1));

/* This function can be called from a callback function to handle
   basic aspects of directories (OPENDIR, GETATTR, RELEASEDIR).
   inh->nodeid is used as the inode number for the directory.  This
   function must be called after support_fuse_next.  */
bool support_fuse_handle_directory (struct support_fuse *f) __nonnull ((1));

/* This function can be called from a callback function to handle
   access to the mount point itself, after call support_fuse_next.  */
bool support_fuse_handle_mountpoint (struct support_fuse *f) __nonnull ((1));

/* If FILTER_ENABLED, future support_fuse_next calls will not return
   FUSE_FORGET events (and simply discared them, as they require no
   reply).  If !FILTER_ENABLED, the callback needs to handle
   FUSE_FORGET events and call support_fuse_no_reply.  */
void support_fuse_filter_forget (struct support_fuse *f, bool filter_enabled)
  __nonnull ((1));

/* This function should be called from the callback function after
   support_fuse_next returned a non-null pointer.  It sends out a
   response packet on the FUSE device with the supplied payload data.  */
void support_fuse_reply (struct support_fuse *f,
                         const void *payload, size_t payload_size)
  __nonnull ((1)) __attr_access ((__read_only__, 2, 3));

/* This function should be called from the callback function.  It
   replies to a request with an error indicator.  ERROR must be positive.  */
void support_fuse_reply_error (struct support_fuse *f, uint32_t error)
    __nonnull ((1));

/* This function should be called from the callback function.  It
   sends out an empty (but success-indicating) reply packet.  */
void support_fuse_reply_empty (struct support_fuse *f) __nonnull ((1));

/* Do not send a reply.  Only to be used after a support_fuse_next
   call that returned a FUSE_FORGET event.  */
void support_fuse_no_reply (struct support_fuse *f) __nonnull ((1));

/* Specific reponse preparation functions.  The returned object can be
   updated as needed.  If a NODEID argument is present, it will be
   used to set the inode and FUSE nodeid fields.  Without such an
   argument, it is initialized from the current request (if the reply
   requires this field).  This function must be called after
   support_fuse_next.  The actual response must be sent using
   support_fuse_reply_prepared (or a support_fuse_reply_error call can
   be used to cancel the response).  */
struct fuse_entry_out *support_fuse_prepare_entry (struct support_fuse *f,
                                                   uint64_t nodeid)
  __nonnull ((1));
struct fuse_attr_out *support_fuse_prepare_attr (struct support_fuse *f)
  __nonnull ((1));

/* Similar to the other support_fuse_prepare_* functions, but it
   prepares for two response packets.  They can be updated through the
   pointers written to *OUT_ENTRY and *OUT_OPEN prior to calling
   support_fuse_reply_prepared.  */
void support_fuse_prepare_create (struct support_fuse *f,
                                  uint64_t nodeid,
                                  struct fuse_entry_out **out_entry,
                                  struct fuse_open_out **out_open)
  __nonnull ((1, 3, 4));


/* Prepare sending a directory stream.  Must be called after
   support_fuse_next and before support_fuse_dirstream_add.    */
struct support_fuse_dirstream;
struct support_fuse_dirstream *support_fuse_prepare_readdir (struct
                                                             support_fuse *f);

/* Adds directory using D_INO, D_OFF, D_TYPE, D_NAME to the directory
   stream D.  Must be called after support_fuse_prepare_readdir.

   D_OFF is the offset of the next directory entry, not the current
   one.  The first entry has offset zero.  The first requested offset
   can be obtained from the READ payload (struct fuse_read_in) prior
   to calling this function.

   Returns true if the entry could be added to the buffer, or false if
   there was insufficient room.  Sending the buffer is delayed until
   support_fuse_reply_prepared is called.  */
bool support_fuse_dirstream_add (struct support_fuse_dirstream *d,
                                 uint64_t d_ino, uint64_t d_off,
                                 uint32_t d_type,
                                 const char *d_name);

/* Send a prepared response.  Must be called after one of the
   support_fuse_prepare_* functions and before the next
   support_fuse_next call.  */
void support_fuse_reply_prepared (struct support_fuse *f) __nonnull ((1));

#endif /* SUPPORT_FUSE_H */
