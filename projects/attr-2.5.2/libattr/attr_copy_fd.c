/*
  Copyright (C) 2002 Andreas Gruenbacher <agruen@suse.de>, SuSE Linux AG.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this manual.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Copy extended attributes between files. */

#if defined (HAVE_CONFIG_H)
#include "config.h"
#endif

#include <sys/types.h>
# include <sys/xattr.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(HAVE_ALLOCA_H)
# include <alloca.h>
#endif

#if defined(HAVE_ATTR_LIBATTR_H)
# include "attr/libattr.h"
#endif

#define ERROR_CONTEXT_MACROS
#include "error_context.h"
#include "nls.h"

#if !defined(ENOTSUP)
# define ENOTSUP (-1)
#endif

#if defined(HAVE_ALLOCA)
# define my_alloc(size) alloca (size)
# define my_free(ptr) do { } while(0)
#else
# define my_alloc(size) malloc (size)
# define my_free(ptr) free (ptr)
#endif

/* Copy extended attributes from src_path to dst_path. If the file
   has an extended Access ACL (system.posix_acl_access) and that is
   copied successfully, the file mode permission bits are copied as
   a side effect. This may not always the case, so the file mode
   and/or ownership must be copied separately. */
int
attr_copy_fd(const char *src_path, int src_fd,
	     const char *dst_path, int dst_fd,
	     int (*check) (const char *, struct error_context *),
	     struct error_context *ctx)
{
#if defined(HAVE_FLISTXATTR) && defined(HAVE_FGETXATTR) && \
    defined(HAVE_FSETXATTR)
	int ret = 0;
	ssize_t size;
	char *names = NULL, *end_names, *name, *value = NULL;
	unsigned int setxattr_ENOTSUP = 0;

	/* ignore acls by default */
	if (check == NULL)
		check = attr_copy_check_permissions;

	size = flistxattr (src_fd, NULL, 0);
	if (size < 0) {
		if (errno != ENOSYS && errno != ENOTSUP) {
			const char *qpath = quote (ctx, src_path);
			error (ctx, _("listing attributes of %s"), qpath);
			quote_free (ctx, qpath);
			ret = -1;
		}
		goto getout;
	}
	names = (char *) my_alloc (size+1);
	if (names == NULL) {
		error (ctx, "");
		ret = -1;
		goto getout;
	}
	size = flistxattr (src_fd, names, size);
	if (size < 0) {
		const char *qpath = quote (ctx, src_path);
		error (ctx, _("listing attributes of %s"), qpath);
		quote_free (ctx, qpath);
		ret = -1;
		goto getout;
	} else {
		names[size] = '\0';
		end_names = names + size;
	}

	for (name = names; name != end_names; name = strchr(name, '\0') + 1) {
		void *old_value;

		/* check if this attribute shall be preserved */
		if (!*name || !check(name, ctx))
			continue;

		size = fgetxattr (src_fd, name, NULL, 0);
		if (size < 0) {
			const char *qpath = quote (ctx, src_path);
			const char *qname = quote (ctx, name);
			error (ctx, _("getting attribute %s of %s"),
			       qname, qpath);
			quote_free (ctx, qname);
			quote_free (ctx, qpath);
			ret = -1;
			continue;
		}
		value = (char *) realloc (old_value = value, size);
		if (size != 0 && value == NULL) {
			free(old_value);
			error (ctx, "");
			ret = -1;
		}
		size = fgetxattr (src_fd, name, value, size);
		if (size < 0) {
			const char *qpath = quote (ctx, src_path);
			const char *qname = quote (ctx, name);
			error (ctx, _("getting attribute %s of %s"),
			       qname, qpath);
			quote_free (ctx, qname);
			quote_free (ctx, qpath);
			ret = -1;
			continue;
		}
		if (fsetxattr (dst_fd, name, value, size, 0) != 0) {
			if (errno == ENOTSUP)
				setxattr_ENOTSUP++;
			else {
				const char *qpath = quote (ctx, dst_path);
				if (errno == ENOSYS) {
					error (ctx, _("setting attributes for "
					       "%s"), qpath);
					ret = -1;
					/* no hope of getting any further */
					break;
				} else {
					const char *qname = quote (ctx, name);
					error (ctx, _("setting attribute %s for "
					       "%s"), qname, qpath);
					quote_free (ctx, qname);
					ret = -1;
				}
				quote_free (ctx, qpath);
			}
		}
	}
	if (setxattr_ENOTSUP) {
		const char *qpath = quote (ctx, dst_path);
		errno = ENOTSUP;
		error (ctx, _("setting attributes for %s"), qpath);
		ret = -1;
		quote_free (ctx, qpath);
	}
getout:
	free (value);
	my_free (names);
	return ret;
#else
	return 0;
#endif
}

