project('Linux-PAM', 'c',
        version: '1.7.1',
        license: 'BSD-3-Clause OR GPL-2.0-or-later',
        default_options: [
          'b_pie=true',
          'prefix=/usr',
          'warning_level=2',
        ],
        meson_version: '>= 0.62.0',
)

fs = import('fs')
pkgconfig = import('pkgconfig')


prefixdir = get_option('prefix')
if not fs.is_absolute(prefixdir)
  error('Prefix is not absolute: "@0@"'.format(prefixdir))
endif
datadir = prefixdir / get_option('datadir')
includedir = prefixdir / get_option('includedir') / 'security'
libdir = prefixdir / get_option('libdir')
localedir = prefixdir / get_option('localedir')
localstatedir = '/' / get_option('localstatedir')
mandir = prefixdir / get_option('mandir')
sbindir = prefixdir / get_option('sbindir')
sysconfdir = prefixdir / get_option('sysconfdir')
securedir = get_option('securedir')
if securedir == ''
  securedir = libdir / 'security'
endif


cc = meson.get_compiler('c')
cdata = configuration_data()

null_dep = dependency('', required: false)

cdata.set('_GNU_SOURCE', 1)
cdata.set_quoted('PACKAGE', meson.project_name())
cdata.set_quoted('PAM_VERSION', meson.project_version())
cdata.set('UNUSED', '__attribute__((__unused__))')
cdata.set('PAM_NO_HEADER_FUNCTIONS', 1, description: 'Disable function bodies in headers')
cdata.set_quoted('SYSCONFDIR', sysconfdir)
cdata.set_quoted('LTDIR', '')
# used in configure_file() only, so not quoted
cdata.set('sbindir', sbindir)

cdata.set('PAM_DEBUG', get_option('pam-debug') ? 1 : false)
cdata.set('PAM_LOCKING', get_option('pamlocking') ? 1 : false)
cdata.set('PAM_READ_BOTH_CONFS', get_option('read-both-confs') ? 1 : false)
cdata.set10('DEFAULT_USERGROUPS_SETTING', get_option('usergroups'))
cdata.set('PAM_USERTYPE_UIDMIN', get_option('uidmin'))
cdata.set('PAM_USERTYPE_OVERFLOW_UID', get_option('kernel-overflow-uid'))
cdata.set('PAM_MISC_CONV_BUFSIZE', get_option('misc-conv-bufsize'))

cdata.set_quoted('_PAM_ISA',
  get_option('isadir') != '' ? get_option('isadir') : '../..' / fs.name(libdir) / 'security')

docdir = get_option('docdir')
if docdir == ''
  docdir = datadir / 'doc' / meson.project_name()
endif

htmldir = get_option('htmldir')
if htmldir == ''
  htmldir = docdir
endif

pdfdir = get_option('pdfdir')
if pdfdir == ''
  pdfdir = docdir
endif

sconfigdir = get_option('sconfigdir')
if sconfigdir == ''
  sconfigdir = sysconfdir / 'security'
endif
cdata.set_quoted('SCONFIG_DIR', sconfigdir)
# used in configure_file() only, so not quoted
cdata.set('SCONFIGDIR', sconfigdir)

vendordir = get_option('vendordir')
if get_option('vendordir') == ''
  vendor_sconfigdir = sconfigdir
  vendor_sysconfdir = sysconfdir
  stringparam_vendordir = []
  stringparam_profileconditions = 'without_vendordir'
else
  vendor_sconfigdir = vendordir / 'security'
  vendor_sysconfdir = vendordir
  cdata.set_quoted('VENDORDIR', vendordir)
  cdata.set_quoted('VENDOR_SCONFIG_DIR', vendor_sconfigdir)
  stringparam_vendordir = ['--stringparam', 'vendordir', vendordir]
  stringparam_profileconditions = 'with_vendordir'
endif

randomdev_option = get_option('randomdev')
if randomdev_option == '' or randomdev_option == 'yes'
  cdata.set_quoted('PAM_PATH_RANDOMDEV', '/dev/urandom')
elif randomdev_option != 'no'
  cdata.set_quoted('PAM_PATH_RANDOMDEV', randomdev_option)
endif

xauth_option = get_option('xauth')
if xauth_option != '' and xauth_option != '/usr/X11R6/bin/xauth'
  cdata.set_quoted('PAM_PATH_XAUTH', xauth_option)
endif


# meson defines -D_STRIP_FILE_OFFSET_BITS=64 even for 64-bit systems, this
# forces glibc to use function names with the same semantics but with "64"
# suffix, and that leads to discrepancies in the build artefacts produced
# by different build systems.
if cc.sizeof('long') >= 8
  add_project_arguments(
    '-U_FILE_OFFSET_BITS',
    language: 'c')
endif


try_cc_flags = [
  '-Wbad-function-cast',
  '-Wcast-align',
  '-Wcast-align=strict',
  '-Wcast-qual',
  '-Wdeprecated',
  '-Wformat=2',
  '-Winit-self',
  '-Wmain',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-prototypes',
  '-Wnull-dereference',
  '-Wpointer-arith',
  '-Wreturn-type',
  '-Wshadow',
  '-Wstrict-prototypes',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused',
  '-Wwrite-strings',
  '-Wzero-as-null-pointer-constant',
]
add_project_arguments(
  cc.get_supported_arguments(try_cc_flags),
  language: 'c')

add_project_link_arguments(
  # --as-needed and --no-undefined are enabled by default
  cc.get_supported_link_arguments([
    '-Wl,--fatal-warnings',
    '-Wl,-O1',
  ]),
  language: 'c')

exe_link_args = cc.get_supported_link_arguments([
  '-Wl,-z,relro',
  '-Wl,-z,now',
])


check_headers = [
  'crypt.h',
  'paths.h',
  'sys/random.h',
]
foreach h: check_headers
  if cc.has_header(h)
    cdata.set('HAVE_' + h.underscorify().to_upper(), 1)
  endif
endforeach


check_functions = [
  ['close_range', '#include <unistd.h>'],
  ['explicit_bzero', '#include <string.h>'],
  ['getdomainname', '#include <unistd.h>'],
  ['getgrgid_r', '#include <grp.h>'],
  ['getgrnam_r', '#include <grp.h>'],
  ['getgrouplist', '#include <grp.h>'],
  ['getmntent_r', '#include <mntent.h>'],
  ['getpwnam', '#include <pwd.h>'],
  ['getpwnam_r', '#include <pwd.h>'],
  ['getpwuid_r', '#include <pwd.h>'],
  ['getrandom', '#include <sys/random.h>'],
  ['getspnam_r', '#include <shadow.h>'],
  ['getutent_r', '#include <utmp.h>'],
  ['innetgr', '#include <netdb.h>'],
  ['memset_explicit', '#include <string.h>'],
  ['quotactl', '#include <sys/quota.h>'],
  ['ruserok', '#include <netdb.h>'],
  ['ruserok_af', '#include <netdb.h>'],
  ['unshare', '#include <sched.h>'],
]
foreach ident: check_functions
  if cc.has_function(ident[0], prefix: ident[1], args: '-D_GNU_SOURCE')
    cdata.set('HAVE_' + ident[0].to_upper(), 1)
  endif
endforeach

enable_pam_keyinit = cc.get_define('__NR_keyctl', prefix: '#include <sys/syscall.h>') != ''

if get_option('mailspool') != ''
  cdata.set_quoted('PAM_PATH_MAILDIR', get_option('mailspool'))
else
  have = cc.get_define('_PATH_MAILDIR', prefix: '#include <paths.h>') != ''
  cdata.set('PAM_PATH_MAILDIR', have ? '_PATH_MAILDIR' : '"/var/spool/mail"')
endif


libdl = dependency('dl')


if get_option('i18n').disabled()
  libintl = null_dep
else
  libintl = dependency('intl', required: get_option('i18n'))
  if libintl.found()
    cdata.set('ENABLE_NLS', 1)
    cdata.set_quoted('LOCALEDIR', localedir)
    foreach f: ['bindtextdomain',
                'dngettext',
               ]
      if cc.has_function(f, prefix: '#include <libintl.h>',
                         args: '-D_GNU_SOURCE',
                         dependencies: libintl)
        cdata.set('HAVE_' + f.to_upper(), 1)
      endif
    endforeach
  endif
endif


libaudit = dependency('audit', required: get_option('audit'))
if libaudit.found()
  cdata.set('HAVE_LIBAUDIT', 1)
  foreach ident: ['struct audit_tty_status']
    if cc.sizeof(ident, prefix: '#include <libaudit.h>') > 0
      cdata.set('HAVE_' + ident.underscorify().to_upper(), 1)
    endif
  endforeach
  foreach ident: [['struct audit_tty_status', 'log_passwd']]
    if cc.has_member(ident[0], ident[1],
                     prefix: '#include <libaudit.h>',
                     args: '-D_GNU_SOURCE')
      cdata.set('HAVE_@0@_@1@'.format(ident[0], ident[1]).underscorify().to_upper(), 1)
    endif
  endforeach
endif


libcrypt = dependency('libcrypt', 'libxcrypt', required: false)
if not libcrypt.found()
  libcrypt = cc.find_library('crypt')
endif
crypt_h = cdata.get('HAVE_CRYPT_H') == 1 ? '''#include <crypt.h>''' : '''#include <unistd.h>'''
foreach f: ['crypt_r']
  if cc.has_function(f, prefix: crypt_h,
                     args: '-D_GNU_SOURCE',
                     dependencies: libcrypt)
    cdata.set('HAVE_' + f.to_upper(), 1)
  endif
endforeach


libeconf = dependency('libeconf', version: '>= 0.5.0', required: get_option('econf'))
if libeconf.found()
  cdata.set('USE_ECONF', 1)
  have_econf_readconfig = true
  if stringparam_vendordir != []
    stringparam_profileconditions += ';with_vendordir_and_with_econf'
  endif

  foreach f: ['econf_newKeyFile_with_options',
              'econf_readConfigWithCallback',
             ]
    if cc.has_function(f, prefix: '#include <libeconf.h>',
                       args: '-D_GNU_SOURCE',
                       dependencies: libeconf)
      cdata.set('HAVE_' + f.to_upper(), 1)
    else
      have_econf_readconfig = false
    endif
    if have_econf_readconfig
      cdata.set('HAVE_ECONF_READCONFIG', 1)
    endif
  endforeach
else
  if stringparam_vendordir != []
    stringparam_profileconditions += ';with_vendordir_and_without_econf'
  endif
endif


libselinux = dependency('libselinux', required: get_option('selinux'))
if libselinux.found()
  cdata.set('WITH_SELINUX', 1)
  foreach f: ['getseuser', 'setkeycreatecon']
    if cc.has_function(f, prefix: '#include <selinux/selinux.h>',
                       dependencies: libselinux)
      cdata.set('HAVE_' + f.to_upper(), 1)
    endif
  endforeach
endif


if get_option('openssl').disabled()
  libcrypto = null_dep
else
  libcrypto = dependency('libcrypto', required: false)
endif
if libcrypto.found() and cc.has_function('EVP_MAC_CTX_new',
                                         prefix: '#include <openssl/hmac.h>',
                                         dependencies: libcrypto)
  cdata.set('WITH_OPENSSL', 1)
  stringparam_profileconditions += ';openssl_hmac'
else
  stringparam_profileconditions += ';no_openssl_hmac'
endif

use_logind = get_option('logind')
use_elogind = get_option('elogind').require(not use_logind.enabled()).disable_auto_if(use_logind.enabled())
use_logind = use_logind.disable_auto_if(use_elogind.enabled())
logind_dep = dependency('libsystemd', version: '>= 254', required: use_logind)
cdata.set('USE_LIBSYSTEMD', logind_dep.found() ? 1 : false)
if not logind_dep.found()
  logind_dep = dependency('libelogind', version: '>= 254', required: use_elogind)
  cdata.set('USE_LIBELOGIND', logind_dep.found() ? 1 : false)
endif
cdata.set('USE_LOGIND', logind_dep.found() ? 1 : false)

systemdunitdir = get_option('systemdunitdir')
if systemdunitdir == ''
  systemdunitdir = prefixdir / 'lib' / 'systemd' / 'system'
  systemd = dependency('systemd', required: false)
  if systemd.found()
    systemdsystemunitdir = systemd.get_variable(
      pkgconfig: 'systemdsystemunitdir',
      default_value: systemdunitdir,
    )
    if systemdsystemunitdir.startswith(prefixdir / '')
      systemdunitdir = systemdsystemunitdir
    endif
  endif
endif


feature_pam_userdb = get_option('pam_userdb')
if feature_pam_userdb.disabled()
  enable_pam_userdb = false
else
  use_db = get_option('db')
  have = false
  if use_db == 'db' or use_db == 'auto'
    db_uniquename = get_option('db-uniquename')
    name = 'db' + db_uniquename
    libdb = cc.find_library(name, required: false)
    if libdb.found()
      foreach f: ['dbm_open' + db_uniquename, 'dbm_open']
        have = cc.links('''
                        #define _GNU_SOURCE
                        #define DB_DBM_HSEARCH 1
                        #define HAVE_DBM 1
                        #include <db.h>
                        int main(void) {return !@0@("", 0, 0);}
                        '''.format(f),
                        args: '-l' + name,
                        name: '@0@ in @1@'.format(f, 'lib' + name))
        if have
          break
        endif
      endforeach
      if have
        use_db = 'db'
        cdata.set('HAVE_DB_H', 1)
      endif
    endif
  endif
  if use_db == 'gdbm' or use_db == 'auto'
    libdb = cc.find_library('gdbm', required: false)
    if libdb.found()
      have = cc.has_function('gdbm_store', prefix: '#include <gdbm.h>',
                             args: '-D_GNU_SOURCE', dependencies: libdb)
      if have
        use_db = 'gdbm'
        cdata.set('HAVE_GDBM_H', 1)
        if cc.compiles('''#include <gdbm.h>
                       int db_close(void *dbm) {return gdbm_close(dbm);}''',
                       args: '-D_GNU_SOURCE',
                       name: 'gdbm_close returns int')
          cdata.set('GDBM_CLOSE_RETURNS_INT', 1)
        endif
      endif
    endif
  endif
  if use_db == 'ndbm' or use_db == 'auto'
    libdb = cc.find_library('ndbm', required: false)
    if libdb.found()
      have = cc.has_function('dbm_store', prefix: '#include <ndbm.h>',
                             args: '-D_GNU_SOURCE', dependencies: libdb)
      if have
        use_db = 'ndbm'
        cdata.set('HAVE_NDBM_H', 1)
      endif
    endif
  endif
  if not have and feature_pam_userdb.enabled()
    error('pam_userdb module is enabled but the db library is not available')
  endif
  enable_pam_userdb = have
endif


feature_pam_lastlog = get_option('pam_lastlog')
if feature_pam_lastlog.disabled()
  enable_pam_lastlog = false
else
  libutil = cc.find_library('util', required: false)
  have = cc.has_function('logwtmp', prefix: '#include <utmp.h>',
                         dependencies: libutil)
  if not have and feature_pam_lastlog.enabled()
    error('pam_lastlog module is enabled but logwtmp() is not available')
  endif
  enable_pam_lastlog = have
endif


enable_pam_unix = not get_option('pam_unix').disabled()
if enable_pam_unix
  if get_option('lckpwdf')
    cdata.set('USE_LCKPWDF', 1)
    foreach f: ['lckpwdf']
      if cc.has_function(f, prefix: '#include <shadow.h>')
        cdata.set('HAVE_' + f.to_upper(), 1)
      endif
    endforeach
  endif

  feature_nis = get_option('nis')
  if feature_nis.disabled()
    enable_nis = false
    libtirpc = null_dep
    libnsl = null_dep
  else
    # If libtirpc is available, prefer that over the system implementation.
    libtirpc = dependency('libtirpc', required: false)
    enable_nis = true
    foreach f: ['getrpcport',
                'rpcb_getaddr',
               ]
      if cc.has_function(f, prefix: '#include <rpc/rpc.h>',
                         dependencies: libtirpc)
        cdata.set('HAVE_' + f.to_upper(), 1)
        cdata.set('HAVE_DECL_' + f.to_upper(), 1)
      endif
    endforeach

    libnsl = dependency('libnsl', required: feature_nis)
    enable_nis = libnsl.found()

    if enable_nis
      foreach f: ['yp_bind',
                  'yp_get_default_domain',
                  'yp_match',
                  'yp_unbind',
                 ]
        if not cc.has_function(f, prefix: '''#include <rpcsvc/ypclnt.h>
                                             #include <rpcsvc/yp_prot.h>''',
                               dependencies: [libnsl, libtirpc])
          enable_nis = false
          libnsl = null_dep
          break
        endif
      endforeach
    endif

    if enable_nis
      cdata.set('HAVE_NIS', 1)
    elif feature_nis.enabled()
      error('NIS is enabled but required interfaces are not available')
    endif
  endif
endif


feature_docs = get_option('docs')
enable_docs = not feature_docs.disabled()
if enable_docs
  prog_xsltproc = find_program(
    'xsltproc',
    required: feature_docs,
    native: true,
    disabler: true,
  )
  prog_xmllint = find_program(
    'xmllint',
    required: feature_docs,
    native: true,
    disabler: true,
  )
  prog_w3m = find_program(
    'w3m',
    required: false,
    native: true,
  )
  if prog_w3m.found()
    browser = [prog_w3m, '-T', 'text/html', '-dump']
  else
    prog_elinks = find_program(
      'elinks',
      required: feature_docs,
      native: true,
      disabler: true,
    )
    browser = [prog_elinks, '-no-numbering', '-no-references', '-dump']
  endif
  prog_fop = find_program(
    'fop',
    required: false,
    native: true,
    disabler: true,
  )

  prog_xmlcatalog = find_program(
    'xmlcatalog',
    required: feature_docs,
    native: true,
    disabler: true,
  )
  xml_catalog = get_option('xml-catalog')
  if xml_catalog == ''
    xml_catalog = '/etc/xml/catalog'
  endif

  have = prog_xmlcatalog.found() and run_command(
    ['test', '-f', xml_catalog],
    check: feature_docs.enabled()
  ).returncode() == 0
  if not have
    xml_catalog = disabler()
  endif

  docbook_rng = get_option('docbook-rng')
  have = run_command(
    [prog_xmlcatalog, '--noout', xml_catalog, docbook_rng],
    check: feature_docs.enabled()
  ).returncode() == 0
  if not have
    docbook_rng = disabler()
  endif

  html_stylesheet = get_option('html-stylesheet')
  have = run_command(
    [prog_xmlcatalog, '--noout', xml_catalog, html_stylesheet],
    check: feature_docs.enabled()
  ).returncode() == 0
  if not have
    html_stylesheet = disabler()
  endif

  txt_stylesheet = get_option('txt-stylesheet')
  have = run_command(
    [prog_xmlcatalog, '--noout', xml_catalog, txt_stylesheet],
    check: feature_docs.enabled()
  ).returncode() == 0
  if not have
    txt_stylesheet = disabler()
  endif

  man_stylesheet = get_option('man-stylesheet')
  have = run_command(
    [prog_xmlcatalog, '--noout', xml_catalog, man_stylesheet],
    check: feature_docs.enabled()
  ).returncode() == 0
  if not have
    man_stylesheet = disabler()
  endif

  pdf_stylesheet = get_option('pdf-stylesheet')
  have = run_command(
    [prog_xmlcatalog, '--noout', xml_catalog, pdf_stylesheet],
    check: feature_docs.enabled()
  ).returncode() == 0
  if not have
    pdf_stylesheet = disabler()
  endif

  stringparam_profileconditions = [
    '--stringparam',
    'profile.condition',
    stringparam_profileconditions
  ]
else
  prog_xsltproc = disabler()
  prog_xmllint = disabler()
  prog_fop = disabler()
  browser = disabler()
  custom_man_xsl = disabler()
  docbook_rng = disabler()
  html_stylesheet = disabler()
  man_stylesheet = disabler()
  txt_stylesheet = disabler()
endif


prog_flex = find_program(
  'flex',
  required: false,
  native: true,
  disabler: true,
)

yacc_cmd = []
prog_yacc = find_program(
  'bison',
  required: false,
  native: true,
)
if prog_yacc.found()
  yacc_cmd = [prog_yacc, '-o', '@OUTPUT0@', '--defines=@OUTPUT1@', '@INPUT@']
else
  prog_yacc = find_program(
    'byacc',
    required: false,
    native: true,
    disabler: true,
  )
  yacc_cmd = [prog_yacc, '-o', '@OUTPUT0@', '-H', '@OUTPUT1@', '@INPUT@']
endif


redir_exe = files('aux' / 'redir_exe.sh')
chdir_meson_build_subdir = files('aux' / 'chdir_meson_build_subdir.sh')


libpam_private_inc = include_directories('libpam')
libpam_inc = include_directories('libpam' / 'include')


subdir('po')
subdir('libpam_internal')
subdir('libpam')
subdir('libpamc')
subdir('libpam_misc')
if enable_docs
  subdir('doc')
endif
subdir('tests')
subdir('modules')
subdir('conf' / 'pam_conv1')
if get_option('examples')
  subdir('examples')
endif
if get_option('xtests')
  subdir('xtests')
endif
