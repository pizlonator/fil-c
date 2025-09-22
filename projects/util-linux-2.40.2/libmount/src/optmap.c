/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * This file is part of libmount from util-linux project.
 *
 * Copyright (C) 2010-2018 Karel Zak <kzak@redhat.com>
 *
 * libmount is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

/**
 * SECTION: optmap
 * @title: Option maps
 * @short_description: description for mount options
 *
 * The mount(2) linux syscall uses two arguments for mount options:
 *
 *	@mountflags: (see MS_* macros in linux/fs.h)
 *
 *	@mountdata: (usually a comma separated string of options)
 *
 * The libmount uses options-map(s) to describe mount options.
 *
 * The option description (map entry) includes:
 *
 *	@name: and argument name
 *
 *	@id: (in the map unique identifier or a mountflags, e.g MS_RDONLY)
 *
 *	@mask: (MNT_INVERT, MNT_NOMTAB)
 *
 * The option argument value is defined by:
 *
 *	"="   -- required argument, e.g "comment="
 *
 *	"[=]" -- optional argument, e.g. "loop[=]"
 *
 * Example:
 *
 * <informalexample>
 *   <programlisting>
 *     #define MY_MS_FOO   (1 << 1)
 *     #define MY_MS_BAR   (1 << 2)
 *
 *     libmnt_optmap myoptions[] = {
 *       { "foo",   MY_MS_FOO },
 *       { "nofoo", MY_MS_FOO | MNT_INVERT },
 *       { "bar=",  MY_MS_BAR },
 *       { NULL }
 *     };
 *   </programlisting>
 * </informalexample>
 *
 * The libmount defines two basic built-in options maps:
 *
 *	@MNT_LINUX_MAP: fs-independent kernel mount options (usually MS_* flags)
 *
 *	@MNT_USERSPACE_MAP: userspace specific mount options (e.g. "user", "loop")
 *
 * For more details about option map struct see "struct mnt_optmap" in
 * mount/mount.h.
 */
#include "mountP.h"
#include "strutils.h"

/*
 * fs-independent mount flags (built-in MNT_LINUX_MAP)
 */
static const struct libmnt_optmap linux_flags_map[] =
{
   { "ro",       MS_RDONLY },                 /* read-only */
   { "rw",       MS_RDONLY, MNT_INVERT },     /* read-write */
   { "exec",     MS_NOEXEC, MNT_INVERT },     /* permit execution of binaries */
   { "noexec",   MS_NOEXEC },                 /* don't execute binaries */
   { "suid",     MS_NOSUID, MNT_INVERT },     /* honor suid executables */
   { "nosuid",   MS_NOSUID },                 /* don't honor suid executables */
   { "dev",      MS_NODEV, MNT_INVERT },      /* interpret device files  */
   { "nodev",    MS_NODEV },                  /* don't interpret devices */

   { "sync",     MS_SYNCHRONOUS, MNT_SUPERBLOCK },            /* synchronous I/O */
   { "async",    MS_SYNCHRONOUS, MNT_INVERT | MNT_SUPERBLOCK },/* asynchronous I/O */

   { "dirsync",  MS_DIRSYNC, MNT_SUPERBLOCK },/* synchronous directory modifications */
   { "remount",  MS_REMOUNT, MNT_NOMTAB },    /* alter flags of mounted FS */
   { "bind",     MS_BIND },                   /* Remount part of the tree elsewhere */
   { "rbind",    MS_BIND | MS_REC },          /* Idem, plus mounted subtrees */
#ifdef MS_NOSUB
   { "sub",      MS_NOSUB, MNT_INVERT },      /* allow submounts */
   { "nosub",    MS_NOSUB },                  /* don't allow submounts */
#endif
#ifdef MS_SILENT
   { "silent",	 MS_SILENT },                 /* be quiet  */
   { "loud",     MS_SILENT, MNT_INVERT },     /* print out messages. */
#endif
#ifdef MS_MANDLOCK
   { "mand",     MS_MANDLOCK, MNT_SUPERBLOCK },               /* Allow mandatory locks on this FS */
   { "nomand",   MS_MANDLOCK, MNT_INVERT | MNT_SUPERBLOCK},   /* Forbid mandatory locks on this FS */
#endif
#ifdef MS_NOATIME
   { "atime",    MS_NOATIME, MNT_INVERT },    /* Update access time */
   { "noatime",	 MS_NOATIME },                /* Do not update access time */
#endif
#ifdef MS_I_VERSION
   { "iversion", MS_I_VERSION },              /* Update inode I_version time */
   { "noiversion", MS_I_VERSION,  MNT_INVERT},/* Don't update inode I_version time */
#endif
#ifdef MS_NODIRATIME
   { "diratime", MS_NODIRATIME, MNT_INVERT }, /* Update dir access times */
   { "nodiratime", MS_NODIRATIME },           /* Do not update dir access times */
#endif
#ifdef MS_RELATIME
   { "relatime", MS_RELATIME },               /* Update access times relative to mtime/ctime */
   { "norelatime", MS_RELATIME, MNT_INVERT }, /* Update access time without regard to mtime/ctime */
#endif
#ifdef MS_STRICTATIME
   { "strictatime", MS_STRICTATIME },         /* Strict atime semantics */
   { "nostrictatime", MS_STRICTATIME, MNT_INVERT }, /* kernel default atime */
#endif
#ifdef MS_LAZYTIME
   { "lazytime", MS_LAZYTIME, MNT_SUPERBLOCK },               /* Update {a,m,c}time on the in-memory inode only */
   { "nolazytime", MS_LAZYTIME, MNT_INVERT | MNT_SUPERBLOCK },
#endif
#ifdef MS_PROPAGATION
   { "unbindable",  MS_UNBINDABLE,          MNT_NOHLPS | MNT_NOMTAB }, /* Unbindable */
   { "runbindable", MS_UNBINDABLE | MS_REC, MNT_NOHLPS | MNT_NOMTAB },
   { "private",     MS_PRIVATE,             MNT_NOHLPS | MNT_NOMTAB }, /* Private */
   { "rprivate",    MS_PRIVATE | MS_REC,    MNT_NOHLPS | MNT_NOMTAB },
   { "slave",       MS_SLAVE,               MNT_NOHLPS | MNT_NOMTAB }, /* Slave */
   { "rslave",      MS_SLAVE | MS_REC,      MNT_NOHLPS | MNT_NOMTAB },
   { "shared",      MS_SHARED,              MNT_NOHLPS | MNT_NOMTAB }, /* Shared */
   { "rshared",     MS_SHARED | MS_REC,     MNT_NOHLPS | MNT_NOMTAB },
#endif
#ifdef MS_NOSYMFOLLOW
   { "symfollow", MS_NOSYMFOLLOW, MNT_INVERT }, /* Don't follow symlinks */
   { "nosymfollow", MS_NOSYMFOLLOW },
#endif
#ifdef MS_MOVE
   { "move",	MS_MOVE,	MNT_NOHLPS | MNT_NOMTAB | MNT_NOFSTAB }, /* --move */
#endif
   { NULL, 0, 0 }
};

/*
 * userspace mount option (built-in MNT_USERSPACE_MAP)
 */
static const struct libmnt_optmap userspace_opts_map[] =
{
   { "defaults", 0, MNT_NOHLPS },      /* default options */

   { "auto",    MNT_MS_NOAUTO, MNT_NOHLPS | MNT_INVERT | MNT_NOMTAB },  /* Can be mounted using -a */
   { "noauto",  MNT_MS_NOAUTO, MNT_NOHLPS | MNT_NOMTAB },  /* Can only be mounted explicitly */

   { "user[=]", MNT_MS_USER },                             /* Allow ordinary user to mount (mtab) */
   { "nouser",  MNT_MS_USER, MNT_INVERT | MNT_NOMTAB },    /* Forbid ordinary user to mount */

   { "users",   MNT_MS_USERS, MNT_NOMTAB },                /* Allow ordinary users to mount */
   { "nousers", MNT_MS_USERS, MNT_INVERT | MNT_NOMTAB },   /* Forbid ordinary users to mount */

   { "owner",   MNT_MS_OWNER, MNT_NOMTAB },                /* Let the owner of the device mount */
   { "noowner", MNT_MS_OWNER, MNT_INVERT | MNT_NOMTAB },   /* Device owner has no special privs */

   { "group",   MNT_MS_GROUP, MNT_NOMTAB },                /* Let the group of the device mount */
   { "nogroup", MNT_MS_GROUP, MNT_INVERT | MNT_NOMTAB },   /* Device group has no special privs */

   /*
    * Note that traditional init scripts assume the _netdev option in /etc/mtab to
    * umount network block devices on shutdown.
    */
   { "_netdev", MNT_MS_NETDEV },                           /* Device requires network */

   { "comment=", MNT_MS_COMMENT, MNT_NOHLPS | MNT_NOMTAB },/* fstab comment only */

   { "x-",      MNT_MS_XCOMMENT,   MNT_NOHLPS | MNT_PREFIX },              /* persistent comments (utab) */
   { "X-",      MNT_MS_XFSTABCOMM, MNT_NOHLPS | MNT_NOMTAB | MNT_PREFIX }, /* fstab only comments */

   { "loop[=]", MNT_MS_LOOP, MNT_NOHLPS },                             /* use the loop device */
   { "offset=", MNT_MS_OFFSET, MNT_NOHLPS | MNT_NOMTAB },		   /* loop device offset */
   { "sizelimit=", MNT_MS_SIZELIMIT, MNT_NOHLPS | MNT_NOMTAB },	   /* loop device size limit */
   { "encryption=", MNT_MS_ENCRYPTION, MNT_NOHLPS | MNT_NOMTAB },	   /* loop device encryption */

   { "nofail",  MNT_MS_NOFAIL, MNT_NOMTAB },               /* Do not fail if ENOENT on dev */

   { "uhelper=", MNT_MS_UHELPER },			   /* /sbin/umount.<helper> */

   { "helper=", MNT_MS_HELPER },			   /* /sbin/mount.<helper> */

   { "verity.hashdevice=", MNT_MS_HASH_DEVICE, MNT_NOHLPS | MNT_NOMTAB },     /* mount a verity device */
   { "verity.roothash=",   MNT_MS_ROOT_HASH, MNT_NOHLPS | MNT_NOMTAB },		   /* verity device root hash */
   { "verity.hashoffset=", MNT_MS_HASH_OFFSET, MNT_NOHLPS | MNT_NOMTAB },	   /* verity device hash offset */
   { "verity.roothashfile=", MNT_MS_ROOT_HASH_FILE, MNT_NOHLPS | MNT_NOMTAB },/* verity device root hash (read from file) */
   { "verity.fecdevice=",   MNT_MS_FEC_DEVICE, MNT_NOHLPS | MNT_NOMTAB },		/* verity FEC device */
   { "verity.fecoffset=", MNT_MS_FEC_OFFSET, MNT_NOHLPS | MNT_NOMTAB },	      /* verity FEC area offset */
   { "verity.fecroots=", MNT_MS_FEC_ROOTS, MNT_NOHLPS | MNT_NOMTAB },	      /* verity FEC roots */
   { "verity.roothashsig=",    MNT_MS_ROOT_HASH_SIG, MNT_NOHLPS | MNT_NOMTAB },	/* verity device root hash signature file */
   { "verity.oncorruption=",   MNT_MS_VERITY_ON_CORRUPTION, MNT_NOHLPS | MNT_NOMTAB },	/* verity: action the kernel takes on corruption */

   { NULL, 0, 0 }
};

/**
 * mnt_get_builtin_map:
 * @id: map id -- MNT_LINUX_MAP or MNT_USERSPACE_MAP
 *
 * MNT_LINUX_MAP - Linux kernel fs-independent mount options
 *                 (usually MS_* flags, see linux/fs.h)
 *
 * MNT_USERSPACE_MAP - userspace mount(8) specific mount options
 *                     (e.g user=, _netdev, ...)
 *
 * Returns: static built-in libmount map.
 */
const struct libmnt_optmap *mnt_get_builtin_optmap(int id)
{
	assert(id);

	if (id == MNT_LINUX_MAP)
		return linux_flags_map;
	if (id == MNT_USERSPACE_MAP)
		return userspace_opts_map;
	return NULL;
}

/*
 * Looks up the @name in @maps and returns a map and in @mapent
 * returns the map entry
 */
const struct libmnt_optmap *mnt_optmap_get_entry(
				struct libmnt_optmap const **maps,
				int nmaps,
				const char *name,
				size_t namelen,
				const struct libmnt_optmap **mapent)
{
	int i;

	assert(maps);
	assert(nmaps);
	assert(name);
	assert(namelen);

	if (mapent)
		*mapent = NULL;

	for (i = 0; i < nmaps; i++) {
		const struct libmnt_optmap *map = maps[i];
		const struct libmnt_optmap *ent;
		const char *p;

		for (ent = map; ent && ent->name; ent++) {
			if (ent->mask & MNT_PREFIX) {
				if (startswith(name, ent->name)) {
					if (mapent)
						*mapent = ent;
					return map;
				}
				continue;
			}
			if (strncmp(ent->name, name, namelen) != 0)
				continue;
			p = ent->name + namelen;
			if (*p == '\0' || *p == '=' || *p == '[') {
				if (mapent)
					*mapent = ent;
				return map;
			}
		}
	}
	return NULL;
}

