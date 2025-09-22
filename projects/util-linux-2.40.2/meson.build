project('util-linux', 'c',
        version : run_command('tools/git-version-gen', check: true).stdout(),
        meson_version: '>=0.60.0',
        license : 'GPLv2+',
        default_options : [
          'c_std=c99',
          'cpp_std=c++11',
        ])

fs = import('fs')
pkgconfig = import('pkgconfig')

# soname versions; This never change because we use symbols versioing. There is also
# API version (LIB*_VERSION macros) and it follow package version.
libblkid_version = '1.1.0'
libblkid_date = '04-Jul-2024'
libuuid_version = '1.3.0'
liblastlog2_version = '2.0.0'
libmount_version = '1.1.0'
libsmartcols_version = '1.1.0'
libfdisk_version = '1.1.0'

prefixdir = get_option('prefix')
if not prefixdir.startswith('/')
        error('Prefix is not absolute: "@0@"'.format(prefixdir))
endif
bindir = join_paths(prefixdir, get_option('bindir'))
sbindir = join_paths(prefixdir, get_option('sbindir'))
sysconfstaticdir = join_paths(prefixdir, 'lib')
docdir = join_paths(prefixdir, get_option('datadir'), 'doc', 'util-linux')
mandir = join_paths(prefixdir, get_option('mandir'))
runstatedir = '/run'
localstatedir = '/var'
execprefixdir = prefixdir
sysconfdir = join_paths(prefixdir, get_option('sysconfdir'))
usrbin_exec_dir = join_paths(execprefixdir, bindir)
usrsbin_exec_dir = join_paths(execprefixdir, sbindir)
bash_completion = dependency('bash-completion', required : get_option('build-bash-completion'))

vendordir = get_option('vendordir')

add_project_arguments('-D_GNU_SOURCE', language : 'c')

if host_machine.system() == 'darwin'
  add_project_arguments('-D_DARWIN_C_SOURCE', language : 'c')
endif

cc = meson.get_compiler('c')

conf = configuration_data()
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
package_string = '@0@ @1@'.format(meson.project_name(), meson.project_version())
conf.set_quoted('PACKAGE_STRING', package_string)

pc_version = []
pc_version = meson.project_version().split('-')[0].split('.')

if pc_version.length() < 3
    pc_version += '0'
endif

pc_version = '.'.join(pc_version)

conf.set_quoted('LIBBLKID_VERSION', pc_version)
conf.set_quoted('LIBBLKID_DATE', libblkid_date)

conf.set('bindir', bindir)
conf.set('sbindir', sbindir)
conf.set('runstatedir', runstatedir)
conf.set('localstatedir', localstatedir)
conf.set('sysconfdir', sysconfdir)
conf.set('usrbin_execdir', usrbin_exec_dir)
conf.set('usrsbin_execdir', usrsbin_exec_dir)
conf.set('docdir', docdir)
conf.set_quoted('_PATH_SYSCONFSTATICDIR', sysconfstaticdir)
conf.set_quoted('_PATH_RUNSTATEDIR', runstatedir)
conf.set_quoted('_PATH_LOCALSTATEDIR', localstatedir)
conf.set_quoted('CONFIG_ADJTIME_PATH', '/etc/adjtime')
conf.set_quoted('ADJTIME_PATH', '/etc/adjtime') # yes, both are used :(

if vendordir != ''
  conf.set_quoted('_PATH_VENDORDIR', vendordir)
endif
conf.set('USE_VENDORDIR', vendordir == '' ? false : 1)

build_libblkid = get_option('build-libblkid').allowed()
conf.set('HAVE_LIBBLKID', build_libblkid ? 1 : false)
summary('libblkid', build_libblkid ? 'enabled' : 'disabled', section : 'components')

build_libuuid = not get_option('build-libuuid').disabled()
conf.set('HAVE_LIBUUID', build_libuuid ? 1 : false)
summary('libuuid', build_libuuid ? 'enabled' : 'disabled', section : 'components')

build_liblastlog2 = not get_option('build-liblastlog2').disabled()
conf.set('HAVE_LIBLASTLOG2', build_liblastlog2 ? 1 : false)
summary('liblastlog2', build_liblastlog2 ? 'enabled' : 'disabled', section : 'components')

have_mountfd_api = cc.sizeof('struct mount_attr', prefix : '#include <linux/mount.h>') > 0
conf.set('HAVE_STRUCT_MOUNT_ATTR', have_mountfd_api ? 1 : false)
conf.set('HAVE_MOUNTFD_API', have_mountfd_api ? 1 : false)

have_struct_statx = cc.sizeof('struct statx', prefix : '#include <sys/stat.h>') > 0
conf.set('HAVE_STRUCT_STATX', have_struct_statx ? 1 : false)

build_libmount = get_option('build-libmount').require(get_option('build-libblkid').allowed()).allowed()

conf.set('HAVE_LIBMOUNT', build_libmount ? 1 : false)
conf.set('USE_LIBMOUNT_SUPPORT_NAMESPACES', 1)
conf.set('USE_LIBMOUNT_MOUNTFD_SUPPORT', have_mountfd_api ? 1 : false)
summary('libmount', build_libmount ? 'enabled' : 'disabled', section : 'components')

build_libsmartcols = not get_option('build-libsmartcols').disabled()
conf.set('HAVE_LIBSMARTCOLS', build_libsmartcols ? 1 : false)
summary('libsmartcols', build_libsmartcols ? 'enabled' : 'disabled', section : 'components')

build_libfdisk = not get_option('build-libfdisk').require(get_option('build-libblkid').allowed()).disabled()
conf.set('HAVE_LIBFDISK', build_libfdisk ? 1 : false)
summary('libfdisk', build_libfdisk ? 'enabled' : 'disabled', section : 'components')

build_uuidd = not get_option('build-uuidd').disabled()
conf.set('HAVE_UUIDD', build_uuidd ? 1 : false)
summary('uuidd', build_uuidd ? 'enabled' : 'disabled', section : 'components')

static_programs = get_option('static-programs')
need_static_libs = static_programs.length() > 0 # a rough estimate...
summary('static programs', static_programs)

LINUX = host_machine.system() in ['linux']
BSD = host_machine.system() in ['dragonfly', 'freebsd', 'netbsd', 'openbsd']

############################################################

code = '''
#include <wchar.h>
#include <wctype.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
  wchar_t wc;
  wint_t w;
  w = fgetwc(stdin);
  if (w == WEOF)
    return 1;
  wc = w;
  fputwc(wc,stdout);
  return 0;
}
'''
have = cc.compiles(code, name : 'wchar_t support')
if not have and get_option('widechar').enabled()
  error('widechar support requested but unavailable')
endif
if get_option('ncurses').enabled() and get_option('widechar').enabled()
  error('widechar support is incompatible with non-wide ncurses')
endif
conf.set('HAVE_WIDECHAR', have ? 1 : false)

headers = '''
        byteswap.h
        crypt.h
        endian.h
        err.h
        errno.h
        fcntl.h
        getopt.h
        inttypes.h
        langinfo.h
        lastlog.h
        libutil.h
        locale.h
        mntent.h
        paths.h
        pty.h
        shadow.h
        stdint.h
        stdio_ext.h
        stdlib.h
        string.h
        strings.h
        unistd.h
        utmp.h
        utmpx.h
        asm-generic/fcntl.h
        asm/fcntl.h
        asm/io.h
        linux/blkzoned.h
        linux/capability.h
        linux/cdrom.h
        linux/compiler.h
        linux/falloc.h
        linux/fd.h
        linux/fs.h
	linux/fiemap.h
	linux/gsmmux.h
        linux/if_alg.h
        linux/landlock.h
        linux/kcmp.h
        linux/net_namespace.h
        linux/nsfs.h
        linux/mount.h
        linux/pr.h
        linux/securebits.h
        linux/tiocl.h
        linux/version.h
        linux/watchdog.h
        net/if.h
        net/if_dl.h
        netinet/in.h
        security/openpam.h
        security/pam_appl.h
        security/pam_misc.h
        security/pam_modules.h
        sys/disk.h
        sys/disklabel.h
        sys/endian.h
        sys/file.h
        sys/io.h
        sys/ioccom.h
        sys/ioctl.h
        sys/mkdev.h
        sys/mount.h
        sys/param.h
        sys/pidfd.h
        sys/prctl.h
        sys/resource.h
	sys/sendfile.h
        sys/signalfd.h
        sys/socket.h
        sys/sockio.h
        sys/stat.h
        sys/statfs.h
        sys/swap.h
        sys/syscall.h
        sys/sysmacros.h
        sys/time.h
        sys/timex.h
        sys/ttydefaults.h
        sys/types.h
        sys/ucred.h
        sys/un.h
        sys/vfs.h
	sys/xattr.h
'''.split()

lib_m = cc.find_library('m')

lib_tinfo = dependency(
  'tinfo',
  required : get_option('tinfo'))

lib_ncursesw = dependency(
  'ncursesw',
  required : get_option('ncursesw'))
if lib_ncursesw.found()
  headers += ['ncursesw/ncurses.h',
              'ncursesw/term.h',
              'ncurses.h',
              'term.h']
  lib_ncurses = disabler()
else
  lib_ncurses = dependency(
    'ncurses',
    required : get_option('ncurses'))
  headers += ['ncurses.h',
              'term.h']
endif

conf.set('HAVE_LIBNCURSESW', lib_ncursesw.found())
conf.set('HAVE_LIBNCURSES', lib_ncurses.found())
conf.set('HAVE_NCURSES', lib_ncursesw.found() or lib_ncurses.found())

lib_slang = dependency(
  'slang',
  required : get_option('slang'))
if lib_slang.found()
  headers += ['slang.h',
              'slang/slang.h',
              'slcurses.h',
              'slang/slcurses.h']
endif
conf.set('HAVE_SLANG', lib_slang.found())

foreach curses_libs : [lib_slang, lib_ncursesw, lib_ncurses]
  if curses_libs.found()
    have = cc.has_function('use_default_colors', dependencies : curses_libs)
    conf.set('HAVE_USE_DEFAULT_COLORS', have ? 1 : false)
    have = cc.has_function('resizeterm', dependencies : curses_libs)
    conf.set('HAVE_RESIZETERM', have ? 1 : false)
    break
  endif
endforeach

lib_z = dependency(
  'zlib',
  required : get_option('zlib'))

lib_readline = dependency(
  'readline',
  required : get_option('readline'))
conf.set('HAVE_LIBREADLINE', lib_readline.found() ? 1 : false)

lib_readline_static = dependency(
  'readline',
  static : true,
  required : need_static_libs ? get_option('readline') : disabler())

if meson.version().version_compare('>= 0.59.0')
  lib_intl = dependency(
    'intl',
    required : get_option('nls'))
  conf.set('ENABLE_NLS', lib_intl.found() ? 1 : false)
else
  if get_option('nls').enabled()
    error('nls is not supported with meson before 0.59.0')
  endif
  lib_intl = dependency('', required : false)
endif

lib_user = dependency(
  'libuser',
  version : '>= 0.58',
  required : get_option('libuser'))
conf.set('HAVE_LIBUSER', lib_user.found() ? 1 : false)

lib_util = cc.find_library(
  'util',
  required : get_option('libutil'))
conf.set('HAVE_LIBUTIL', lib_util.found() ? 1 : false)

lib_utempter = cc.find_library(
  'utempter',
  required : get_option('libutempter'))
conf.set('HAVE_LIBUTEMPTER', lib_utempter.found() ? 1 : false)

systemd = dependency(
  'systemd',
  required : get_option('systemd'))

lib_systemd = dependency(
  'libsystemd',
  required : get_option('systemd'))
conf.set('HAVE_LIBSYSTEMD', lib_systemd.found() ? 1 : false)
conf.set('USE_SYSTEMD', lib_systemd.found() ? 1 : false)

have = cc.has_function(
  'sd_session_get_username',
  dependencies : lib_systemd)
conf.set('HAVE_DECL_SD_SESSION_GET_USERNAME', have ? 1 : false)

lib_udev = dependency(
  'libudev',
  required : get_option('systemd'))
conf.set('HAVE_LIBUDEV', lib_udev.found() ? 1 : false)

lib_crypt = cc.find_library('crypt', required : get_option('build-newgrp'))
if not lib_crypt.found()
  lib_crypt = cc.find_library('crypt', required : get_option('build-sulogin'))
endif

lib_pam = cc.find_library('pam', required : get_option('build-login'))
if not lib_pam.found()
  lib_pam = cc.find_library('pam', required : get_option('build-chfn-chsh'))
endif
if not lib_pam.found()
  lib_pam = cc.find_library('pam', required : get_option('build-su'))
endif
if not lib_pam.found()
  lib_pam = cc.find_library('pam', required : get_option('build-runuser'))
endif
if lib_pam.found()
  lib_pam_misc = cc.find_library('pam_misc')
  lib_pam = [lib_pam, lib_pam_misc]
else
  lib_pam_misc = declare_dependency()
endif

lib_cryptsetup = dependency(
  'libcryptsetup',
  required : get_option('cryptsetup'))
conf.set('HAVE_CRYPTSETUP', lib_cryptsetup.found() ? 1 : false)

cryptsetup_dlopen = not get_option('cryptsetup').disabled() and get_option('cryptsetup-dlopen').enabled()
if cryptsetup_dlopen
  if meson.version().version_compare('>= 0.62.0')
    lib_dl = dependency('dl')
  else
    lib_dl = cc.find_library('dl')
  endif
  conf.set('CRYPTSETUP_VIA_DLOPEN', 1)
  summary('cryptsetup support (dlopen)',
          'enabled',
          section : 'components')
else
  summary('cryptsetup support',
          lib_cryptsetup.found()  ? 'enabled' : 'disabled',
          section : 'components')
endif

have = cc.has_function(
  'crypt_activate_by_signed_key',
  dependencies : lib_cryptsetup)
conf.set('HAVE_CRYPT_ACTIVATE_BY_SIGNED_KEY', have ? 1 : false)

lib_cap_ng = dependency(
  'libcap-ng',
  required : get_option('build-setpriv'))
if not lib_cap_ng.found()
  lib_cap_ng = disabler()
endif

lib_selinux = dependency(
  'libselinux',
  version : '>= 2.5',
  required : get_option('selinux'))
conf.set('HAVE_LIBSELINUX', lib_selinux.found() ? 1 : false)

lib_magic = dependency(
  'libmagic',
  required : get_option('magic'))
conf.set('HAVE_MAGIC', lib_magic.found() ? 1 : false)

lib_econf = dependency(
  'libeconf',
  required : get_option('econf'))
conf.set('HAVE_LIBECONF', lib_econf.found() ? 1 : false)

lib_audit = dependency(
  'audit',
  required : get_option('audit'))
conf.set('HAVE_LIBAUDIT', lib_audit.found() ? 1 : false)

conf.set('HAVE_SMACK', not get_option('smack').disabled())

lib_sqlite3 = dependency('sqlite3', required : get_option('build-liblastlog2'))

foreach header : headers
  have = cc.has_header(header)
  conf.set('HAVE_' + header.underscorify().to_upper(), have ? 1 : false)
endforeach

header = 'linux/btrfs.h'
enable_btrfs = cc.has_header(header,
                             required : get_option('btrfs'))
conf.set('HAVE_' + header.underscorify().to_upper(), enable_btrfs ? 1 : false)
conf.set('HAVE_BTRFS_SUPPORT', enable_btrfs ? 1 : false)

prefix = conf.get('HAVE_LINUX_COMPILER_H') ? '#include <linux/compiler.h>' : ''
foreach header : [
  'linux/blkpg.h',
  'linux/major.h',
]
  have = cc.has_header(header,
                       prefix : prefix)
  conf.set('HAVE_' + header.underscorify().to_upper(), have ? 1 : false)
endforeach

have = cc.has_header('sched.h')
conf.set10('HAVE_DECL_CPU_ALLOC', have)
have_cpu_set_t = cc.has_type('cpu_set_t', args : '-D_GNU_SOURCE', prefix : '#include <sched.h>')
conf.set('HAVE_CPU_SET_T', have_cpu_set_t ? 1 : false)

have = cc.has_header_symbol('unistd.h', 'environ', args : '-D_GNU_SOURCE')
conf.set('HAVE_ENVIRON_DECL', have ? 1 : false)

have = cc.has_header_symbol('signal.h', 'sighandler_t', args : '-D_GNU_SOURCE')
conf.set('HAVE_SIGHANDLER_T', have ? 1 : false)

have = cc.has_function('strsignal')
conf.set10('HAVE_STRSIGNAL_DECL', have)

have = cc.sizeof('union semun', prefix : '#include <sys/sem.h>') > 0
conf.set('HAVE_UNION_SEMUN', have ? 1 : false)

have = cc.has_type('loff_t',
                   args : '-D_GNU_SOURCE',
                   prefix : '#include <sys/types.h>')
conf.set('HAVE_LOFF_T', have ? 1 : false)

have = cc.compiles('''
   #define _GNU_SOURCE 1
   #include <langinfo.h>
   int main(void) {
        char *str;
        str = nl_langinfo (ALTMON_1);
        str = nl_langinfo (ALTMON_2);
        str = nl_langinfo (ALTMON_3);
        str = nl_langinfo (ALTMON_4);
        str = nl_langinfo (ALTMON_5);
        str = nl_langinfo (ALTMON_6);
        str = nl_langinfo (ALTMON_7);
        str = nl_langinfo (ALTMON_8);
        str = nl_langinfo (ALTMON_9);
        str = nl_langinfo (ALTMON_10);
        str = nl_langinfo (ALTMON_11);
        str = nl_langinfo (ALTMON_12);
        return 0;
   }
   ''',
  name : 'langinfo.h defines ALTMON_x constants')
conf.set('HAVE_LANGINFO_ALTMON', have ? 1 : false)

have = cc.compiles('''
   #define _GNU_SOURCE 1
   #include <langinfo.h>
   int main(void) {
        char *str;
        str = nl_langinfo (_NL_ABALTMON_1);
        str = nl_langinfo (_NL_ABALTMON_2);
        str = nl_langinfo (_NL_ABALTMON_3);
        str = nl_langinfo (_NL_ABALTMON_4);
        str = nl_langinfo (_NL_ABALTMON_5);
        str = nl_langinfo (_NL_ABALTMON_6);
        str = nl_langinfo (_NL_ABALTMON_7);
        str = nl_langinfo (_NL_ABALTMON_8);
        str = nl_langinfo (_NL_ABALTMON_9);
        str = nl_langinfo (_NL_ABALTMON_10);
        str = nl_langinfo (_NL_ABALTMON_11);
        str = nl_langinfo (_NL_ABALTMON_12);
        return 0;
   }
   ''',
  name : 'langinfo.h defines _NL_ABALTMON_x constants')
conf.set('HAVE_LANGINFO_NL_ABALTMON', have ? 1 : false)

have = cc.compiles('''
   #define _GNU_SOURCE 1
   #include <langinfo.h>
   int main(void) {
        char *str;
        str = nl_langinfo (_NL_TIME_WEEK_1STDAY);
        return 0;
   }
   ''',
  name : 'langinfo.h defines _NL_TIME_WEEK_1STDAY constant')
conf.set('HAVE_DECL__NL_TIME_WEEK_1STDAY', have ? 1 : false)

funcs = '''
        cachestat
        clearenv
        close_range
        __fpurge
        fpurge
        __fpending
        secure_getenv
        __secure_getenv
        eaccess
        err
        errx
        explicit_bzero
        fnmatch
        fseeko
        fsconfig
        fsmount
        fsopen
        fspick
        fsync
        getttynam
        utimensat
        getdomainname
        getdtablesize
        getexecname
        getmntinfo
        getrandom
        getrlimit
        getsgnam
        inotify_init
        jrand48
        landlock_create_ruleset
        landlock_add_rule
        landlock_restrict_self
        lchown
        lgetxattr
        llistxattr
        llseek
        newlocale
        mkostemp
        move_mount
        mount_setattr
        nanosleep
        ntp_gettime
        open_tree
        personality
        pidfd_open
        pidfd_send_signal
        posix_fadvise
        prctl
        qsort_r
        reallocarray
        renameat2
        rpmatch
        scandirat
        setprogname
        sendfile
        setns
        setresgid
        setresuid
        sched_setattr
        sched_setscheduler
        sigqueue
        srandom
        statx
        strnchr
        strndup
        strnlen
        strtod_l
        sysconf
        sysinfo
        swapon
        swapoff
        timegm
        unshare
        usleep
        uselocale
        utimensat
        vwarnx
        warn
        warnx
        prlimit

        openat
        fstatat
        unlinkat
        ioperm
        iopl
        futimens
        inotify_init1
        open_memstream
        reboot
        getusershell
'''.split()

foreach func: funcs
  have = cc.has_function(func)
  # For autotools compatibility, use either #define FOO 1 or #undef FOO.
  # This makes little sense, but is necessary to avoid warnings about
  # redefined macros from Python.h, which uses this convention.
  conf.set('HAVE_' + func.to_upper(), have ? 1 : false)
endforeach

have_mempcpy = cc.has_function('mempcpy', prefix: '#include <string.h>', args: '-D_GNU_SOURCE')
conf.set('HAVE_MEMPCPY', have_mempcpy ? 1 : false)

have = conf.get('HAVE_FUTIMENS') in [1] and conf.get('HAVE_INOTIFY_INIT1') in [1]
conf.set('AGETTY_RELOAD', have ? 1 : false)
if not have
  warning('futimens or inotify_init1 not found; agetty(8) will not provide --reload functionality')
endif

have_dirfd = (cc.has_function('dirfd') or
              cc.has_header_symbol('dirent.h', 'dirfd',
                                   prefix : '#include <sys/types.h>'))
conf.set('HAVE_DIRFD', have_dirfd ? 1 : false)

have_ddfd = cc.has_member('DIR', 'dd_fd',
                          prefix : '''
                          #include <sys/types.h>
                          #include <dirent.h>
                          ''')
conf.set('HAVE_DECL_DDFD', have_ddfd ? 1 : false)

have = cc.has_member('struct tm', 'tm_gmtoff',
                     args : '-D_GNU_SOURCE',
                     prefix : '''
                     #include <time.h>
                     #include <unistd.h>
                     ''')
conf.set('HAVE_TM_GMTOFF', have ? 1 : false)



have = cc.sizeof('enum fsconfig_command', prefix : '#include <linux/mount.h>') > 0
conf.set('HAVE_ENUM_FSCONFIG_COMMAND', have ? 1 : false)

have = cc.has_member('struct termios', 'c_line',
                     prefix : '#include <termios.h>')
conf.set('HAVE_STRUCT_TERMIOS_C_LINE', have ? 1 : false)

have = cc.has_member('struct stat', 'st_mtim.tv_nsec',
                     args : '-D_GNU_SOURCE',
                     prefix : '#include <sys/stat.h>')
conf.set('HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC', have ? 1 : false)

have = cc.has_member('struct statx', 'stx_mnt_id',
                     prefix : '#include <sys/stat.h>')
conf.set('HAVE_STRUCT_STATX_STX_MNT_ID', have ? 1 : false)

# replacement for AC_STRUCT_TIMEZONE
have = cc.has_member('struct tm', 'tm_zone',
                     args : '-D_GNU_SOURCE',
                     prefix : '#include <time.h>')
conf.set('HAVE_STRUCT_TM_TM_ZONE', have ? 1 : false)

have = cc.has_header_symbol('time.h', 'tzname', args: '-D_GNU_SOURCE')
conf.set('HAVE_DECL_TZNAME', have ? 1 : false)

have = cc.has_header_symbol('linux/blkzoned.h', 'BLK_ZONE_REP_CAPACITY')
conf.set('HAVE_DECL_BLK_ZONE_REP_CAPACITY', have ? 1 : false)

have = cc.has_header_symbol('linux/pr.h', 'PR_REP_CAPACITY')
conf.set('HAVE_DECL_PR_REP_CAPACITY', have ? 1 : false)

code = '''
#include <time.h>
#if !@0@
extern char *tzname[];
#endif
int main(void) {
  return tzname ? 0 : 1;
}
'''.format(have ? 1 : 0)
have = cc.compiles(code, name : 'using tzname[]')
conf.set('HAVE_TZNAME', have ? 1 : false)

have = cc.sizeof('time_t', prefix : '#include <time.h>')
if have < 8
  add_global_arguments('-D_TIME_BITS=64', language : 'c')
  have = cc.sizeof('time_t', args : '-D_TIME_BITS=64', prefix : '#include <time.h>')
  if have < 8
    if get_option('allow-32bit-time')
      warning('Could not make time_t 64bits wide')
    else
      error('Could not make time_t 64bits wide')
    endif
  endif
endif

socket_libs = []
if not cc.has_function('socket')
  socket_libs += cc.find_library('socket', required : true)
  have = cc.has_function('socket',
                         dependencies : socket_libs)
  if not have
    error('socket() function not available')
  endif
endif

lib_rt = cc.find_library('rt', required : false)
realtime_libs = []
have = cc.has_function('clock_gettime')
if not have
  if lib_rt.found()
    realtime_libs += lib_rt
    have = cc.has_function('clock_gettime',
                          dependencies : realtime_libs)
  endif
endif
conf.set('HAVE_CLOCK_GETTIME', have ? 1 : false)

thread_libs = dependency('threads')

have = cc.has_function('timer_create')
if not have
  if lib_rt.found()
    realtime_libs = [lib_rt]
    have = cc.has_function('timer_create',
                           dependencies : realtime_libs)
  endif
  if not have
    realtime_libs += thread_libs
    have = cc.has_function('timer_create',
                           dependencies : realtime_libs)
  endif
endif
conf.set('HAVE_TIMER_CREATE', have ? 1 : false)
if not have
  have = cc.has_function('setitimer')
  conf.set('HAVE_SETITIMER', have ? 1 : false)
endif

rtas_libs = cc.find_library('rtas', required : false)
conf.set('HAVE_LIBRTAS', rtas_libs.found() ? 1 : false)

math_libs = []
if not cc.has_header_symbol('math.h', 'isnan')
  lib = cc.find_library('m', required : true)
  if (cc.has_function('isnan', dependencies : lib) and
      cc.has_function('__isnan', dependencies : lib))
    math_libs += lib
  endif
endif

have = cc.has_header_symbol('errno.h', 'program_invocation_short_name',
                            args : '-D_GNU_SOURCE')
conf.set('HAVE_PROGRAM_INVOCATION_SHORT_NAME', have ? 1 : false)

code = '''
extern char *__progname;
int main(void) {
    return (*__progname != 0);
}
'''
have = cc.compiles(code, name : 'using __progname')
conf.set('HAVE___PROGNAME', have ? 1 : false)

have_pty = conf.get('HAVE_PTY_H').to_string() == '1' \
           and conf.get('HAVE_SYS_SIGNALFD_H').to_string() == '1'
conf.set('HAVE_PTY', have_pty ? 1 : false)

have_opal_get_status= cc.has_header_symbol('linux/sed-opal.h', 'IOC_OPAL_GET_STATUS')
conf.set('HAVE_OPAL_GET_STATUS', have_opal_get_status ? 1 : false)

build_plymouth_support = get_option('build-plymouth-support')
have_tiocglcktrmios = cc.has_header_symbol(
  'sys/ioctl.h', 'TIOCGLCKTRMIOS',
  required : build_plymouth_support.enabled())
have_sock_cloexec = cc.has_header_symbol(
  'sys/socket.h', 'SOCK_CLOEXEC',
  prefix : '#include <sys/types.h>',
  required : build_plymouth_support.enabled())
have_sock_nonblock = cc.has_header_symbol(
  'sys/socket.h', 'SOCK_NONBLOCK',
  prefix : '#include <sys/types.h>',
  required : build_plymouth_support.enabled())
have_so_passcred = cc.has_header_symbol(
  'sys/socket.h', 'SO_PASSCRED',
  args : ['-D_GNU_SOURCE'],
  prefix : '#include <sys/types.h>',
  required : build_plymouth_support.enabled())

build_plymouth_support = (not build_plymouth_support.disabled() and 
                          have_tiocglcktrmios and
                          have_sock_cloexec and
                          have_sock_nonblock and
                          have_so_passcred)
conf.set('ENABLE_PLYMOUTH_SUPPORT', build_plymouth_support ? 1 : false)
summary('plymouth support',
        build_plymouth_support  ? 'enabled' : 'disabled',
        section : 'components')

# check for valid fallocate() function
# with 32 bits glibc 2.10, fallocate() exists but not fallocate64()
# when _FILE_OFFSET_BITS==64, fallocate() is redirect to fallocate64()
# and program can't be linked.
code = '''
#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>

int main(void) {
   long ret;
   ret = fallocate(0, FALLOC_FL_KEEP_SIZE, 0xfffffffful, 0xfffffffful);
   return ret == 0 ? 0 : 1;
}
'''
have = cc.links(code, name : 'fallocate() function')
conf.set('HAVE_FALLOCATE', have ? 1 : false)

code = '''
#include <unistd.h>
#include <fcntl.h>

int main(void) {
   long ret;
   ret = posix_fallocate(0, 0xfffffffful, 0xfffffffful);
   return ret == 0 ? 0 : 1;
}
'''
have = cc.links(code, name : 'posix_fallocate() function')
conf.set('HAVE_POSIX_FALLOCATE', have ? 1 : false)

use_hwclock_cmos = host_machine.cpu_family() in ['x86', 'x86_64']
message('Use CMOS clock: @0@'.format(use_hwclock_cmos))
conf.set('USE_HWCLOCK_CMOS', use_hwclock_cmos ? 1 : false)

conf.set('HAVE_TLS', get_option('use-tls') ? 1 : false)
conf.set('PG_BELL', get_option('pg-bell') ? 1 : false)
conf.set('USE_COLORS_BY_DEFAULT', get_option('colors-default') ? 1 : false)

is_glibc = cc.has_header_symbol('limits.h', '__GLIBC__')

############################################################


fs_search_path = get_option('fs-search-path')
fs_search_path_extra = get_option('fs-search-path-extra')
if fs_search_path_extra != ''
  fs_search_path = ':'.join(fs_search_path, fs_search_path_extra)
endif
conf.set_quoted('FS_SEARCH_PATH', fs_search_path)

systemdsystemunitdir = ''
if systemd.found()
  systemdsystemunitdir = systemd.get_variable(pkgconfig : 'systemdsystemunitdir')
endif

sysvinit = get_option('sysvinit').enabled()
sysvinitrcdir = sysconfdir + '/init.d'

program_tests = get_option('program-tests')

chfn_chsh_password = get_option('chfn-chsh-password') or lib_user.found()
conf.set('CHFN_CHSH_PASSWORD', chfn_chsh_password ? 1 : false)

have = get_option('chsh-only-listed')
conf.set('ONLY_LISTED_SHELLS', have ? 1 : false)

have = get_option('use-tty-group')
conf.set('USE_TTY_GROUP', have ? 1 : false)

bison = find_program('bison')
flex = find_program('flex')
sed = find_program('sed')

build_hwclock = not get_option('build-hwclock').disabled()
bison_gen = generator(
  bison,
  output : ['@BASENAME@.tab.c', '@BASENAME@.tab.h'],
  arguments : ['@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@'])

python_program = find_program('python3', 'python', native : true)

meson_make_symlink = meson.current_source_dir() + '/tools/meson-make-symlink.sh'
meson_make_manpage_stub = meson.current_source_dir() + '/tools/meson-make-manpage-stub.sh'

configure_file(
  output : 'config.h',
  configuration : conf)

add_project_arguments('-include', meson.current_build_dir() / 'config.h', language : 'c')

compiler_flags = [
  '-fno-common',

  '-Waddress-of-packed-member',
  '-Wdiscarded-qualifiers',
  '-Wembedded-directive',
  '-Wextra-semi',
  '-Wformat-security',
  '-Wimplicit-function-declaration',
  '-Wmissing-declarations',
  '-Wmissing-parameter-type',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wno-missing-field-initializers',
  '-Wold-style-definition',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wsign-compare',
  '-Wstrict-prototypes',
  '-Wtype-limits',
  '-Wuninitialized',
  '-Wunused-but-set-parameter',
  '-Wunused-but-set-variable',
  '-Wunused-parameter',
  '-Wunused-result',
  '-Wunused-variable',
  '-Wvla',
  '-Walloca',
]
foreach compiler_flag : compiler_flags
  if cc.has_argument(compiler_flag)
    add_project_arguments(compiler_flag, language : 'c')
  endif
endforeach

manadocs = []
manlinks = {}

bashcompletions = []
bashcompletionslinks = {}

subdir('include')
subdir('lib')
subdir('libblkid')
subdir('libmount')
subdir('libsmartcols')
subdir('libuuid')
subdir('liblastlog2')
subdir('libfdisk')
subdir('pam_lastlog2')
subdir('login-utils')
subdir('sys-utils')
subdir('disk-utils')
subdir('misc-utils')
subdir('text-utils')
subdir('term-utils')
subdir('po')

includes = [dir_include,
            dir_libsmartcols,
            dir_libfdisk,
            dir_libuuid,
            dir_liblastlog2,
            dir_sys_utils]

exes = []

opt = not get_option('build-chfn-chsh').disabled()
exe = executable(
  'chfn',
  chfn_sources,
  chfn_chsh_sources,
  include_directories : includes,
  link_with : [lib_common, logindefs_c],
  dependencies : chfn_chsh_deps,
  install_dir : usrbin_exec_dir,
  install_mode : 'rwsr-xr-x',
  install : opt,
  build_by_default : opt)

exe2 = executable(
  'chsh',
  'login-utils/chsh.c',
  'lib/shells.c',
  chfn_chsh_sources,
  include_directories : includes,
  link_with : lib_common,
  dependencies : chfn_chsh_deps,
  install_dir : usrbin_exec_dir,
  install_mode : 'rwsr-xr-x',
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += [exe, exe2]
  manadocs += ['login-utils/chfn.1.adoc', 'login-utils/chsh.1.adoc']
  bashcompletions += ['chfn', 'chsh']
endif

exe = executable(
  'test_islocal',
  test_islocal_sources,
  include_directories : includes,
  c_args : '-DTEST_PROGRAM',
  build_by_default : program_tests)
exes += exe

exe = executable(
  'test-consoles',
  test_consoles_sources,
  c_args : ['-DTEST_PROGRAM'],
  include_directories : includes,
  link_with : lib_common,
  build_by_default : program_tests)
exes += exe

opt = not get_option('build-last').disabled()
exe = executable(
  'last',
  last_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  meson.add_install_script(meson_make_symlink,
                           'last',
                           join_paths(usrbin_exec_dir, 'lastb'))
  manadocs += ['login-utils/last.1.adoc']
  manlinks += {'lastb.1': 'last.1'}
  bashcompletions += ['last']
  bashcompletionslinks += {'lastb': 'last'}
endif

opt = not get_option('build-nologin').disabled()
exe = executable(
  'nologin',
  'login-utils/nologin.c',
  include_directories : includes,
  install_dir : sbindir,
  link_with : [lib_common],
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['login-utils/nologin.8.adoc']
endif

opt = not get_option('build-utmpdump').disabled()
exe = executable(
  'utmpdump',
  'login-utils/utmpdump.c',
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['login-utils/utmpdump.1.adoc']
  bashcompletions += ['utmpdump']
endif

opt = get_option('build-su').require(have_pty).allowed()
exe = executable(
  'su',
  'login-utils/su.c',
  'login-utils/su-common.c',
  'login-utils/su-common.h',
  'lib/shells.c',
  pty_session_c,
  monotonic_c,
  include_directories : includes,
  link_with : [lib_common, logindefs_c],
  dependencies : [lib_pam,
                  lib_pam_misc,
                  lib_util,
                  realtime_libs],
  install_mode : 'rwsr-xr-x',
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['login-utils/su.1.adoc']
  bashcompletions += ['su']
endif

opt = not get_option('build-newgrp').disabled()
exe = executable(
  'newgrp',
  'login-utils/newgrp.c',
  include_directories : includes,
  dependencies : [lib_crypt],
  install_dir : usrbin_exec_dir,
  install_mode : 'rwsr-xr-x',
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['login-utils/newgrp.1.adoc']
  bashcompletions += ['newgrp']
endif

opt = not get_option('build-lslogins').disabled()
exe = executable(
  'lslogins',
  'login-utils/lslogins.c',
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols,
               logindefs_c],
  dependencies : [lib_selinux,
                  lib_systemd],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['login-utils/lslogins.1.adoc']
  bashcompletions += ['lslogins']
endif

opt = not get_option('build-vipw').disabled()
exe = executable(
  'vipw',
  'login-utils/vipw.c',
  'login-utils/setpwnam.h',
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [lib_selinux],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  meson.add_install_script(meson_make_symlink,
                           'vipw',
                           join_paths(usrbin_exec_dir, 'vigr'))
  manadocs += ['login-utils/vipw.8.adoc']
  meson.add_install_script(meson_make_symlink,
                           'vipw.8',
                           join_paths(mandir, 'man8/vigr.8'))
endif

opt = get_option('build-runuser').require(have_pty).allowed()
exe = executable(
  'runuser',
  'login-utils/runuser.c',
  'login-utils/su-common.c',
  'login-utils/su-common.h',
  'lib/shells.c',
  pty_session_c,
  monotonic_c,
  include_directories : includes,
  link_with : [lib_common, logindefs_c],
  dependencies : [lib_pam,
                  lib_pam_misc,
                  lib_util,
                  realtime_libs],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['login-utils/runuser.1.adoc']
  bashcompletionslinks += {'runuser': 'su'}
endif

############################################################

if is_glibc
  exe = executable(
    'col',
    col_sources,
    include_directories : includes,
    link_with : lib_common,
    install_dir : usrbin_exec_dir,
    install : true)
  exes += exe
  manadocs += ['text-utils/col.1.adoc']
  bashcompletions += ['col']
endif

exe = executable(
  'colcrt',
  colcrt_sources,
  include_directories : includes,
  install_dir : usrbin_exec_dir,
  install : true)
exes += exe
manadocs += ['text-utils/colcrt.1.adoc']
bashcompletions += ['colcrt']

exe = executable(
  'colrm',
  colrm_sources,
  include_directories : includes,
  link_with : lib_common,
  install_dir : usrbin_exec_dir,
  install : true)
exes += exe
manadocs += ['text-utils/colrm.1.adoc']
bashcompletions += ['colrm']

exe = executable(
  'rev',
  rev_sources,
  include_directories : includes,
  install_dir : usrbin_exec_dir,
  install : true)
exes += exe
manadocs += ['text-utils/rev.1.adoc']
bashcompletions += ['rev']

exe = executable(
  'column',
  column_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  install_dir : usrbin_exec_dir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['text-utils/column.1.adoc']
  bashcompletions += ['column']
endif

opt = not get_option('build-line').disabled()
exe = executable(
  'line',
  line_sources,
  include_directories : includes,
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['text-utils/line.1.adoc']
endif

opt = not get_option('build-pg').disabled()
exe = executable(
  'pg',
  pg_sources,
  link_with : lib_common,
  include_directories : includes,
  dependencies : [lib_tinfo,
                  curses_libs],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['text-utils/pg.1.adoc']
  bashcompletions += ['pg']
endif

opt = not get_option('build-ul').disabled()
exe = executable(
  'ul',
  ul_sources,
  include_directories : includes,
  dependencies : [lib_tinfo,
                  curses_libs],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['text-utils/ul.1.adoc']
  bashcompletions += ['ul']
endif

opt = not get_option('build-more').disabled()
exe = executable(
  'more',
  more_sources,
  link_with : [lib_common],
  include_directories : includes,
  dependencies : [lib_tinfo,
                  curses_libs,
		  lib_magic],
  install : opt,
  build_by_default : opt)
exe2 = executable(
  'test_more',
  more_sources,
  link_with : [lib_common],
  include_directories : includes,
  c_args : '-DTEST_PROGRAM',
  dependencies : [lib_tinfo,
                  curses_libs,
		  lib_magic],
  build_by_default : opt and program_tests)
exes += exe
if opt and not is_disabler(exe)
  exes += [exe, exe2]
  manadocs += ['text-utils/more.1.adoc']
  bashcompletions += ['more']
endif

exe = executable(
  'hexdump',
  hexdump_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_tcolors],
  install_dir : usrbin_exec_dir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['text-utils/hexdump.1.adoc']
  bashcompletions += ['hexdump']
endif

opt = not get_option('build-lsmem').disabled()
exe = executable(
  'lsmem',
  lsmem_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/lsmem.1.adoc']
  bashcompletions += ['lsmem']
endif

opt = not get_option('build-chmem').disabled()
exe = executable(
  'chmem',
  chmem_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/chmem.8.adoc']
  bashcompletions += ['chmem']
endif

exe = executable(
  'choom',
  choom_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : true)
exes += exe
manadocs += ['sys-utils/choom.1.adoc']

has_seminfo_type = cc.has_type('struct seminfo', args : '-D_GNU_SOURCE', prefix : '#include <sys/sem.h>')

opt = get_option('build-ipcmk').require(has_seminfo_type).allowed()
exe = executable(
  'ipcmk',
  ipcmk_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/ipcmk.1.adoc']
  bashcompletions += ['ipcmk']
endif

opt = get_option('build-ipcrm').require(has_seminfo_type).allowed()
exe = executable(
  'ipcrm',
  ipcrm_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/ipcrm.1.adoc']
  bashcompletions += ['ipcrm']
endif

opt = not get_option('build-ipcs').require(has_seminfo_type).disabled()
exe = executable(
  'ipcs',
  ipcs_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/ipcs.1.adoc']
  bashcompletions += ['ipcs']
endif

opt = not get_option('build-rfkill').disabled()
exe = executable(
  'rfkill',
  rfkill_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  install_dir : usrsbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/rfkill.8.adoc']
  bashcompletions += ['rfkill']
endif

exe = executable(
  'renice',
  renice_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  install_dir : usrbin_exec_dir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/renice.1.adoc']
  bashcompletions += ['renice']
endif

exe = executable(
  'setpgid',
  setpgid_sources,
  include_directories: includes,
  link_with : [lib_common,
               lib_smartcols],
  install_dir : usrbin_exec_dir,
  install : true)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/setpgid.1.adoc']
  bashcompletions += ['setpgid']
endif

exe = executable(
  'setsid',
  setsid_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  install_dir : usrbin_exec_dir,
  install : true)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/setsid.1.adoc']
  bashcompletions += ['setsid']
endif

exe = executable(
  'readprofile',
  readprofile_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  install_dir : usrsbin_exec_dir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/readprofile.8.adoc']
  bashcompletions += ['readprofile']
endif

opt = not get_option('build-tunelp').disabled()
exe = executable(
  'tunelp',
  tunelp_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrsbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/tunelp.8.adoc']
  bashcompletions += ['tunelp']
endif

opt = not get_option('build-fstrim').disabled()
exe = executable(
  'fstrim',
  fstrim_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [mount_dep],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/fstrim.8.adoc']
  bashcompletions += ['fstrim']
endif

opt = get_option('build-dmesg').require(cc.has_header('sys/klog.h')).allowed()
exe = executable(
  'dmesg',
  dmesg_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_tcolors],
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/dmesg.1.adoc']
  bashcompletions += ['dmesg']
endif

exe = executable(
  'test_dmesg',
  dmesg_sources,
  include_directories : dir_include,
  c_args : '-DTEST_DMESG',
  link_with : [lib_common,
               lib_tcolors],
  build_by_default : program_tests)
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'ctrlaltdel',
  ctrlaltdel_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : sbindir,
  install : true)
exes += exe
manadocs += ['sys-utils/ctrlaltdel.8.adoc']
bashcompletions += ['ctrlaltdel']

have_linux_fs_h = conf.get('HAVE_LINUX_FS_H').to_string() == '1'

opt = get_option('build-fsfreeze').require(have_linux_fs_h).allowed()
exe = executable(
  'fsfreeze',
  fsfreeze_sources,
  include_directories : includes,
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/fsfreeze.8.adoc']
  bashcompletions += ['fsfreeze']
endif

opt = get_option('build-blkdiscard').require(have_linux_fs_h).allowed()
exe = executable(
  'blkdiscard',
  blkdiscard_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [blkid_dep],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/blkdiscard.8.adoc']
  bashcompletions += ['blkdiscard']
endif

opt = get_option('build-blkzone').require(have_linux_fs_h).allowed()
exe = executable(
  'blkzone',
  blkzone_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/blkzone.8.adoc']
  bashcompletions += ['blkzone']
endif

opt = get_option('build-blkpr').require(cc.has_header('linux/pr.h')).allowed()
exe = executable(
  'blkpr',
  blkpr_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/blkpr.8.adoc']
endif

opt = get_option('build-ldattach').require(cc.has_header('linux/if.h')).allowed()
exe = executable(
  'ldattach',
  ldattach_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrsbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/ldattach.8.adoc']
  bashcompletions += ['ldattach']
endif

have_linux_rtc_h = cc.has_header('linux/rtc.h')

opt = get_option('build-rtcwake').require(have_linux_rtc_h).allowed()
exe = executable(
  'rtcwake',
  rtcwake_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrsbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/rtcwake.8.adoc']
  bashcompletions += ['rtcwake']
endif

opt = get_option('build-setarch').require(cc.has_header('sys/personality.h')).allowed()
exe = executable(
  'setarch',
  setarch_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/setarch.8.adoc']
  bashcompletions += ['setarch']
endif

if opt
  setarch_links = ['uname26', 'linux32', 'linux64']
  setarch_links_arch = {
    's390x' :   ['s390', 's390x'],
    'x86' :     ['i386'],
    'x86_64' :  ['i386', 'x86_64'],
    'ppc64' :   ['ppc', 'ppc64', 'ppc32'],
    'space64' : ['sparc', 'sparc64', 'sparc32', 'sparc32bash'],
    'mips64' :  ['mips', 'mips64', 'mips32'],
    'ia64' :    ['i386', 'ia64'],
    'hppa' :    ['parisc', 'parisc64', 'parisc32'],
  }
  setarch_links += setarch_links_arch.get(host_machine.cpu_family(), [])

  foreach link: setarch_links
    meson.add_install_script(meson_make_symlink,
                            'setarch',
                            join_paths(usrbin_exec_dir, link))
    manlinks += {link + '.8': 'setarch.8'}
  endforeach
endif

opt = not get_option('build-eject').disabled()
exe = executable(
  'eject',
  eject_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [mount_dep],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exe = exe
  manadocs += ['sys-utils/eject.1.adoc']
  bashcompletions += ['eject']
endif

opt = not get_option('build-losetup').disabled()
exe = executable(
  'losetup',
  losetup_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  manadocs += ['sys-utils/losetup.8.adoc']
  exes += exe
  bashcompletions += ['losetup']
endif

opt = opt and 'losetup' in static_programs
exe = executable(
  'losetup.static',
  losetup_sources,
  include_directories : includes,
  link_args : ['--static'],
  link_with : [lib_common,
               lib_smartcols.get_static_lib()],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
endif

opt = not get_option('build-zramctl').disabled()
exe = executable(
  'zramctl',
  zramctl_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/zramctl.8.adoc']
  bashcompletions += ['zramctl']
endif

exe = executable(
  'prlimit',
  prlimit_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  install_dir : usrbin_exec_dir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/prlimit.1.adoc']
  bashcompletions += ['prlimit']
endif

opt = not get_option('build-lsns').disabled()
exe = executable(
  'lsns',
  lsns_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  dependencies : [mount_dep],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/lsns.8.adoc']
  bashcompletions += ['lsns']
endif

opt = not get_option('build-mount').disabled()
exe = executable(
  'mount',
  mount_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  dependencies : [lib_selinux, mount_dep],
  install_mode : 'rwsr-xr-x',
  install : opt,
  build_by_default : opt)
exe2 = executable(
  'umount',
  umount_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [mount_dep],
  install_mode : 'rwsr-xr-x',
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += [exe, exe2]
  manadocs += ['sys-utils/fstab.5.adoc',
		    'sys-utils/mount.8.adoc',
	            'sys-utils/umount.8.adoc']
  bashcompletions += ['mount', 'umount']
endif

opt2 = opt and 'mount' in static_programs
exe = executable(
  'mount.static',
  mount_sources,
  include_directories : includes,
  link_args : ['--static'],
  link_with : [lib_common,
               lib_smartcols_static],
  dependencies : [mount_static_dep],
  install : opt2,
  build_by_default : opt2)
if opt2 and not is_disabler(exe)
  exes += exe
endif

opt2 = opt and 'umount' in static_programs
exe = executable(
  'umount.static',
  umount_sources,
  include_directories : includes,
  link_args : ['--static'],
  link_with : [lib_common],
  dependencies : [mount_static_dep],
  install : opt2,
  build_by_default : opt2)
if opt2 and not is_disabler(exe)
  exes += exe
endif

# setuid?

opt = not get_option('build-swapon').disabled()
exe = executable(
  'swapon',
  swapon_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  dependencies : [blkid_dep, mount_dep],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/swapon.8.adoc']
  bashcompletions += ['swapon']
endif

opt = not get_option('build-swapoff').disabled()
exe = executable(
  'swapoff',
  swapoff_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [blkid_dep, mount_dep],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manlinks += {'swapoff.8': 'swapon.8'}
  bashcompletions += ['swapoff']
endif

exe = executable(
  'lscpu',
  lscpu_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  dependencies : [rtas_libs],
  install_dir : usrbin_exec_dir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/lscpu.1.adoc']
  bashcompletions += ['lscpu']
endif

opt = get_option('build-chcpu').require(have_cpu_set_t).allowed()
exe = executable(
  'chcpu',
  chcpu_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/chcpu.8.adoc']
  bashcompletions += ['chcpu']
endif

exe = executable(
  'wdctl',
  wdctl_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/wdctl.8.adoc']
  bashcompletions += ['wdctl']
endif

opt = not get_option('build-mountpoint').disabled()
exe = executable(
  'mountpoint',
  mountpoint_sources,
  include_directories : includes,
  dependencies : [mount_dep],
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/mountpoint.1.adoc']
  bashcompletions += ['mountpoint']
endif

opt = not get_option('build-fallocate').disabled()
exe = executable(
  'fallocate',
  fallocate_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/fallocate.1.adoc']
  bashcompletions += ['fallocate']
endif

opt = not get_option('build-pivot_root').disabled()
exe = executable(
  'pivot_root',
  pivot_root_sources,
  include_directories : includes,
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/pivot_root.8.adoc']
  bashcompletions += ['pivot_root']
endif

opt = not get_option('build-switch_root').disabled()
if opt and  not have_dirfd and not have_ddfd
  error('neither dirfd nor ddfd are available')
endif
exe = executable(
  'switch_root',
  switch_root_sources,
  include_directories : includes,
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/switch_root.8.adoc']
endif

opt = not get_option('build-unshare').disabled()
exe = executable(
  'unshare',
  unshare_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [mount_dep],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/unshare.1.adoc']
  bashcompletions += ['unshare']
endif

opt = opt and 'unshare' in static_programs
exe = executable(
  'unshare.static',
  unshare_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [mount_dep],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
endif

opt = not get_option('build-nsenter').disabled()
exe = executable(
  'nsenter',
  nsenter_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [lib_selinux],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/nsenter.1.adoc']
  bashcompletions += ['nsenter']
endif

opt = opt and 'nsenter' in static_programs
exe = executable(
  'nsenter.static',
  nsenter_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [lib_selinux],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
endif

opt = not get_option('build-setpriv').disabled() and lib_cap_ng.found()
exe = executable(
  'setpriv',
  setpriv_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [lib_cap_ng],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/setpriv.1.adoc']
  bashcompletions += ['setpriv']
endif

exe = executable(
  'flock',
  flock_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : realtime_libs,
  install_dir : usrbin_exec_dir,
  install : true)
exes += exe
manadocs += ['sys-utils/flock.1.adoc']
bashcompletions += ['flock']

opt = not get_option('build-lsirq').disabled()
exe = executable(
  'lsirq',
  lsirq_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/lsirq.1.adoc']
  bashcompletions += ['lsirq']
endif

opt = not get_option('build-irqtop').disabled()
exe = executable(
  'irqtop',
  irqtop_sources,
  include_directories : includes,
  dependencies : [realtime_libs, curses_libs],
  link_with : [lib_common,
               lib_smartcols,
	       lib_tcolors],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/irqtop.1.adoc']
  bashcompletions += ['irqtop']
endif

opt = not get_option('build-ipcs').disabled()
exe = executable(
  'lsipc',
  lsipc_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/lsipc.1.adoc']
  bashcompletions += ['lsipc']
endif

opt = build_hwclock
exe = executable(
  'hwclock',
  hwclock_sources,
  include_directories : includes,
  link_with :    [lib_common],
  dependencies : [lib_m,
                  lib_audit],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['sys-utils/hwclock.8.adoc', 'sys-utils/adjtime_config.5.adoc']
  bashcompletions += ['hwclock']
endif

exe = executable(
  'mkfs',
  mkfs_sources,
  include_directories : includes,
  install_dir : sbindir,
  install : true)
exes += exe
manadocs += ['disk-utils/mkfs.8.adoc']
bashcompletions += ['mkfs']

opt = not get_option('build-bfs').disabled()
exe = executable(
  'mkfs.bfs',
  mkfs_bfs_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['disk-utils/mkfs.bfs.8.adoc']
  bashcompletions += ['mkfs.bfs']
endif

exe = executable(
  'isosize',
  isosize_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : true)
exes += exe
manadocs += ['disk-utils/isosize.8.adoc']
bashcompletions += ['isosize']

exe = executable(
  'mkswap',
  mkswap_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_uuid],
  dependencies: [blkid_dep, lib_selinux],
  install_dir : sbindir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['disk-utils/mkswap.8.adoc']
  bashcompletions += ['mkswap']
endif

exe = executable(
  'swaplabel',
  swaplabel_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_uuid],
  dependencies : [blkid_dep],
  install_dir : sbindir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['disk-utils/swaplabel.8.adoc']
  bashcompletions += ['swaplabel']
endif

opt = not get_option('build-fsck').disabled()
exe = executable(
  'fsck',
  fsck_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [blkid_dep, mount_dep, realtime_libs],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['disk-utils/fsck.8.adoc']
  bashcompletions += ['fsck']
endif

opt = not get_option('build-minix').disabled()
exe = executable(
  'mkfs.minix',
  mkfs_minix_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
exe2 = executable(
  'test_mkfs_minix',
  mkfs_minix_sources,
  include_directories : includes,
  c_args : '-DTEST_SCRIPT',
  link_with : [lib_common],
  build_by_default : opt and program_tests)
exe3 = executable(
  'fsck.minix',
  fsck_minix_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += [exe, exe2, exe3]
  manadocs += ['disk-utils/mkfs.minix.8.adoc','disk-utils/fsck.minix.8.adoc']
  bashcompletions += ['mkfs.minix', 'fsck.minix']
endif

opt = not get_option('build-cramfs').disabled()
exe = executable(
  'mkfs.cramfs',
  mkfs_cramfs_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [lib_z],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
exe2 = executable(
  'fsck.cramfs',
  fsck_cramfs_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [lib_z],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if not is_disabler(exe)
  exes += [exe, exe2]
  manadocs += ['disk-utils/mkfs.cramfs.8.adoc','disk-utils/fsck.cramfs.8.adoc']
  bashcompletions += ['mkfs.cramfs', 'fsck.cramfs']
endif

opt = not get_option('build-raw').disabled()
if opt
  cc.has_header('linux/raw.h', required: opt)
endif
exe = executable(
  'raw',
  raw_sources,
  include_directories : includes,
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['disk-utils/raw.8.adoc']
  bashcompletions += ['raw']
endif

opt = not get_option('build-fdformat').disabled()
exe = executable(
  'fdformat',
  fdformat_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrsbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['disk-utils/fdformat.8.adoc']
  bashcompletions += ['fdformat']
endif

opt = get_option('build-blockdev').require(LINUX).allowed()
exe = executable(
  'blockdev',
  blockdev_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['disk-utils/blockdev.8.adoc']
  bashcompletions += ['blockdev']
endif

opt = not get_option('build-fdisks').disabled()
if opt and not have_dirfd and not have_ddfd
  error('neither dirfd nor ddfd are available')
endif
exe = executable(
  'fdisk',
  fdisk_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_fdisk,
               lib_smartcols,
               lib_tcolors],
  dependencies : [lib_readline],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  bashcompletions += ['fdisk']
endif

opt2 = opt and 'fdisk' in static_programs
exe = executable(
  'fdisk.static',
  fdisk_sources,
  link_args : ['--static'],
  include_directories : includes,
  link_with : [lib_common,
               lib_tcolors,
               lib_fdisk_static,
               lib_smartcols.get_static_lib()],
  dependencies : [lib_readline_static],
  install_dir : sbindir,
  install : opt2,
  build_by_default : opt2)
if opt2 and not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'sfdisk',
  sfdisk_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_fdisk,
               lib_smartcols,
               lib_tcolors],
  dependencies : [lib_readline],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  bashcompletions += ['sfdisk']
endif

opt2 = opt and 'sfdisk' in static_programs
exe = executable(
  'sfdisk.static',
  sfdisk_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_tcolors,
               lib_fdisk_static,
               lib_smartcols.get_static_lib()],
  dependencies : [lib_readline_static],
  install_dir : sbindir,
  install : opt2,
  build_by_default : opt2)
if opt2 and not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'cfdisk',
  cfdisk_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_fdisk,
               lib_smartcols,
               lib_tcolors],
  dependencies : [curses_libs, mount_dep],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['disk-utils/fdisk.8.adoc',
		    'disk-utils/sfdisk.8.adoc',
		    'disk-utils/cfdisk.8.adoc']
  bashcompletions += ['cfdisk']
endif

opt = not get_option('build-partx').disabled()
exe = executable(
  'addpart',
  addpart_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrsbin_exec_dir,
  install : opt,
  build_by_default : opt)
exe2 = executable(
  'delpart',
  delpart_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrsbin_exec_dir,
  install : opt,
  build_by_default : opt)
exe3 = executable(
  'resizepart',
  resizepart_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrsbin_exec_dir,
  install : opt,
  build_by_default : opt)
exe4 = executable(
  'partx',
  partx_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  dependencies : [blkid_dep],
  install_dir : usrsbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt
  exes += [exe, exe2, exe3, exe4]
  manadocs += ['disk-utils/addpart.8.adoc',
		    'disk-utils/delpart.8.adoc',
		    'disk-utils/resizepart.8.adoc',
		    'disk-utils/partx.8.adoc']
  bashcompletions += ['addpart', 'delpart', 'resizepart', 'partx']
endif

############################################################

opt = get_option('build-script').require(have_pty).allowed()
exe = executable(
  'script',
  script_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [lib_util,
                  lib_utempter,
                  realtime_libs,
                  math_libs],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['term-utils/script.1.adoc']
  bashcompletions += ['script']
endif

exe = executable(
  'test_script',
  script_sources,
  include_directories : includes,
  c_args : '-DTEST_SCRIPT',
  link_with : [lib_common],
  dependencies : [lib_util,
                  lib_utempter,
                  realtime_libs,
                  math_libs],
  build_by_default : opt and program_tests)
if opt and not is_disabler(exe)
  exes += exe
endif

opt = get_option('build-scriptlive').require(have_pty).allowed()
exe = executable(
  'scriptlive',
  scriptlive_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [lib_util,
                  realtime_libs,
                  math_libs],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['term-utils/scriptlive.1.adoc']
  bashcompletions += ['scriptlive']
endif

exe = executable(
  'scriptreplay',
  scriptreplay_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [math_libs],
  install_dir : usrbin_exec_dir,
  install : true)
exes += exe
manadocs += ['term-utils/scriptreplay.1.adoc']
bashcompletions += ['scriptreplay']

opt = not get_option('build-agetty').disabled()
exe = executable(
  'agetty',
  agetty_sources,
  include_directories : includes,
  link_with : [lib_common, logindefs_c],
  dependencies : [BSD ? lib_util : [], lib_systemd],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt
  exes += exe
  manadocs += ['term-utils/agetty.8.adoc']
endif

opt = not get_option('build-setterm').disabled()
exe = executable(
  'setterm',
  setterm_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [curses_libs],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt
  exes += exe
  manadocs += ['term-utils/setterm.1.adoc']
  bashcompletions += ['setterm']
endif

opt = not get_option('build-mesg').disabled()
exe = executable(
  'mesg',
  mesg_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt
  exes += exe
  manadocs += ['term-utils/mesg.1.adoc']
  bashcompletions += ['mesg']
endif

opt = not get_option('build-wall').disabled()
exe = executable(
  'wall',
  wall_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [lib_systemd],
  install_dir : usrbin_exec_dir,
  install_mode : [ 'rwxr-sr-x', 'root', 'tty' ],
  install : opt,
  build_by_default : opt)
if opt
  exes += exe
  manadocs += ['term-utils/wall.1.adoc']
  bashcompletions += ['wall']
endif

# chgrp tty $(DESTDIR)$(usrbin_execdir)/wall
# chmod g+s $(DESTDIR)$(usrbin_execdir)/wall

opt = not get_option('build-write').disabled()
exe = executable(
  'write',
  write_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [lib_systemd],
  install_dir : usrbin_exec_dir,
  install_mode : [ 'rwxr-sr-x', 'root', 'tty' ],
  install : opt,
  build_by_default : opt)
if opt
  exes += exe
  manadocs += ['term-utils/write.1.adoc']
  bashcompletions += ['write']
endif

# chgrp tty $(DESTDIR)$(usrbin_execdir)/write
# chmod g+s $(DESTDIR)$(usrbin_execdir)/write

opt = not get_option('build-login').disabled()
exe = executable(
  'login',
  login_sources,
  include_directories : includes,
  link_with : [lib_common, logindefs_c],
  dependencies : [lib_pam,
                  lib_audit,
                  lib_selinux],
  install : opt,
  build_by_default : opt)
if not is_disabler(exe)
  exes += exe
  manadocs += ['login-utils/login.1.adoc']
endif

opt = not get_option('build-sulogin').disabled()
exe = executable(
  'sulogin',
  sulogin_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [lib_crypt,
                  lib_selinux],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if not is_disabler(exe)
  exes += exe
  manadocs += ['login-utils/sulogin.8.adoc']
endif

exe = executable(
  'cal',
  cal_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_tcolors],
  dependencies : [curses_libs],
  install_dir : usrbin_exec_dir,
  install : true)
exes += exe
manadocs += ['misc-utils/cal.1.adoc']
bashcompletions += ['cal']

opt = not get_option('build-logger').disabled()
exe = executable(
  'logger',
  logger_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [lib_systemd],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/logger.1.adoc']
  bashcompletions += ['logger']
endif

exe = executable(
  'test_logger',
  logger_sources,
  include_directories : includes,
  c_args : '-DTEST_LOGGER',
  link_with : [lib_common],
  dependencies : [lib_systemd],
  build_by_default: program_tests)
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'look',
  look_sources,
  include_directories : includes,
  install_dir : usrbin_exec_dir,
  install : true)
exes += exe
manadocs += ['misc-utils/look.1.adoc']
bashcompletions += ['look']

exe = executable(
  'mcookie',
  mcookie_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : true)
exes += exe
manadocs += ['misc-utils/mcookie.1.adoc']
bashcompletions += ['mcookie']

if build_liblastlog2
  exe = executable(
    'lastlog2',
    lastlog2_sources,
    include_directories : includes,
    link_with : [lib_common, lib_lastlog2],
    install_dir : usrbin_exec_dir,
    install : true)
  exes += exe
  manadocs += ['misc-utils/lastlog2.8.adoc']
  bashcompletions += ['lastlog2']
  manadocs += ['liblastlog2/man/lastlog2.3.adoc',
               'liblastlog2/man/ll2_write_entry.3.adoc',
	       'liblastlog2/man/ll2_read_entry.3.adoc',
	       'liblastlog2/man/ll2_import_lastlog.3.adoc',
	       'liblastlog2/man/ll2_read_all.3.adoc',
	       'liblastlog2/man/ll2_remove_entry.3.adoc',
	       'liblastlog2/man/ll2_rename_user.3.adoc',
	       'liblastlog2/man/ll2_update_login_time.3.adoc'
	       ]
endif

exe = executable(
  'namei',
  namei_sources,
  include_directories : includes,
  dependencies : [lib_selinux],
  install_dir : usrbin_exec_dir,
  install : true)
exes += exe
manadocs += ['misc-utils/namei.1.adoc']
bashcompletions += ['namei']

exe = executable(
  'whereis',
  whereis_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : true)
exes += exe
manadocs += ['misc-utils/whereis.1.adoc']
bashcompletions += ['whereis']

opt = not get_option('build-lslocks').disabled()
exe = executable(
  'lslocks',
  lslocks_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  dependencies : [mount_dep],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/lslocks.8.adoc']
  bashcompletions += ['lslocks']
endif

opt = not get_option('build-lsblk').disabled()
exe = executable(
  'lsblk',
  lsblk_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_tcolors,
               lib_smartcols],
  dependencies : [blkid_dep, lib_udev, mount_dep],
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/lsblk.8.adoc']
  bashcompletions += ['lsblk']
endif

opt = not get_option('build-lsfd').require(lib_rt.found()).disabled()
exe = executable(
  'lsfd',
  lsfd_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  dependencies : [lib_rt],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/lsfd.1.adoc']
endif

exe = executable(
  'uuidgen',
  uuidgen_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_uuid],
  install_dir : usrbin_exec_dir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/uuidgen.1.adoc']
  bashcompletions += ['uuidgen']
endif

exe = executable(
  'uuidparse',
  uuidparse_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_uuid,
               lib_smartcols],
  install_dir : usrbin_exec_dir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/uuidparse.1.adoc']
  bashcompletions += ['uuidparse']
endif

opt = build_uuidd
exe = executable(
  'uuidd',
  uuidd_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_uuid],
  dependencies : [realtime_libs,
                  lib_systemd],
  install_dir : usrsbin_exec_dir,
  install : opt,
  build_by_default : opt)
exe2 = executable(
  'test_uuidd',
  test_uuidd_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_uuid],
  dependencies : thread_libs,
  build_by_default : opt and program_tests)
if not is_disabler(exe)
  exes += [exe, exe2]
  manadocs += ['misc-utils/uuidd.8.adoc']
  bashcompletions += ['uuidd']
endif

opt = build_libblkid
exe = executable(
  'blkid',
  blkid_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [blkid_dep],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/blkid.8.adoc']
  bashcompletions += ['blkid']
endif

opt = opt and 'blkid' in static_programs
exe = executable(
  'blkid.static',
  blkid_sources,
  include_directories : includes,
  link_with : [lib_common],
  dependencies : [blkid_static_dep],
  install_dir : sbindir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'sample-mkfs',
  'libblkid/samples/mkfs.c',
  include_directories : includes,
  dependencies : [blkid_dep])
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'sample-partitions',
  'libblkid/samples/partitions.c',
  include_directories : includes,
  dependencies : [blkid_dep])
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'sample-superblocks',
  'libblkid/samples/superblocks.c',
  include_directories : includes,
  dependencies : [blkid_dep])
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'sample-topology',
  'libblkid/samples/topology.c',
  include_directories : includes,
  dependencies : [blkid_dep])
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'test_blkid_fuzz_sample',
  'libblkid/src/fuzz.c',
  include_directories: includes,
  dependencies : [blkid_dep],
  build_by_default: program_tests)
if not is_disabler(exe)
  exes += exe
endif

############################################################

exe = executable(
  'findfs',
  findfs_sources,
  include_directories : includes,
  dependencies : [blkid_dep],
  install_dir : sbindir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/findfs.8.adoc']
  bashcompletions += ['findfs']
endif

exe = executable(
  'wipefs',
  wipefs_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  dependencies : [blkid_dep],
  install_dir : sbindir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/wipefs.8.adoc']
  bashcompletions += ['wipefs']
endif

opt = not get_option('build-findmnt').disabled()
exe = executable(
  'findmnt',
  findmnt_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  dependencies : [blkid_dep, lib_udev, mount_dep],
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/findmnt.8.adoc']
  bashcompletions += ['findmnt']
endif

exe = executable(
  'kill',
  kill_sources,
  include_directories : includes,
  link_with : [lib_common],
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/kill.1.adoc']
endif

opt = not get_option('build-rename').disabled()
exe = executable(
  'rename',
  rename_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/rename.1.adoc']
  bashcompletions += ['rename']
endif

exe = executable(
  'getopt',
  getopt_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : true)
exes += exe
manadocs += ['misc-utils/getopt.1.adoc']
bashcompletions += ['getopt']

exe = executable(
  'fincore',
  fincore_sources,
  include_directories : includes,
  link_with : [lib_common,
               lib_smartcols],
  install_dir : usrbin_exec_dir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/fincore.1.adoc']
  bashcompletions += ['fincore']
endif

exe = executable(
  'hardlink',
  hardlink_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/hardlink.1.adoc']
  bashcompletions += ['hardlink']
endif

opt = get_option('build-pipesz').allowed()
exe = executable(
  'pipesz',
  pipesz_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/pipesz.1.adoc']
  bashcompletions += ['pipesz']
endif

exe = executable(
  'test_cal',
  cal_sources,
  include_directories : includes,
  c_args : '-DTEST_CAL',
  link_with : [lib_common,
               lib_tcolors],
  dependencies : [curses_libs],
  build_by_default: program_tests)
if not is_disabler(exe)
  exes += exe
endif

have_posix_fadvise = conf.get('HAVE_POSIX_FADVISE').to_string() == '1'

opt = get_option('build-fadvise').require(have_posix_fadvise).allowed()
exe = executable(
  'fadvise',
  fadvise_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/fadvise.1.adoc']
  bashcompletions += ['fadvise']
endif

if LINUX and conf.get('HAVE_PIDFD_OPEN').to_string() == '1'
  exe = executable(
    'waitpid',
    waitpid_sources,
    include_directories : includes,
    link_with : [lib_common],
    install_dir : usrbin_exec_dir,
    install : true)
  if not is_disabler(exe)
    exes += exe
    manadocs += ['misc-utils/waitpid.1.adoc']
    bashcompletions += ['waitpid']
  endif
endif

syscalls_h = custom_target('syscalls.h',
  input : 'tools/all_syscalls',
  output : 'syscalls.h',
  command : ['tools/all_syscalls', sed.full_path(),
             cc.cmd_array(), get_option('c_args')],
)

have_linux_audit_h = cc.has_header('linux/audit.h')
have_audit_arch_native = cc.compiles(fs.read('include/audit-arch.h'), name : 'has AUDIT_ARCH_NATIVE')

opt = get_option('build-enosys').require(have_linux_audit_h and have_audit_arch_native).allowed()
exe = executable(
  'enosys',
  'misc-utils/enosys.c', syscalls_h,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/enosys.1.adoc']
  bashcompletions += ['enosys']
endif

opt = get_option('build-lsclocks').require(have_linux_rtc_h).allowed()
exe = executable(
  'lsclocks',
  lsclocks_sources,
  include_directories : includes,
  link_with : [lib_common, lib_smartcols],
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)
if opt and not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/lsclocks.1.adoc']
  bashcompletions += ['lsclocks']
endif

if conf.get('HAVE_RENAMEAT2').to_string() == '1'
exe = executable(
  'exch',
  exch_sources,
  include_directories : includes,
  link_with : [lib_common],
  install_dir : usrbin_exec_dir,
  install : true)
if not is_disabler(exe)
  exes += exe
  manadocs += ['misc-utils/exch.1.adoc']
  bashcompletions += ['exch']
endif
endif

############################################################

opt = not get_option('build-schedutils').disabled()
exe = executable(
  'chrt',
  'schedutils/chrt.c',
  include_directories : includes,
  link_with : lib_common,
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)

exe2 = executable(
  'ionice',
  'schedutils/ionice.c',
  include_directories : includes,
  link_with : lib_common,
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)

exe3 = executable(
  'taskset',
  'schedutils/taskset.c',
  include_directories : includes,
  link_with : lib_common,
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)

exe4 = executable(
  'uclampset',
  'schedutils/uclampset.c',
  include_directories : includes,
  link_with : lib_common,
  install_dir : usrbin_exec_dir,
  install : opt,
  build_by_default : opt)

if opt and not is_disabler(exe)
  exes += [exe, exe2, exe3, exe4]
  manadocs += ['schedutils/chrt.1.adoc',
               'schedutils/ionice.1.adoc',
               'schedutils/taskset.1.adoc',
	       'schedutils/uclampset.1.adoc']
  bashcompletions += ['chrt', 'ionice', 'taskset', 'uclampset']
endif

############################################################

# TODO: when autotools compat is not needed, s/_/-/g in file names?

exe = executable(
  'test_ttyutils',
  'lib/ttyutils.c',
  c_args : ['-DTEST_PROGRAM_TTYUTILS'],
  include_directories : dir_include,
  link_with : lib_common,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_blkdev',
  'lib/blkdev.c',
  c_args : ['-DTEST_PROGRAM_BLKDEV'],
  include_directories : dir_include,
  link_with : lib_common,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_ismounted',
  'lib/ismounted.c',
  c_args : ['-DTEST_PROGRAM_ISMOUNTED'],
  include_directories : dir_include,
  link_with : lib_common,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_mangle',
  'lib/mangle.c',
  c_args : ['-DTEST_PROGRAM_MANGLE'],
  include_directories : dir_include,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_strutils',
  'lib/strutils.c',
  c_args : ['-DTEST_PROGRAM_STRUTILS'],
  include_directories : dir_include,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_colors',
  'lib/colors.c',
  'lib/color-names.c',
  c_args : ['-DTEST_PROGRAM_COLORS'],
  include_directories : dir_include,
  link_with : [lib_common, lib_tcolors],
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_randutils',
  'lib/randutils.c',
  c_args : ['-DTEST_PROGRAM_RANDUTILS'],
  include_directories : dir_include,
  build_by_default: program_tests)
exes += exe

if conf.get('HAVE_OPENAT').to_string() == '1' \
   and conf.get('HAVE_DIRFD').to_string() == '1'
  exe = executable(
    'test_procfs',
    'lib/procfs.c',
    c_args : ['-DTEST_PROGRAM_PROCFS'],
    include_directories : dir_include,
    link_with : lib_common,
    build_by_default: program_tests)
  exes += exe

  exe = executable(
    'test_path',
    'lib/path.c',
    'lib/fileutils.c',
    have_cpu_set_t ? 'lib/cpuset.c' : [],
    c_args : ['-DTEST_PROGRAM_PATH'],
    include_directories : dir_include,
    link_with : lib_common,
    build_by_default: program_tests)
  exes += exe
endif

if have_pty
  exe = executable(
    'test_pty',
    pty_session_c,
    monotonic_c,
    c_args : ['-DTEST_PROGRAM_PTY'],
    include_directories : dir_include,
    link_with : [lib_common],
    dependencies : [lib_m,
                    realtime_libs,
                    lib_util],
    build_by_default: program_tests)
  exes += exe
endif

if LINUX
  exe = executable(
    'test_cpuset',
    'lib/cpuset.c',
    c_args : ['-DTEST_PROGRAM_CPUSET'],
    include_directories : dir_include,
    build_by_default: program_tests)
  exes += exe
endif

exe = executable(
  'test_sysfs',
  'lib/sysfs.c',
  'lib/path.c',
  'lib/buffer.c',
  'lib/mbsalign.c',
  'lib/fileutils.c',
  have_cpu_set_t ? 'lib/cpuset.c' : [],
  c_args : ['-DTEST_PROGRAM_SYSFS'],
  include_directories : dir_include,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_pager',
  'lib/pager.c',
  c_args : ['-DTEST_PROGRAM_PAGER'],
  include_directories : dir_include,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_linux_version',
  'lib/linux_version.c',
  c_args : ['-DTEST_PROGRAM_LINUXVERSION'],
  include_directories : dir_include,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_fileutils',
  'lib/fileutils.c',
  c_args : ['-DTEST_PROGRAM_FILEUTILS'],
  include_directories : dir_include,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_canonicalize',
  'lib/canonicalize.c',
  c_args : ['-DTEST_PROGRAM_CANONICALIZE'],
  include_directories : dir_include,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_timeutils',
  'lib/timeutils.c',
  'lib/strutils.c',
  c_args : ['-DTEST_PROGRAM_TIMEUTILS'],
  include_directories : dir_include,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_pwdutils',
  'lib/pwdutils.c',
  c_args : ['-DTEST_PROGRAM'],
  include_directories : dir_include,
  link_with : lib_common,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_logindefs',
  'lib/logindefs.c',
  c_args : ['-DTEST_PROGRAM'],
  include_directories : dir_include,
  link_with : [lib_common, logindefs_c],
  build_by_default: program_tests)
exes += exe


############################################################

exe = executable(
  'test_uuid_parser',
  'libuuid/src/test_uuid.c',
  include_directories : [dir_include, dir_libuuid],
  link_with : lib_uuid,
  dependencies : socket_libs,
  build_by_default: program_tests)
if not is_disabler(exe)
  exes += exe
endif

############################################################

libfdisk_tests_cflags = ['-DTEST_PROGRAM']
libfdisk_tests_ldadd = [lib_fdisk_static, lib_uuid]
libfdisk_tests_ldadd_deps = [blkid_dep]

exe = executable(
  'test_fdisk_ask',
  'libfdisk/src/ask.c',
  c_args : libfdisk_tests_cflags,
  include_directories : lib_fdisk_includes,
  link_with : libfdisk_tests_ldadd,
  dependencies : libfdisk_tests_ldadd_deps,
  build_by_default: program_tests)
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'test_fdisk_gpt',
  'libfdisk/src/gpt.c',
  c_args : libfdisk_tests_cflags,
  include_directories : lib_fdisk_includes,
  link_with : libfdisk_tests_ldadd,
  dependencies : libfdisk_tests_ldadd_deps,
  build_by_default: program_tests)
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'test_fdisk_utils',
  'libfdisk/src/utils.c',
  c_args : libfdisk_tests_cflags,
  include_directories : lib_fdisk_includes,
  link_with : libfdisk_tests_ldadd,
  dependencies : libfdisk_tests_ldadd_deps,
  build_by_default: program_tests)
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'test_fdisk_script',
  'libfdisk/src/script.c',
  c_args : libfdisk_tests_cflags,
  include_directories : lib_fdisk_includes,
  link_with : libfdisk_tests_ldadd,
  dependencies : libfdisk_tests_ldadd_deps,
  build_by_default: program_tests)
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'test_fdisk_version',
  'libfdisk/src/version.c',
  c_args : libfdisk_tests_cflags,
  include_directories : lib_fdisk_includes,
  link_with : libfdisk_tests_ldadd,
  dependencies : libfdisk_tests_ldadd_deps,
  build_by_default: program_tests)
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'test_fdisk_item',
  'libfdisk/src/item.c',
  c_args : libfdisk_tests_cflags,
  include_directories : lib_fdisk_includes,
  link_with : libfdisk_tests_ldadd,
  dependencies : libfdisk_tests_ldadd_deps,
  build_by_default: program_tests)
if not is_disabler(exe)
  exes += exe
endif

sample_fdisk_ldadd = [lib_common, lib_fdisk]

exe = executable(
  'sample-fdisk-mkpart',
  'libfdisk/samples/mkpart.c',
  include_directories : lib_fdisk_includes,
  link_with : sample_fdisk_ldadd)
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'sample-fdisk-mkpart-fullspec',
  'libfdisk/samples/mkpart-fullspec.c',
  include_directories : lib_fdisk_includes,
  link_with : sample_fdisk_ldadd)
if not is_disabler(exe)
  exes += exe
endif

############################################################

exe = executable(
  'test_cap',
  'tests/helpers/test_cap.c',
  include_directories : includes,
  dependencies : [lib_cap_ng],
  build_by_default: program_tests)
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'test_mbsencode',
  'tests/helpers/test_mbsencode.c',
  include_directories : includes,
  link_with : lib_common,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_byteswap',
  'tests/helpers/test_byteswap.c',
  include_directories : includes,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_md5',
  'tests/helpers/test_md5.c',
  md5_c,
  include_directories : includes,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_sha1',
  'tests/helpers/test_sha1.c',
  sha1_c,
  include_directories : includes,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_pathnames',
  'tests/helpers/test_pathnames.c',
  include_directories : includes,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_strerror',
  'tests/helpers/test_strerror.c',
  include_directories : includes,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_sysinfo',
  'tests/helpers/test_sysinfo.c',
  include_directories : includes,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_sigreceive',
  'tests/helpers/test_sigreceive.c',
  include_directories : includes,
  link_with : lib_common,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_tiocsti',
  'tests/helpers/test_tiocsti.c',
  include_directories : includes,
  build_by_default: program_tests)
exes += exe

exe = executable(
  'test_uuid_namespace',
  'tests/helpers/test_uuid_namespace.c',
  predefined_c,
  unpack_c,
  unparse_c,
  include_directories : includes,
  build_by_default: program_tests)
exes += exe

if LINUX and lib_rt.found()
  exe = executable(
    'test_mkfds',
    'tests/helpers/test_mkfds.c',
    'tests/helpers/test_mkfds.h',
    'tests/helpers/test_mkfds_ppoll.c',
    include_directories : includes,
    dependencies : [lib_rt],
    build_by_default: program_tests)
  exes += exe
endif

exe = executable(
  'test_enosys',
  'tests/helpers/test_enosys.c',
  include_directories : includes,
  build_by_default: program_tests)
exes += exe

############################################################

if conf.get('HAVE_OPENAT').to_string() == '1'
  exe = executable(
    'sample-scols-tree',
    'libsmartcols/samples/tree.c',
    include_directories : includes,
    link_with : [lib_smartcols, lib_common],
    build_by_default: program_tests)
  if not is_disabler(exe)
    exes += exe
  endif
endif

exe = executable(
  'sample-scols-title',
  'libsmartcols/samples/title.c',
  include_directories : includes,
  link_with : [lib_smartcols, lib_common])
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'sample-scols-wrap',
  'libsmartcols/samples/wrap.c',
  include_directories : includes,
  link_with : [lib_smartcols, lib_common])
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'sample-scols-continuous',
  'libsmartcols/samples/continuous.c',
  include_directories : includes,
  link_with : [lib_smartcols, lib_common])
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'sample-scols-continuous-json',
  'libsmartcols/samples/continuous-json.c',
  include_directories : includes,
  link_with : [lib_smartcols, lib_common])
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'sample-scols-maxout',
  'libsmartcols/samples/maxout.c',
  include_directories : includes,
  link_with : [lib_smartcols, lib_common])
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'sample-scols-fromfile',
  'libsmartcols/samples/fromfile.c',
  include_directories : includes,
  link_with : [lib_smartcols, lib_common])
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'sample-scols-grouping-simple',
  'libsmartcols/samples/grouping-simple.c',
  include_directories : includes,
  link_with : [lib_smartcols, lib_common])
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'sample-scols-grouping-overlay',
  'libsmartcols/samples/grouping-overlay.c',
  include_directories : includes,
  link_with : [lib_smartcols, lib_common])
if not is_disabler(exe)
  exes += exe
endif

exe = executable(
  'test_boilerplate',
  'Documentation/boilerplate.c',
   include_directories : includes,
   build_by_default: program_tests)
if not is_disabler(exe)
  exes += exe
endif

############################################################

# Let the test runner know whether we're running under asan and export
# some paths. We use a file on disk so that it is possible to run the
# test scripts from commandline without setting any variables.
configure_file(output : 'meson.conf',
               capture : true,
               command : ['echo',
                          '''asan=@0@
PYTHON=@1@
'''.format(get_option('b_sanitize')=='address' ? 'yes' : '',
           python_program.full_path())])

run_sh = find_program('tests/run.sh')
run_target(
  'check',
  command : [run_sh,
             '--srcdir=' + meson.current_source_dir(),
             '--builddir=' + meson.current_build_dir(),
             '--parallel',
             '--nonroot'],
  depends : exes)


manadocs += ['lib/terminal-colors.d.5.adoc']
manadocs += ['libblkid/libblkid.3.adoc']

if build_libuuid
  manadocs += [
    'libuuid/man/uuid.3.adoc',
    'libuuid/man/uuid_clear.3.adoc',
    'libuuid/man/uuid_compare.3.adoc',
    'libuuid/man/uuid_copy.3.adoc',
    'libuuid/man/uuid_generate.3.adoc',
    'libuuid/man/uuid_is_null.3.adoc',
    'libuuid/man/uuid_parse.3.adoc',
    'libuuid/man/uuid_time.3.adoc',
    'libuuid/man/uuid_unparse.3.adoc']
  manlinks += {
    'uuid_generate_random.3': 'uuid_generate.3',
    'uuid_generate_time.3': 'uuid_generate.3',
    'uuid_generate_time_safe.3': 'uuid_generate.3',
  }
endif

asciidoctor = find_program('asciidoctor', required : false)
if asciidoctor.found()
  foreach adoc : manadocs
    name = adoc.split('/')[-1]
    page = name.split('.adoc')[0]
    section = page.split('.')[-1]
    mandirn = join_paths(mandir, 'man' + section)
    input = adoc

    custom_target(
      page,
      command : [ asciidoctor,
	  '-b', 'manpage',
	  '-a', 'VERSION=' + meson.project_version(),
	  '-a', 'release-version=' + meson.project_version(),
	  '-a', 'ADJTIME_PATH=/etc/adjtime',
	  '-a', 'package-docdir=' + docdir,
	  '--base-dir=' + meson.current_source_dir(),
          '--destination-dir=' + meson.current_build_dir(),
          '--load-path', '@SOURCE_ROOT@/tools',
          '--require', 'asciidoctor-includetracker',
	  '@INPUT@'],
      input : input,
      output : [page],
      depfile : page + '.deps',
      install: true,
      install_dir : mandirn)
  endforeach

  foreach link_name, target : manlinks
    link_section = link_name.split('.')[-1]
    target_section = target.split('.')[-1]
    meson.add_install_script(meson_make_manpage_stub,
      join_paths('man' + target_section, target),
      join_paths(mandir, 'man' + link_section, link_name))
  endforeach
endif

if bash_completion.found()
  foreach completion : bashcompletions
    install_data(
      join_paths('bash-completion', completion),
      install_dir : bash_completion.get_variable(pkgconfig : 'completionsdir')
    )
  endforeach
  foreach link_name, target : bashcompletionslinks
    meson.add_install_script(meson_make_symlink,
      target,
      join_paths(bash_completion.get_variable(pkgconfig : 'completionsdir'), link_name))
  endforeach
endif
