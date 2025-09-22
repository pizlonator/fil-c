/* SPDX-License-Identifier: BSD-2-Clause

  Copyright (c) 2023, Thorsten Kukuk <kukuk@suse.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _LIBLASTLOG2_H
#define _LIBLASTLOG2_H

#ifdef __cplusplus
extern "C" {
#endif

#define LL2_DEFAULT_DATABASE _PATH_LOCALSTATEDIR "/lib/lastlog/lastlog2.db"

#include <stdint.h>

struct ll2_context;

/* Set the ll2 context/environment */
/* Returns the context or NULL if an error has happened. */
extern struct ll2_context * ll2_new_context(const char *db_path);

/* Release ll2 context/environment */
extern void ll2_unref_context(struct ll2_context *context);

/* Writes a new entry. Returns 0 on success, -ENOMEM or -1 on other failure. */
extern int ll2_write_entry (struct ll2_context *context, const char *user,
			    int64_t ll_time, const char *tty,
			    const char *rhost, const char *pam_service,
			    char **error);

/* Calling a defined function for each entry. Returns 0 on success, -ENOMEM or -1 on other failure. */
extern int ll2_read_all (struct ll2_context *context,
			 int (*callback)(const char *user, int64_t ll_time,
					 const char *tty, const char *rhost,
					 const char *pam_service, const char *cb_error),
			 char **error);

/* Reads one entry from database and returns that.
   Returns 0 on success, -ENOMEM or -1 on other failure. */
extern int ll2_read_entry (struct ll2_context *context, const char *user,
			   int64_t *ll_time, char **tty, char **rhost,
			   char **pam_service, char **error);

/* Write a new entry with updated login time.
   Returns 0 on success, -ENOMEM or -1 on other failure. */
extern int ll2_update_login_time (struct ll2_context *context,
				  const char *user, int64_t ll_time,
				  char **error);

/* Remove an user entry. Returns 0 on success, -ENOMEM or -1 on other failure. */
extern int ll2_remove_entry (struct ll2_context *context, const char *user,
			     char **error);

/* Renames an user entry. Returns 0 on success, -ENOMEM or -1 on other failure. */
extern int ll2_rename_user (struct ll2_context *context, const char *user,
			    const char *newname, char **error);


/* Import old lastlog file.
   Returns 0 on success, -ENOMEM or -1 on other failure. */
extern int ll2_import_lastlog (struct ll2_context *context,
		               const char *lastlog_file, char **error);

#ifdef __cplusplus
}
#endif

#endif /* _LIBLASTLOG2_H */
