lib_common_sources = '''
	blkdev.c
	buffer.c
	canonicalize.c
	color-names.c
	crc32.c
	crc32c.c
        crc64.c
	c_strtod.c
	encode.c
	env.c
	fileutils.c
	idcache.c
	jsonwrt.c
	mangle.c
	match.c
	mbsalign.c
	mbsedit.c
	md5.c
	procfs.c
	pwdutils.c
	randutils.c
	sha1.c
	sha256.c
	signames.c
	strutils.c
	strv.c
	timeutils.c
	ttyutils.c
	xxhash.c
'''.split()

idcache_c = files('idcache.c')
randutils_c = files('randutils.c')
md5_c = files('md5.c')
sha1_c = files('sha1.c')
strutils_c = files('strutils.c')
strv_c = files('strv.c')
pager_c = files('pager.c')

lib_common_sources += [idcache_c,
                       randutils_c,
                       md5_c,
                       sha1_c,
                       strutils_c,
                       strv_c]

monotonic_c = files('monotonic.c')
timer_c = files('timer.c')
swapprober_c = files('swapprober.c')
pty_session_c = files('pty-session.c')
ismounted_c = files('ismounted.c')
exec_shell_c = files('exec_shell.c')
fileeq_c = files('fileeq.c')
logindefs_c = static_library('logindefs',
  sources : ['logindefs.c'],
  include_directories : dir_include,
  dependencies : lib_econf,
)
selinux_utils_c = files('selinux-utils.c')

if LINUX
  lib_common_sources += '''
    caputils.c
    linux_version.c
    loopdev.c
'''.split()
endif

if build_plymouth_support
  lib_common_sources += '''
        plymouth-ctrl.c
'''.split()
endif

if conf.get('HAVE_LANGINFO_H') not in [1]
  lib_common_sources += 'langinfo.c'
endif

if conf.get('HAVE_CPU_SET_T') in [1]
  lib_common_sources += 'cpuset.c'
endif

if conf.get('HAVE_OPENAT') in [1] and conf.get('HAVE_DIRFD') in [1]
  lib_common_sources += '''
    path.c
    procfs.c
    sysfs.c
'''.split()
endif

lib_common = static_library(
  'common',
  lib_common_sources,
  include_directories : dir_include)


lib_color_sources = files('''
  colors.c
  color-names.c
'''.split())
# colors.h include/color-names.h

lib_tcolors = static_library(
  'tcolors',
  lib_color_sources,
  include_directories : dir_include,
  dependencies : curses_libs)
