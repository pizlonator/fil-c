/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * This file is part of libmount from util-linux project.
 *
 * Copyright (C) 2009-2018 Karel Zak <kzak@redhat.com>
 *
 * libmount is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

/**
 * SECTION: optstr
 * @title: Options string
 * @short_description: low-level API for working with mount options
 *
 * This is a simple and low-level API to working with mount options that are stored
 * in a string.
 */
#include <ctype.h>

#include "strutils.h"
#include "mountP.h"

/*
 * Option location
 */
struct libmnt_optloc {
	char	*begin;
	char	*end;
	char	*value;
	size_t	valsz;
	size_t  namesz;
};

#define MNT_INIT_OPTLOC	{ .begin = NULL }

#define mnt_optmap_entry_novalue(e) \
		(e && (e)->name && !strchr((e)->name, '=') && !((e)->mask & MNT_PREFIX))

/*
 * Locates the first option that matches @name. The @end is set to the
 * char behind the option (it means ',' or \0).
 *
 * @ol is optional.
 *
 * Returns negative number on parse error, 1 when not found and 0 on success.
 */
static int mnt_optstr_locate_option(char *optstr,
					const char *name, size_t namesz,
					struct libmnt_optloc *ol)
{
	char *n;
	size_t nsz;
	int rc;

	if (!optstr)
		return 1;

	assert(name);

	if (!namesz)
		namesz = strlen(name);
	if (!namesz)
		return 1;

	do {
		rc = ul_optstr_next(&optstr, &n, &nsz,
					ol ? &ol->value : NULL,
					ol ? &ol->valsz : NULL);
		if (rc)
			break;

		if (namesz == nsz && strncmp(n, name, nsz) == 0) {
			if (ol) {
				ol->begin = n;
				ol->end = *(optstr - 1) == ',' ? optstr - 1 : optstr;
				ol->namesz = nsz;
			}
			return 0;
		}
	} while(1);

	return rc;
}
/**
 * mnt_optstr_next_option:
 * @optstr: option string, returns the position of the next option
 * @name: returns the option name
 * @namesz: returns the option name length
 * @value: returns the option value or NULL
 * @valuesz: returns the option value length or zero
 *
 * Parses the first option in @optstr.
 *
 * Returns: 0 on success, 1 at the end of @optstr or negative number in case of
 * error.
 */
int mnt_optstr_next_option(char **optstr, char **name, size_t *namesz,
					char **value, size_t *valuesz)
{
	if (!optstr || !*optstr)
		return -EINVAL;

	return ul_optstr_next(optstr, name, namesz, value, valuesz);
}

int mnt_buffer_append_option(struct ul_buffer *buf,
			const char *name, size_t namesz,
			const char *val, size_t valsz,
			int quoted)
{
	int rc = 0;

	if (!ul_buffer_is_empty(buf))
		rc = ul_buffer_append_data(buf, ",", 1);
	if (!rc)
		rc = ul_buffer_append_data(buf, name, namesz);
	if (val && !rc) {
		/* we need to append '=' is value is empty string, see
		 * 727c689908c5e68c92aa1dd65e0d3bdb6d91c1e5 */
		rc = ul_buffer_append_data(buf, "=", 1);
		if (!rc && valsz) {
			if (quoted)
				rc = ul_buffer_append_data(buf, "\"", 1);
			if (!rc)
				rc = ul_buffer_append_data(buf, val, valsz);
			if (quoted)
				rc = ul_buffer_append_data(buf, "\"", 1);
		}
	}
	return rc;
}

/**
 * mnt_optstr_append_option:
 * @optstr: option string or NULL, returns a reallocated string
 * @name: value name
 * @value: value
 *
 * Returns: 0 on success or <0 in case of error. After an error the @optstr should
 *          be unmodified.
 */
int mnt_optstr_append_option(char **optstr, const char *name, const char *value)
{
	struct ul_buffer buf = UL_INIT_BUFFER;
	int rc;
	size_t nsz, vsz, osz;

	if (!optstr)
		return -EINVAL;
	if (!name || !*name)
		return 0;

	nsz = strlen(name);
	osz = *optstr ? strlen(*optstr) : 0;
	vsz = value ? strlen(value) : 0;

	ul_buffer_refer_string(&buf, *optstr);
	ul_buffer_set_chunksize(&buf, osz + nsz + vsz + 3);	/* to call realloc() only once */

	rc = mnt_buffer_append_option(&buf, name, nsz, value, vsz, 0);
	if (!rc)
		*optstr = ul_buffer_get_data(&buf, NULL, NULL);
	else if (osz == 0)
		ul_buffer_free_data(&buf);

	return rc;
}
/**
 * mnt_optstr_prepend_option:
 * @optstr: option string or NULL, returns a reallocated string
 * @name: value name
 * @value: value
 *
 * Returns: 0 on success or <0 in case of error. After an error the @optstr should
 *          be unmodified.
 */
int mnt_optstr_prepend_option(char **optstr, const char *name, const char *value)
{
	struct ul_buffer buf = UL_INIT_BUFFER;
	size_t nsz, vsz, osz;
	int rc;

	if (!optstr)
		return -EINVAL;
	if (!name || !*name)
		return 0;

	nsz = strlen(name);
	osz = *optstr ? strlen(*optstr) : 0;
	vsz = value ? strlen(value) : 0;

	ul_buffer_set_chunksize(&buf, osz + nsz + vsz + 3);   /* to call realloc() only once */

	rc = mnt_buffer_append_option(&buf, name, nsz, value, vsz, 0);
	if (*optstr && !rc) {
		rc = ul_buffer_append_data(&buf, ",", 1);
		if (!rc)
			rc = ul_buffer_append_data(&buf, *optstr, osz);
		free(*optstr);
	}

	if (!rc)
		*optstr = ul_buffer_get_data(&buf, NULL, NULL);
	else
		ul_buffer_free_data(&buf);

	return rc;
}

/**
 * mnt_optstr_get_option:
 * @optstr: string with a comma separated list of options
 * @name: requested option name
 * @value: returns a pointer to the beginning of the value (e.g. name=VALUE) or NULL
 * @valsz: returns size of the value or 0
 *
 * Returns: 0 on success, 1 when not found the @name or negative number in case
 * of error.
 */
int mnt_optstr_get_option(const char *optstr, const char *name,
			  char **value, size_t *valsz)
{
	struct libmnt_optloc ol = MNT_INIT_OPTLOC;
	int rc;

	if (!optstr || !name)
		return -EINVAL;

	rc = mnt_optstr_locate_option((char *) optstr, name, 0, &ol);
	if (!rc) {
		if (value)
			*value = ol.value;
		if (valsz)
			*valsz = ol.valsz;
	}
	return rc;
}

/**
 * mnt_optstr_deduplicate_option:
 * @optstr: string with a comma separated list of options
 * @name: requested option name
 *
 * Removes all instances of @name except the last one.
 *
 * Returns: 0 on success, 1 when not found the @name or negative number in case
 * of error.
 */
int mnt_optstr_deduplicate_option(char **optstr, const char *name)
{
	int rc;
	char *begin = NULL, *end = NULL, *opt;

	if (!optstr || !name)
		return -EINVAL;

	opt = *optstr;
	do {
		struct libmnt_optloc ol = MNT_INIT_OPTLOC;

		rc = mnt_optstr_locate_option(opt, name, 0, &ol);
		if (!rc) {
			if (begin) {
				/* remove the previous instance */
				size_t shift = strlen(*optstr);

				mnt_optstr_remove_option_at(optstr, begin, end);

				/* now all the offsets are not valid anymore - recount */
				shift -= strlen(*optstr);
				ol.begin -= shift;
				ol.end -= shift;
			}
			begin = ol.begin;
			end = ol.end;
			opt = end && *end ? end + 1 : NULL;
		}
		if (opt == NULL)
			break;
	} while (rc == 0 && *opt);

	return rc < 0 ? rc : begin ? 0 : 1;
}

/*
 * The result never starts or ends with a comma or contains two commas
 *    (e.g. ",aaa,bbb" or "aaa,,bbb" or "aaa,")
 */
int mnt_optstr_remove_option_at(char **optstr, char *begin, char *end)
{
	size_t sz;

	if (!optstr || !begin || !end)
		return -EINVAL;

	if ((begin == *optstr || *(begin - 1) == ',') && *end == ',')
		end++;

	sz = strlen(end);

	memmove(begin, end, sz + 1);
	if (!*begin && (begin > *optstr) && *(begin - 1) == ',')
		*(begin - 1) = '\0';

	return 0;
}

/* insert 'substr' or '=substr' to @str on position @pos */
static int __attribute__((nonnull(1,2,3)))
insert_value(char **str, char *pos, const char *substr, char **next)
{
	size_t subsz = strlen(substr);			/* substring size */
	size_t strsz = strlen(*str);
	size_t possz = strlen(pos);
	size_t posoff;
	char *p;
	int sep;

	/* is it necessary to prepend '=' before the substring ? */
	sep = !(pos > *str && *(pos - 1) == '=');

	/* save an offset of the place where we need to add substr */
	posoff = pos - *str;

	p = realloc(*str, strsz + sep + subsz + 1);
	if (!p)
		return -ENOMEM;

	/* zeroize the newly allocated memory -- valgrind loves us... */
	memset(p + strsz, 0, sep + subsz + 1);

	/* set pointers to the reallocated string */
	*str = p;
	pos = p + posoff;

	if (possz)
		/* create a room for the new substring */
		memmove(pos + subsz + sep, pos, possz + 1);
	if (sep)
		*pos++ = '=';

	memcpy(pos, substr, subsz);

	if (next) {
		/* set pointer to the next option */
		*next = pos + subsz;
		if (**next == ',')
			(*next)++;
	}
	return 0;
}

/**
 * mnt_optstr_set_option:
 * @optstr: string with a comma separated list of options
 * @name: requested option
 * @value: new value or NULL
 *
 * Set or unset the option @value.
 *
 * Returns: 0 on success, 1 when not found the @name or negative number in case
 * of error.
 */
int mnt_optstr_set_option(char **optstr, const char *name, const char *value)
{
	struct libmnt_optloc ol = MNT_INIT_OPTLOC;
	char *nameend;
	int rc = 1;

	if (!optstr || !name)
		return -EINVAL;

	if (*optstr)
		rc = mnt_optstr_locate_option(*optstr, name, 0, &ol);
	if (rc < 0)
		return rc;			/* parse error */
	if (rc == 1)
		return mnt_optstr_append_option(optstr, name, value);	/* not found */

	nameend = ol.begin + ol.namesz;

	if (value == NULL && ol.value && ol.valsz)
		/* remove unwanted "=value" */
		mnt_optstr_remove_option_at(optstr, nameend, ol.end);

	else if (value && ol.value == NULL)
		/* insert "=value" */
		rc = insert_value(optstr, nameend, value, NULL);

	else if (value && ol.value && strlen(value) == ol.valsz)
		/* simply replace =value */
		memcpy(ol.value, value, ol.valsz);

	else if (value && ol.value) {
		mnt_optstr_remove_option_at(optstr, nameend, ol.end);
		rc = insert_value(optstr, nameend, value, NULL);
	}
	return rc;
}

/**
 * mnt_optstr_remove_option:
 * @optstr: string with a comma separated list of options
 * @name: requested option name
 *
 * Returns: 0 on success, 1 when not found the @name or negative number in case
 * of error.
 */
int mnt_optstr_remove_option(char **optstr, const char *name)
{
	struct libmnt_optloc ol = MNT_INIT_OPTLOC;
	int rc;

	if (!optstr || !name)
		return -EINVAL;

	rc = mnt_optstr_locate_option(*optstr, name, 0, &ol);
	if (rc != 0)
		return rc;

	mnt_optstr_remove_option_at(optstr, ol.begin, ol.end);
	return 0;
}

/**
 * mnt_split_optstr:
 * @optstr: string with comma separated list of options
 * @user: returns newly allocated string with userspace options
 * @vfs: returns newly allocated string with VFS options
 * @fs: returns newly allocated string with FS options
 * @ignore_user: option mask for options that should be ignored
 * @ignore_vfs: option mask for options that should be ignored
 *
 * For example:
 *
 *	mnt_split_optstr(optstr, &u, NULL, NULL, MNT_NOMTAB, 0);
 *
 * returns all userspace options, the options that do not belong to
 * mtab are ignored.
 *
 * Note that FS options are all options that are undefined in MNT_USERSPACE_MAP
 * or MNT_LINUX_MAP.
 *
 * Returns: 0 on success, or a negative number in case of error.
 */
int mnt_split_optstr(const char *optstr, char **user, char **vfs,
		     char **fs, int ignore_user, int ignore_vfs)
{
	int rc = 0;
	char *name, *val, *str = (char *) optstr;
	size_t namesz, valsz, chunsz;
	struct libmnt_optmap const *maps[2];
	struct ul_buffer xvfs = UL_INIT_BUFFER,
			 xfs = UL_INIT_BUFFER,
			 xuser = UL_INIT_BUFFER;

	if (!optstr)
		return -EINVAL;

	maps[0] = mnt_get_builtin_optmap(MNT_LINUX_MAP);
	maps[1] = mnt_get_builtin_optmap(MNT_USERSPACE_MAP);

	chunsz = strlen(optstr) / 2;

	while (!mnt_optstr_next_option(&str, &name, &namesz, &val, &valsz)) {
		struct ul_buffer *buf = NULL;
		const struct libmnt_optmap *ent = NULL;
		const struct libmnt_optmap *m =
			 mnt_optmap_get_entry(maps, 2, name, namesz, &ent);

		if (ent && !ent->id)
			continue;	/* ignore undefined options (comments) */

		/* ignore name=<value> if options map expects <name> only */
		if (valsz && mnt_optmap_entry_novalue(ent))
			m = NULL;

		if (ent && m && m == maps[0] && vfs) {
			if (ignore_vfs && (ent->mask & ignore_vfs))
				continue;
			if (vfs)
				buf = &xvfs;
		} else if (ent && m && m == maps[1] && user) {
			if (ignore_user && (ent->mask & ignore_user))
				continue;
			if (user)
				buf = &xuser;
		} else if (!m && fs) {
			if (fs)
				buf = &xfs;
		}

		if (buf) {
			if (ul_buffer_is_empty(buf))
				ul_buffer_set_chunksize(buf, chunsz);
			rc = mnt_buffer_append_option(buf, name, namesz, val, valsz, 0);
		}
		if (rc)
			break;
	}

	if (vfs)
		*vfs  = rc ? NULL : ul_buffer_get_data(&xvfs, NULL, NULL);
	if (fs)
		*fs   = rc ? NULL : ul_buffer_get_data(&xfs, NULL, NULL);
	if (user)
		*user = rc ? NULL : ul_buffer_get_data(&xuser, NULL, NULL);
	if (rc) {
		ul_buffer_free_data(&xvfs);
		ul_buffer_free_data(&xfs);
		ul_buffer_free_data(&xuser);
	}

	return rc;
}

/**
 * mnt_optstr_get_options
 * @optstr: string with a comma separated list of options
 * @subset: returns newly allocated string with options
 * @map: options map
 * @ignore: mask of the options that should be ignored
 *
 * Extracts options from @optstr that belong to the @map, for example:
 *
 *	 mnt_optstr_get_options(optstr, &p,
 *			mnt_get_builtin_optmap(MNT_LINUX_MAP),
 *			MNT_NOMTAB);
 *
 * the 'p' returns all VFS options, the options that do not belong to mtab
 * are ignored.
 *
 * Returns: 0 on success, or a negative number in case of error.
 */
int mnt_optstr_get_options(const char *optstr, char **subset,
			    const struct libmnt_optmap *map, int ignore)
{
	struct libmnt_optmap const *maps[1];
	struct ul_buffer buf = UL_INIT_BUFFER;
	char *name, *val, *str = (char *) optstr;
	size_t namesz, valsz;
	int rc = 0;

	if (!optstr || !subset)
		return -EINVAL;

	maps[0] = map;

	ul_buffer_set_chunksize(&buf, strlen(optstr)/2);

	while (!mnt_optstr_next_option(&str, &name, &namesz, &val, &valsz)) {
		const struct libmnt_optmap *ent;

		mnt_optmap_get_entry(maps, 1, name, namesz, &ent);

		if (!ent || !ent->id)
			continue;	/* ignore undefined options (comments) */

		if (ignore && (ent->mask & ignore))
			continue;

		/* ignore name=<value> if options map expects <name> only */
		if (valsz && mnt_optmap_entry_novalue(ent))
			continue;

		rc = mnt_buffer_append_option(&buf, name, namesz, val, valsz, 0);
		if (rc)
			break;
	}

	*subset  = rc ? NULL : ul_buffer_get_data(&buf, NULL, NULL);
	if (rc)
		ul_buffer_free_data(&buf);
	return rc;
}

/*
 * @optstr: string with comma separated list of options
 * @wanted: options expected in @optstr
 * @missing: returns options from @wanted which missing in @optstr (optional)
 *
 * Retursn: <0 on error, 0 on missing options, 1 if nothing is missing
 */
int mnt_optstr_get_missing(const char *optstr, const char *wanted, char **missing)
{
	char *name, *val, *str = (char *) wanted;
	size_t namesz = 0, valsz = 0;
	struct ul_buffer buf = UL_INIT_BUFFER;
	int rc = 0;

	if (!wanted)
		return 1;
	if (missing) {
		/* caller wants data, prepare buffer */
		ul_buffer_set_chunksize(&buf, strlen(wanted) + 3);	/* to call realloc() only once */
		*missing = NULL;
	}

	while (!mnt_optstr_next_option(&str, &name, &namesz, &val, &valsz)) {

		rc = mnt_optstr_locate_option((char *) optstr, name, namesz, NULL);
		if (rc == 1) {			/* not found */
			if (!missing)
				return 0;
			rc = mnt_buffer_append_option(&buf, name, namesz, val, valsz, 0);
		}
		if (rc < 0)
			break;
		rc = 0;
	}

	if (!rc && missing) {
		if (ul_buffer_is_empty(&buf))
			rc = 1;
		else
			*missing = ul_buffer_get_data(&buf, NULL, NULL);
	} else
		ul_buffer_free_data(&buf);

	return rc;
}

/**
 * mnt_optstr_get_flags:
 * @optstr: string with comma separated list of options
 * @flags: returns mount flags
 * @map: options map
 *
 * Returns in @flags IDs of options from @optstr as defined in the @map.
 *
 * For example:
 *
 *	"bind,exec,foo,bar"   --returns->   MS_BIND
 *
 *	"bind,noexec,foo,bar" --returns->   MS_BIND|MS_NOEXEC
 *
 * Note that @flags are not zeroized by this function! This function sets/unsets
 * bits in the @flags only.
 *
 * Returns: 0 on success or negative number in case of error
 */
int mnt_optstr_get_flags(const char *optstr, unsigned long *flags,
		const struct libmnt_optmap *map)
{
	struct libmnt_optmap const *maps[2];
	char *name, *str = (char *) optstr;
	size_t namesz = 0, valsz = 0;
	int nmaps = 0;

	if (!optstr || !flags || !map)
		return -EINVAL;

	maps[nmaps++] = map;

	if (map == mnt_get_builtin_optmap(MNT_LINUX_MAP))
		/*
		 * Add userspace map -- the "user" is interpreted as
		 *                      MS_NO{EXEC,SUID,DEV}.
		 */
		maps[nmaps++] = mnt_get_builtin_optmap(MNT_USERSPACE_MAP);

	while(!mnt_optstr_next_option(&str, &name, &namesz, NULL, &valsz)) {
		const struct libmnt_optmap *ent;
		const struct libmnt_optmap *m;

		m = mnt_optmap_get_entry(maps, nmaps, name, namesz, &ent);
		if (!m || !ent || !ent->id)
			continue;

		/* ignore name=<value> if options map expects <name> only */
		if (valsz && mnt_optmap_entry_novalue(ent))
			continue;

		if (m == map) {				/* requested map */
			if (ent->mask & MNT_INVERT)
				*flags &= ~ent->id;
			else
				*flags |= ent->id;

		} else if (nmaps == 2 && m == maps[1] && valsz == 0) {
			/*
			 * Special case -- translate "user" (but no user=) to
			 * MS_ options
			 */
			if (ent->mask & MNT_INVERT)
				continue;
			if (ent->id & (MNT_MS_OWNER | MNT_MS_GROUP))
				*flags |= MS_OWNERSECURE;
			else if (ent->id & (MNT_MS_USER | MNT_MS_USERS))
				*flags |= MS_SECURE;
		}
	}

	return 0;
}

/**
 * mnt_optstr_apply_flags:
 * @optstr: string with comma separated list of options
 * @flags: returns mount flags
 * @map: options map
 *
 * Removes/adds options to the @optstr according to flags. For example:
 *
 *	MS_NOATIME and "foo,bar,noexec"   --returns->  "foo,bar,noatime"
 *
 * Returns: 0 on success or negative number in case of error.
 *
 * Deprecated: since v2.39.
 */
int mnt_optstr_apply_flags(char **optstr, unsigned long flags,
				const struct libmnt_optmap *map)
{
	struct libmnt_optmap const *maps[1];
	char *name, *next, *val;
	size_t namesz = 0, valsz = 0, multi = 0;
	unsigned long fl;
	int rc = 0;

	if (!optstr || !map)
		return -EINVAL;

	DBG(CXT, ul_debug("applying 0x%08lx flags to '%s'", flags, *optstr));

	maps[0] = map;
	next = *optstr;
	fl = flags;

	/*
	 * There is a convention that 'rw/ro' flags are always at the beginning of
	 * the string (although the 'rw' is unnecessary).
	 */
	if (map == mnt_get_builtin_optmap(MNT_LINUX_MAP)) {
		const char *o = (fl & MS_RDONLY) ? "ro" : "rw";

		if (next &&
		    (!strncmp(next, "rw", 2) || !strncmp(next, "ro", 2)) &&
		    (*(next + 2) == '\0' || *(next + 2) == ',')) {

			/* already set, be paranoid and fix it */
			memcpy(next, o, 2);
		} else {
			rc = mnt_optstr_prepend_option(optstr, o, NULL);
			if (rc)
				goto err;
			next = *optstr;		/* because realloc() */
		}
		fl &= ~MS_RDONLY;
		next += 2;
		if (*next == ',')
			next++;
	}

	if (next && *next) {
		/*
		 * scan @optstr and remove options that are missing in
		 * @flags
		 */
		while(!mnt_optstr_next_option(&next, &name, &namesz,
							&val, &valsz)) {
			const struct libmnt_optmap *ent;

			if (mnt_optmap_get_entry(maps, 1, name, namesz, &ent)) {
				/*
				 * remove unwanted option (rw/ro is already set)
				 */
				if (!ent || !ent->id)
					continue;
				/* ignore name=<value> if options map expects <name> only */
				if (valsz && mnt_optmap_entry_novalue(ent))
					continue;

				if (ent->id == MS_RDONLY ||
				    (ent->mask & MNT_INVERT) ||
				    (fl & ent->id) != (unsigned long) ent->id) {

					char *end = val ? val + valsz :
							  name + namesz;
					next = name;
					rc = mnt_optstr_remove_option_at(
							optstr, name, end);
					if (rc)
						goto err;
				}
				if (!(ent->mask & MNT_INVERT)) {
					/* allow options with prefix (X-mount.foo,X-mount.bar) more than once */
					if (ent->mask & MNT_PREFIX)
						multi |= ent->id;
					else
						fl &= ~ent->id;
					if (ent->id & MS_REC)
						fl |= MS_REC;
				}
			}
		}
	}

	/* remove from flags options which are allowed more than once */
	fl &= ~multi;

	/* add missing options (but ignore fl if contains MS_REC only) */
	if (fl && fl != MS_REC) {

		const struct libmnt_optmap *ent;
		struct ul_buffer buf = UL_INIT_BUFFER;
		size_t sz;
		char *p;

		ul_buffer_refer_string(&buf, *optstr);

		for (ent = map; ent && ent->name; ent++) {
			if ((ent->mask & MNT_INVERT)
			    || ent->id == 0
			    || (fl & ent->id) != (unsigned long) ent->id)
				continue;

			/* don't add options which require values (e.g. offset=%d) */
			p = strchr(ent->name, '=');
			if (p) {
				if (p > ent->name && *(p - 1) == '[')
					p--;			/* name[=] */
				else
					continue;		/* name= */
				sz = p - ent->name;
			} else
				sz = strlen(ent->name);

			rc = mnt_buffer_append_option(&buf, ent->name, sz, NULL, 0, 0);
			if (rc)
				break;
		}

		if (rc) {
			ul_buffer_free_data(&buf);
			goto err;
		} else
			*optstr = ul_buffer_get_data(&buf, NULL, NULL);
	}

	DBG(CXT, ul_debug("new optstr '%s'", *optstr));
	return rc;
err:
	DBG(CXT, ul_debug("failed to apply flags [rc=%d]", rc));
	return rc;
}

/**
 * mnt_match_options:
 * @optstr: options string
 * @pattern: comma delimited list of options
 *
 * The "no" could be used for individual items in the @options list. The "no"
 * prefix does not have a global meaning.
 *
 * Unlike fs type matching, nonetdev,user and nonetdev,nouser have
 * DIFFERENT meanings; each option is matched explicitly as specified.
 *
 * The "no" prefix interpretation could be disabled by the "+" prefix, for example
 * "+noauto" matches if @optstr literally contains the "noauto" string.
 *
 * The alone "no" is error and all matching ends with False.
 *
 * "xxx,yyy,zzz" : "nozzz"	-> False
 *
 * "xxx,yyy,zzz" : "xxx,noeee"	-> True
 *
 * "bar,zzz"     : "nofoo"      -> True		(does not contain "foo")
 *
 * "nofoo,bar"   : "nofoo"      -> True		(does not contain "foo")
 *
 * "nofoo,bar"   : "+nofoo"     -> True		(contains "nofoo")
 *
 * "bar,zzz"     : "+nofoo"     -> False	(does not contain "nofoo")
 *
 * "bar,zzz"     : "" or  "+"   -> True		(empty pattern is matching)
 *
 * ""            : ""           -> True
 *
 * ""            : "foo"        -> False
 *
 * ""            : "nofoo"      -> True
 *
 * ""            : "no,foo"     -> False	(alone "no" is error)
 *
 * "no"          : "+no"        -> True		("no" is an option due to "+")
 *
 * Returns: 1 if pattern is matching, else 0. This function also returns 0
 *          if @pattern is NULL and @optstr is non-NULL.
 */
int mnt_match_options(const char *optstr, const char *pattern)
{
	char *name, *pat = (char *) pattern;
	char *buf = NULL, *patval;
	size_t namesz = 0, patvalsz = 0;
	int match = 1;

	if (!pattern && !optstr)
		return 1;
	if (pattern && optstr && !*pattern && !*optstr)
		return 1;
	if (!pattern)
		return 0;

	/* walk on pattern string
	 */
	while (match && !mnt_optstr_next_option(&pat, &name, &namesz,
						&patval, &patvalsz)) {
		char *val;
		size_t sz = 0;
		int no = 0, rc;

		if (*name == '+')
			name++, namesz--;
		else if ((no = (startswith(name, "no") != NULL))) {
			name += 2, namesz -= 2;
			if (!*name || *name == ',') {
				match = 0;
				break;	/* alone "no" keyword is error */
			}
		}

		if (optstr && *optstr && *name) {
			if (!buf) {
				buf = malloc(strlen(pattern) + 1);
				if (!buf)
					return 0;
			}

			xstrncpy(buf, name, namesz + 1);
			rc = mnt_optstr_get_option(optstr, buf, &val, &sz);

		} else if (!*name) {
			rc = 0;		/* empty pattern matches */
		} else {
			rc = 1;		/* not found in empty string */
		}

		/* check also value (if the pattern is "foo=value") */
		if (rc == 0 && patvalsz > 0 &&
		    (patvalsz != sz || strncmp(patval, val, sz) != 0))
			rc = 1;

		switch (rc) {
		case 0:		/* found */
			match = no == 0 ? 1 : 0;
			break;
		case 1:		/* not found */
			match = no == 1 ? 1 : 0;
			break;
		default:	/* parse error */
			match = 0;
			break;
		}
	}

	free(buf);
	return match;
}

#ifdef TEST_PROGRAM
static int test_append(struct libmnt_test *ts __attribute__((unused)),
		       int argc, char *argv[])
{
	const char *value = NULL, *name;
	char *optstr;
	int rc;

	if (argc < 3)
		return -EINVAL;
	optstr = strdup(argv[1]);
	if (!optstr)
		err_oom();
	name = argv[2];

	if (argc == 4)
		value = argv[3];

	rc = mnt_optstr_append_option(&optstr, name, value);
	if (!rc)
		printf("result: >%s<\n", optstr);
	free(optstr);
	return rc;
}

static int test_prepend(struct libmnt_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	const char *value = NULL, *name;
	char *optstr;
	int rc;

	if (argc < 3)
		return -EINVAL;
	optstr = strdup(argv[1]);
	if (!optstr)
		err_oom();
	name = argv[2];

	if (argc == 4)
		value = argv[3];

	rc = mnt_optstr_prepend_option(&optstr, name, value);
	if (!rc)
		printf("result: >%s<\n", optstr);
	free(optstr);
	return rc;
}

static int test_split(struct libmnt_test *ts __attribute__((unused)),
		      int argc, char *argv[])
{
	char *optstr, *user = NULL, *fs = NULL, *vfs = NULL;
	int rc;

	if (argc < 2)
		return -EINVAL;

	optstr = strdup(argv[1]);
	if (!optstr)
		err_oom();

	rc = mnt_split_optstr(optstr, &user, &vfs, &fs, 0, 0);
	if (!rc) {
		printf("user : %s\n", user);
		printf("vfs  : %s\n", vfs);
		printf("fs   : %s\n", fs);
	}

	free(user);
	free(vfs);
	free(fs);
	free(optstr);
	return rc;
}

static int test_flags(struct libmnt_test *ts __attribute__((unused)),
		      int argc, char *argv[])
{
	char *optstr;
	int rc;
	unsigned long fl = 0;

	if (argc < 2)
		return -EINVAL;

	optstr = strdup(argv[1]);
	if (!optstr)
		err_oom();

	rc = mnt_optstr_get_flags(optstr, &fl, mnt_get_builtin_optmap(MNT_LINUX_MAP));
	if (rc)
		return rc;
	printf("mountflags:           0x%08lx\n", fl);

	fl = 0;
	rc = mnt_optstr_get_flags(optstr, &fl, mnt_get_builtin_optmap(MNT_USERSPACE_MAP));
	if (rc)
		return rc;
	printf("userspace-mountflags: 0x%08lx\n", fl);

	free(optstr);
	return rc;
}

static int test_apply(struct libmnt_test *ts __attribute__((unused)),
		      int argc, char *argv[])
{
	char *optstr;
	int rc, map;
	unsigned long flags;

	if (argc < 4)
		return -EINVAL;

	if (!strcmp(argv[1], "--user"))
		map = MNT_USERSPACE_MAP;
	else if (!strcmp(argv[1], "--linux"))
		map = MNT_LINUX_MAP;
	else {
		fprintf(stderr, "unknown option '%s'\n", argv[1]);
		return -EINVAL;
	}

	optstr = strdup(argv[2]);
	if (!optstr)
		err_oom();
	flags = strtoul(argv[3], NULL, 16);

	printf("flags:  0x%08lx\n", flags);

	rc = mnt_optstr_apply_flags(&optstr, flags, mnt_get_builtin_optmap(map));
	printf("optstr: %s\n", optstr);

	free(optstr);
	return rc;
}

static int test_set(struct libmnt_test *ts __attribute__((unused)),
		    int argc, char *argv[])
{
	const char *value = NULL, *name;
	char *optstr;
	int rc;

	if (argc < 3)
		return -EINVAL;
	optstr = strdup(argv[1]);
	if (!optstr)
		err_oom();
	name = argv[2];

	if (argc == 4)
		value = argv[3];

	rc = mnt_optstr_set_option(&optstr, name, value);
	if (!rc)
		printf("result: >%s<\n", optstr);
	free(optstr);
	return rc;
}

static int test_get(struct libmnt_test *ts __attribute__((unused)),
		    int argc, char *argv[])
{
	char *optstr;
	const char *name;
	char *val = NULL;
	size_t sz = 0;
	int rc;

	if (argc < 2)
		return -EINVAL;
	optstr = argv[1];
	name = argv[2];

	rc = mnt_optstr_get_option(optstr, name, &val, &sz);
	if (rc == 0) {
		printf("found; name: %s", name);
		if (sz) {
			printf(", argument: size=%zd data=", sz);
			if (fwrite(val, 1, sz, stdout) != sz)
				return -1;
		}
		printf("\n");
	} else if (rc == 1)
		printf("%s: not found\n", name);
	else
		printf("parse error: %s\n", optstr);
	return rc;
}

static int test_missing(struct libmnt_test *ts __attribute__((unused)),
		    int argc, char *argv[])
{
	const char *optstr;
	const char *wanted;
	char *missing = NULL;
	int rc;

	if (argc < 2)
		return -EINVAL;
	optstr = argv[1];
	wanted = argv[2];

	rc = mnt_optstr_get_missing(optstr, wanted, &missing);
	if (rc == 0)
		printf("missing: %s\n", missing);
	else if (rc == 1) {
		printf("nothing\n");
		rc = 0;
	} else
		printf("parse error: %s\n", optstr);
	return rc;
}

static int test_remove(struct libmnt_test *ts __attribute__((unused)),
		       int argc, char *argv[])
{
	const char *name;
	char *optstr;
	int rc;

	if (argc < 3)
		return -EINVAL;
	optstr = strdup(argv[1]);
	if (!optstr)
		err_oom();
	name = argv[2];

	rc = mnt_optstr_remove_option(&optstr, name);
	if (!rc)
		printf("result: >%s<\n", optstr);
	free(optstr);
	return rc;
}

static int test_dedup(struct libmnt_test *ts __attribute__((unused)),
		      int argc, char *argv[])
{
	const char *name;
	char *optstr;
	int rc;

	if (argc < 3)
		return -EINVAL;
	optstr = strdup(argv[1]);
	if (!optstr)
		err_oom();
	name = argv[2];

	rc = mnt_optstr_deduplicate_option(&optstr, name);
	if (!rc)
		printf("result: >%s<\n", optstr);
	free(optstr);
	return rc;
}

static int test_match(struct libmnt_test *ts __attribute__((unused)),
		      int argc, char *argv[])
{
	char *optstr, *pattern;

	if (argc < 3)
		return -EINVAL;

	optstr = argv[1];
	pattern = argv[2];
	printf("%-6s: \"%s\"\t:\t\"%s\"\n",
			mnt_match_options(optstr, pattern) == 1 ? "true" : "false",
			optstr, pattern);
	return 0;
}

int main(int argc, char *argv[])
{
	struct libmnt_test tss[] = {
		{ "--append", test_append, "<optstr> <name> [<value>]  append value to optstr" },
		{ "--prepend",test_prepend,"<optstr> <name> [<value>]  prepend value to optstr" },
		{ "--set",    test_set,    "<optstr> <name> [<value>]  (un)set value" },
		{ "--get",    test_get,    "<optstr> <name>            search name in optstr" },
		{ "--missing",test_missing,"<optstr> <wanted>          what from wanted is missing" },
		{ "--remove", test_remove, "<optstr> <name>            remove name in optstr" },
		{ "--dedup",  test_dedup,  "<optstr> <name>            deduplicate name in optstr" },
		{ "--match",  test_match,  "<optstr> <pattern>         compare optstr with pattern" },
		{ "--split",  test_split,  "<optstr>                   split into FS, VFS and userspace" },
		{ "--flags",  test_flags,  "<optstr>                   convert options to MS_* flags" },
		{ "--apply",  test_apply,  "--{linux,user} <optstr> <mask>    apply mask to optstr" },

		{ NULL }
	};
	return  mnt_run_test(tss, argc, argv);
}
#endif /* TEST_PROGRAM */
