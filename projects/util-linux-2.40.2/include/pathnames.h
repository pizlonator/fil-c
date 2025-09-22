/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef PATHNAMES_H
#define PATHNAMES_H

#ifdef HAVE_PATHS_H
# include <paths.h>
#endif

#ifndef __STDC__
# error "we need an ANSI compiler"
#endif

/* used by kernel in /proc (e.g. /proc/swaps) for deleted files */
#define PATH_DELETED_SUFFIX	" (deleted)"

/* DEFPATHs from <paths.h> don't include /usr/local */
#undef _PATH_DEFPATH

#ifdef USE_USRDIR_PATHS_ONLY
# define _PATH_DEFPATH	        "/usr/local/bin:/usr/bin"
#else
# define _PATH_DEFPATH	        "/usr/local/bin:/bin:/usr/bin"
#endif

#undef _PATH_DEFPATH_ROOT

#ifdef USE_USRDIR_PATHS_ONLY
# define _PATH_DEFPATH_ROOT	"/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin"
#else
# define _PATH_DEFPATH_ROOT	"/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin"
#endif

#define	_PATH_HUSHLOGIN		".hushlogin"
#define	_PATH_HUSHLOGINS	"/etc/hushlogins"

#define _PATH_NOLOGIN_TXT	"/etc/nologin.txt"

#ifndef _PATH_MAILDIR
# define _PATH_MAILDIR		"/var/spool/mail"
#endif
#define	_PATH_MOTDFILE		"/usr/share/misc/motd:/run/motd:/etc/motd"
#ifndef _PATH_NOLOGIN
# define _PATH_NOLOGIN		"/etc/nologin"
#endif
#define	_PATH_VAR_NOLOGIN	"/var/run/nologin"

#ifndef _PATH_LOGIN
# define _PATH_LOGIN		"/bin/login"
#endif
#define _PATH_SHUTDOWN		"/sbin/shutdown"
#define _PATH_POWEROFF		"/sbin/poweroff"

#define _PATH_TERMCOLORS_DIRNAME "terminal-colors.d"
#define _PATH_TERMCOLORS_DIR	"/etc/" _PATH_TERMCOLORS_DIRNAME

/* login paths */
#define _PATH_PASSWD		"/etc/passwd"
#define _PATH_GSHADOW		"/etc/gshadow"
#define _PATH_GROUP		"/etc/group"
#define _PATH_SHADOW_PASSWD	"/etc/shadow"
#define _PATH_SHELLS		"/etc/shells"

#ifndef _PATH_TMP
# define _PATH_TMP		"/tmp/"
#endif

#ifndef _PATH_BTMP
# define _PATH_BTMP		"/var/log/btmp"
#endif

#define _PATH_ISSUE_FILENAME	"issue"
#define _PATH_ISSUE_DIRNAME	_PATH_ISSUE_FILENAME ".d"

#define _PATH_ISSUE		"/etc/" _PATH_ISSUE_FILENAME
#define _PATH_ISSUEDIR		"/etc/" _PATH_ISSUE_DIRNAME

#define _PATH_OS_RELEASE_ETC	"/etc/os-release"
#define _PATH_OS_RELEASE_USR	"/usr/lib/os-release"
#define _PATH_NUMLOCK_ON	_PATH_RUNSTATEDIR "/numlock-on"
#define _PATH_LOGINDEFS		"/etc/login.defs"

#define _PATH_SD_UNITSLOAD	_PATH_RUNSTATEDIR "/systemd/systemd-units-load"

/* misc paths */
#define _PATH_WORDS             "/usr/share/dict/words"
#define _PATH_WORDS_ALT         "/usr/share/dict/web2"

/* mount paths */
#define _PATH_FILESYSTEMS	"/etc/filesystems"
#define _PATH_PROC		"/proc"
#define _PATH_PROC_SWAPS	"/proc/swaps"
#define _PATH_PROC_FILESYSTEMS	"/proc/filesystems"
#define _PATH_PROC_MOUNTS	"/proc/mounts"
#define _PATH_PROC_PARTITIONS	"/proc/partitions"
#define _PATH_PROC_DEVICES	"/proc/devices"
#define _PATH_PROC_MOUNTINFO	"/proc/self/mountinfo"
#define _PATH_PROC_LOCKS        "/proc/locks"
#define _PATH_PROC_CDROMINFO	"/proc/sys/dev/cdrom/info"

/* unshare paths */
#define _PATH_SUBUID		"/etc/subuid"
#define _PATH_SUBGID		"/etc/subgid"
#define _PATH_PROC_UIDMAP	"/proc/self/uid_map"
#define _PATH_PROC_GIDMAP	"/proc/self/gid_map"
#define _PATH_PROC_SETGROUPS	"/proc/self/setgroups"

#define _PATH_PROC_FDDIR	"/proc/self/fd"
#define _PATH_PROC_TIMENS_OFF   "/proc/self/timens_offsets"

#define _PATH_PROC_ATTR_CURRENT	"/proc/self/attr/current"
#define _PATH_PROC_ATTR_EXEC	"/proc/self/attr/exec"
#define _PATH_PROC_CAPLASTCAP	"/proc/sys/kernel/cap_last_cap"


#define _PATH_SYS_BLOCK		"/sys/block"
#define _PATH_SYS_DEVBLOCK	"/sys/dev/block"
#define _PATH_SYS_DEVCHAR	"/sys/dev/char"
#define _PATH_SYS_CLASS		"/sys/class"
#define _PATH_SYS_SCSI		"/sys/bus/scsi"
#define _PATH_SYS_CPU_BYTEORDER	"/sys/kernel/cpu_byteorder"
#define _PATH_SYS_ADDRESS_BITS	"/sys/kernel/address_bits"

#define _PATH_SYS_SELINUX	"/sys/fs/selinux"
#define _PATH_SYS_APPARMOR	"/sys/kernel/security/apparmor"

#ifndef _PATH_MOUNTED
# ifdef MOUNTED					/* deprecated */
#  define _PATH_MOUNTED		MOUNTED
# else
#  define _PATH_MOUNTED		"/etc/mtab"
# endif
#endif

#ifndef _PATH_MNTTAB
# ifdef MNTTAB					/* deprecated */
#  define _PATH_MNTTAB		MNTTAB
# else
#  define _PATH_MNTTAB		"/etc/fstab"
# endif
#endif

#ifndef _PATH_DEV
  /*
   * The tailing '/' in _PATH_DEV is there for compatibility with libc.
   */
# define _PATH_DEV		"/dev/"
#endif

#define _PATH_DEV_MAPPER	"/dev/mapper"

#define _PATH_DEV_MEM		"/dev/mem"

#define _PATH_DEV_LOOP		"/dev/loop"
#define _PATH_DEV_LOOPCTL	"/dev/loop-control"

/* udev paths */
#define _PATH_DEV_BYLABEL	"/dev/disk/by-label"
#define _PATH_DEV_BYUUID	"/dev/disk/by-uuid"
#define _PATH_DEV_BYID		"/dev/disk/by-id"
#define _PATH_DEV_BYPATH	"/dev/disk/by-path"
#define _PATH_DEV_BYPARTLABEL	"/dev/disk/by-partlabel"
#define _PATH_DEV_BYPARTUUID	"/dev/disk/by-partuuid"

/* hwclock paths */
#ifdef CONFIG_ADJTIME_PATH
# define _PATH_ADJTIME		CONFIG_ADJTIME_PATH
#else
# define _PATH_ADJTIME		"/etc/adjtime"
#endif

#ifdef __ia64__
# define _PATH_RTC_DEV		"/dev/efirtc"
#else
# define _PATH_RTC_DEV		"/dev/rtc0"
#endif

/* raw paths*/
#define _PATH_RAWDEVDIR		"/dev/raw/"
#define _PATH_RAWDEVCTL		_PATH_RAWDEVDIR "rawctl"
/* deprecated */
#define _PATH_RAWDEVCTL_OLD	"/dev/rawctl"

#define _PATH_PROC_KERNEL	"/proc/sys/kernel"

/* ipc paths */
#define _PATH_PROC_SYSV_MSG	"/proc/sysvipc/msg"
#define _PATH_PROC_SYSV_SEM	"/proc/sysvipc/sem"
#define _PATH_PROC_SYSV_SHM	"/proc/sysvipc/shm"
#define _PATH_PROC_IPC_MSGMAX	_PATH_PROC_KERNEL "/msgmax"
#define _PATH_PROC_IPC_MSGMNB	_PATH_PROC_KERNEL "/msgmnb"
#define _PATH_PROC_IPC_MSGMNI	_PATH_PROC_KERNEL "/msgmni"
#define _PATH_PROC_IPC_SEM	_PATH_PROC_KERNEL "/sem"
#define _PATH_PROC_IPC_SHMALL	_PATH_PROC_KERNEL "/shmall"
#define _PATH_PROC_IPC_SHMMAX	_PATH_PROC_KERNEL "/shmmax"
#define _PATH_PROC_IPC_SHMMNI	_PATH_PROC_KERNEL "/shmmni"

/* util clamp */
#define _PATH_PROC_UCLAMP_MIN	_PATH_PROC_KERNEL "/sched_util_clamp_min"
#define _PATH_PROC_UCLAMP_MAX	_PATH_PROC_KERNEL "/sched_util_clamp_max"

/* sysctl fs paths */
#define _PATH_PROC_SYS_FS	"/proc/sys/fs"
#define _PATH_PROC_PIPE_MAX_SIZE	_PATH_PROC_SYS_FS "/pipe-max-size"

/* irqtop paths */
#define _PATH_PROC_INTERRUPTS	"/proc/interrupts"
#define _PATH_PROC_SOFTIRQS	"/proc/softirqs"
#define _PATH_PROC_UPTIME	"/proc/uptime"

/* kernel command line */
#define _PATH_PROC_CMDLINE	"/proc/cmdline"


/* logger paths */
#define _PATH_DEVLOG		"/dev/log"

/* ctrlaltdel paths */
#define _PATH_PROC_CTRL_ALT_DEL	"/proc/sys/kernel/ctrl-alt-del"

/* lscpu paths */
#define _PATH_PROC_CPUINFO	"/proc/cpuinfo"

/* rfkill paths */
#define _PATH_DEV_RFKILL	"/dev/rfkill"
#define _PATH_SYS_RFKILL	"/sys/class/rfkill"

/* cgroup path */
#define _PATH_SYS_CGROUP	"/sys/fs/cgroup"

/* Maximum number of PIDs system supports */
#define _PATH_PROC_PIDMAX	"/proc/sys/kernel/pid_max"

/* libeconf default */
#ifndef _PATH_VENDORDIR
# define _PATH_VENDORDIR	"/usr/etc"
#endif

#endif /* PATHNAMES_H */
