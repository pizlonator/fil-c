/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_MOUNT_API_UTILS
#define UTIL_LINUX_MOUNT_API_UTILS

#if defined(HAVE_MOUNTFD_API) && defined(HAVE_LINUX_MOUNT_H)

#include <sys/syscall.h>
#include <linux/mount.h>

/* Accepted by both open_tree() and mount_setattr(). */
#ifndef AT_RECURSIVE
# define AT_RECURSIVE 0x8000
#endif

#ifndef OPEN_TREE_CLONE
# define OPEN_TREE_CLONE 1
#endif

#ifndef OPEN_TREE_CLOEXEC
# define OPEN_TREE_CLOEXEC O_CLOEXEC
#endif

#if !defined(HAVE_OPEN_TREE) && defined(SYS_open_tree)
static inline int open_tree(int dfd, const char *filename, unsigned int flags)
{
	return syscall(SYS_open_tree, dfd, filename, flags);
}
#endif

#ifndef MOVE_MOUNT_F_SYMLINKS
# define MOVE_MOUNT_F_SYMLINKS 0x00000001 /* Follow symlinks on from path */
#endif

#ifndef MOVE_MOUNT_F_AUTOMOUNTS
# define MOVE_MOUNT_F_AUTOMOUNTS 0x00000002 /* Follow automounts on from path */
#endif

#ifndef MOVE_MOUNT_F_EMPTY_PATH
# define MOVE_MOUNT_F_EMPTY_PATH 0x00000004 /* Empty from path permitted */
#endif

#ifndef MOVE_MOUNT_T_SYMLINKS
# define MOVE_MOUNT_T_SYMLINKS 0x00000010 /* Follow symlinks on to path */
#endif

#ifndef MOVE_MOUNT_T_AUTOMOUNTS
# define MOVE_MOUNT_T_AUTOMOUNTS 0x00000020 /* Follow automounts on to path */
#endif

#ifndef MOVE_MOUNT_T_EMPTY_PATH
# define MOVE_MOUNT_T_EMPTY_PATH 0x00000040 /* Empty to path permitted */
#endif

#ifndef MOVE_MOUNT_SET_GROUP
# define MOVE_MOUNT_SET_GROUP	0x00000100 /* Set sharing group instead */
#endif

#ifndef MOVE_MOUNT__MASK
# define MOVE_MOUNT__MASK 0x00000077
#endif

#if !defined(HAVE_MOVE_MOUNT) && defined(SYS_move_mount)
static inline int move_mount(int from_dfd, const char *from_pathname, int to_dfd,
			     const char *to_pathname, unsigned int flags)
{
	return syscall(SYS_move_mount, from_dfd, from_pathname, to_dfd,
		       to_pathname, flags);
}
#endif

#ifndef MOUNT_ATTR_RDONLY
# define MOUNT_ATTR_RDONLY 0x00000001
#endif

#ifndef MOUNT_ATTR_NOSUID
# define MOUNT_ATTR_NOSUID 0x00000002
#endif

#ifndef MOUNT_ATTR_NODEV
# define MOUNT_ATTR_NODEV 0x00000004
#endif

#ifndef MOUNT_ATTR_NOEXEC
# define MOUNT_ATTR_NOEXEC 0x00000008
#endif

#ifndef MOUNT_ATTR__ATIME
# define MOUNT_ATTR__ATIME 0x00000070
#endif

#ifndef MOUNT_ATTR_RELATIME
# define MOUNT_ATTR_RELATIME 0x00000000
#endif

#ifndef MOUNT_ATTR_NOATIME
# define MOUNT_ATTR_NOATIME 0x00000010
#endif

#ifndef MOUNT_ATTR_STRICTATIME
# define MOUNT_ATTR_STRICTATIME 0x00000020
#endif

#ifndef MOUNT_ATTR_NODIRATIME
# define MOUNT_ATTR_NODIRATIME 0x00000080
#endif

#ifndef MOUNT_ATTR_IDMAP
# define MOUNT_ATTR_IDMAP 0x00100000
#endif

#ifndef MOUNT_ATTR_NOSYMFOLLOW
# define MOUNT_ATTR_NOSYMFOLLOW 0x00200000
#endif

#ifndef HAVE_STRUCT_MOUNT_ATTR
# ifndef MOUNT_ATTR_SIZE_VER0 /* For case mount.h comes from a place invisible for autotools/meson */
# include <inttypes.h>
struct mount_attr {
	uint64_t attr_set;
	uint64_t attr_clr;
	uint64_t propagation;
	uint64_t userns_fd;
};
# endif
#endif

#if !defined(HAVE_MOUNT_SETATTR) && defined(SYS_mount_setattr)
static inline int mount_setattr(int dfd, const char *path, unsigned int flags,
				struct mount_attr *attr, size_t size)
{
	return syscall(SYS_mount_setattr, dfd, path, flags, attr, size);
}
#endif

#ifndef HAVE_ENUM_FSCONFIG_COMMAND
# ifndef FSOPEN_CLOEXEC /* For case mount.h comes from a place invisible for autotools/meson */
enum fsconfig_command {
	FSCONFIG_SET_FLAG	= 0,	/* Set parameter, supplying no value */
	FSCONFIG_SET_STRING	= 1,	/* Set parameter, supplying a string value */
	FSCONFIG_SET_BINARY	= 2,	/* Set parameter, supplying a binary blob value */
	FSCONFIG_SET_PATH	= 3,	/* Set parameter, supplying an object by path */
	FSCONFIG_SET_PATH_EMPTY	= 4,	/* Set parameter, supplying an object by (empty) path */
	FSCONFIG_SET_FD		= 5,	/* Set parameter, supplying an object by fd */
	FSCONFIG_CMD_CREATE	= 6,	/* Invoke superblock creation */
	FSCONFIG_CMD_RECONFIGURE = 7,	/* Invoke superblock reconfiguration */
};
# endif
#endif

#if !defined(HAVE_FSCONFIG) && defined(SYS_fsconfig)
static inline int fsconfig(int fd, unsigned int cmd, const char *key,
                    const void *value, int aux)
{
        return syscall(SYS_fsconfig, fd, cmd, key, value, aux);
}
#endif

#ifndef FSOPEN_CLOEXEC
# define FSOPEN_CLOEXEC          0x00000001
#endif

#if !defined(HAVE_FSOPEN) && defined(SYS_fsopen)
static inline int fsopen(const char *fsname, unsigned int flags)
{
        return syscall(SYS_fsopen, fsname, flags);
}
#endif

#ifndef FSMOUNT_CLOEXEC
# define FSMOUNT_CLOEXEC         0x00000001
#endif

#if !defined(HAVE_FSMOUNT) && defined(SYS_fsmount)
static inline int fsmount(int fd, unsigned int flags, unsigned int mount_attrs)
{
        return syscall(SYS_fsmount, fd, flags, mount_attrs);
}
#endif

#ifndef FSPICK_CLOEXEC
# define FSPICK_CLOEXEC          0x00000001
#endif

#ifndef FSPICK_SYMLINK_NOFOLLOW
# define FSPICK_SYMLINK_NOFOLLOW 0x00000002
#endif

#ifndef FSPICK_NO_AUTOMOUNT
# define FSPICK_NO_AUTOMOUNT     0x00000004
#endif

#ifdef FSPICK_EMPTY_PATH
# define FSPICK_EMPTY_PATH       0x00000008
#endif

#if !defined(HAVE_FSPICK) && defined(SYS_fspick)
static inline int fspick(int dfd, const char *pathname, unsigned int flags)
{
        return syscall(SYS_fspick, dfd, pathname, flags);
}
#endif

#endif /* HAVE_MOUNTFD_API && HAVE_LINUX_MOUNT_H */
#endif /* UTIL_LINUX_MOUNT_API_UTILS */

