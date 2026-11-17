# Copyright © 2019, 2020, 2021 Christian Persch
#
# This programme is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# This programme is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this programme.  If not, see <https://www.gnu.org/licenses/>.

project(
  'gnome-terminal',
  ['c', 'cpp',],
  default_options: [
    'buildtype=release',
    'warning_level=0',
    'b_ndebug=false',
  ],
  license: [
    'GPL-3.0-or-later',             # the programme
    'GPL-3.0-only', 'CC-BY-SA-3.0', # the documentation
    'GFDL-1.3-only',                # the appstream data
  ],
  meson_version: '>= 0.62.0',
  version: '3.54.5'
)

# Naming

gt_name     = 'gnome-terminal'
gt_dns_name = 'org.gnome.Terminal'

# Compiler requirements

c_req_std = 'gnu11'
cxx_req_std               = 'gnu++14'
gxx_req_version           = '4.8.1'
clangxx_req_version       = '3.3'

# Requirements

vte_req                      = 'vte-2.91'
vte_req_version              = '0.78.0'
vte_default_options          = [
  'docs=false',
  'gir=false',
  'glade=false',
  'gtk3=true',
  'gtk4=false',
  'vapi=false',
]

glib_req_version             = '2.52.0'
glib_min_req_version         = '2.52'
# Fil-C: GType is a pointer in the pizlonated glib; GLIB_VERSION_MAX_ALLOWED
# must be 2.80 or newer so that G_DEFINE_TYPE generates the pointer-based
# g_once_init_enter_pointer path.
glib_max_allowed_version     = '2.80'

gtk_req_version              = '3.22.27'
gtk_min_req_version          = '3.18'
gtk_max_allowed_version      = '3.24'

libnautilus_ext_req_version  = '43'
pcre2_req_version            = '10.00'
schemas_req_version          = '0.1.0'

# Versioning

gt_version = meson.project_version()

version_split = meson.project_version().split('.')
gt_major_version = version_split[0].to_int()
gt_minor_version = version_split[1].to_int()
gt_micro_version = version_split[2].to_int()

# Directories

gt_bindir     = get_option('bindir')
gt_datadir    = get_option('datadir')
gt_includedir = get_option('includedir')
gt_libdir     = get_option('libdir')
gt_libexecdir = get_option('libexecdir')
gt_localedir  = get_option('localedir')
gt_mandir     = get_option('mandir')
gt_prefix     = get_option('prefix')

gt_pkgdatadir = gt_datadir / gt_name
gt_pkglibdir  = gt_libdir  / gt_name

gt_schemadir = gt_datadir / 'glib-2.0' / 'schemas'

gt_searchproviderdir = gt_datadir / 'gnome-shell' / 'search-providers'

gt_nautilusextensiondir = gt_libdir / 'nautilus' / 'extensions-4'

gt_dbusinterfacedir = gt_datadir / 'dbus-1' / 'interfaces'
gt_dbusservicedir   = gt_datadir / 'dbus-1' / 'services'

# It is correct for this to be in ${prefix}/lib, even on systems where that
# does not match ${libdir}. This is what systemd uses on such platforms.
gt_systemduserdir = 'lib' / 'systemd' / 'user'

# Debug

enable_debug = get_option('dbg') or get_option('debug') or get_option('buildtype').contains('debug')

# Meson modules

gnome = import('gnome')
i18n  = import('i18n')
pkg   = import('pkgconfig')

# i18n

gt_gettext_domain = gt_name

# Compilers

# C compiler

cc = meson.get_compiler('c')

c_std_opt = '-std=' + c_req_std
assert(cc.has_argument(c_std_opt), 'option ' + c_std_opt + ' not supported by ' + cc.get_id())
add_project_arguments(c_std_opt, language: 'c')

# C++ compiler

cxx = meson.get_compiler('cpp')

cxx_std_opt = '-std=' + cxx_req_std
assert(cxx.has_argument(cxx_std_opt), 'option ' + cxx_std_opt + ' not supported by ' + cxx.get_id())
add_project_arguments(cxx_std_opt, language: 'cpp')

# The above only checks that the compiler supports the given -std option,
# but not that the compiler really supports that C++ standard version.
# Do a simple version check based on
# https://gcc.gnu.org/projects/cxx-status.html and
# https://clang.llvm.org/cxx_status.html
# for the C++ features that vte actually uses.

if cxx.get_id() == 'gcc'
  assert(cxx.version().version_compare('>=' + gxx_req_version), 'needs g++ >= ' + gxx_req_version + ' for ' + cxx_req_std + ' support')
endif

if cxx.get_id() == 'clang'
  assert(cxx.version().version_compare('>=' + clangxx_req_version), 'needs clang++ >= ' + clangxx_req_version + ' for ' + cxx_req_std + ' support')
endif

# Include directories

top_inc = include_directories('.')

# Asserts must not be disabled

assert(get_option('b_ndebug') == 'false', 'assertions may not be disabled')

# Start config.h

config_h = configuration_data()

config_h.set_quoted('GETTEXT_PACKAGE', gt_gettext_domain)
config_h.set_quoted('VERSION', gt_version)
config_h.set('ENABLE_DEBUG', enable_debug)
config_h.set('ENABLE_SEARCH_PROVIDER', get_option('search_provider'))

# Enable libc features

libc_feature_defines = [
  ['_GNU_SOURCE', '1'],
  ['_POSIX_C_SOURCE', '200809L'],
  ['_XOPEN_SOURCE', '700'],
  ['_XOPEN_SOURCE_EXTENDED', '1'],
]

system = host_machine.system()

if system == 'freebsd'
  # Defining _POSIX_C_SOURCE above makes freebsd not expose some functionality
  # that's hidden behind __BSD_VISIBLE.  Not defininy any of the above however
  # makes it expose verything.

  libc_feature_defines = []

elif system == 'darwin'
  # See issue #2592
  libc_feature_defines += [
    ['_DARWIN_C_SOURCE', '1'],
  ]

elif system == 'netbsd'
  libc_feature_defines += [
    ['_NETBSD_SOURCE', '1'],
  ]

elif system == 'openbsd'
  libc_feature_defines += [
    ['_BSD_SOURCE', '1'],
  ]
endif

foreach f: libc_feature_defines
  config_h.set(f[0], f[1])
endforeach

# Min/max version requirement flags

minver = glib_min_req_version.split('.')
maxver = glib_max_allowed_version.split('.')

glib_version_cxxflags = [
  '-DGLIB_VERSION_MIN_REQUIRED=(G_ENCODE_VERSION(' + minver[0] + ',' + minver[1] + '))',
  '-DGLIB_VERSION_MAX_ALLOWED=(G_ENCODE_VERSION(' + maxver[0] + ',' + maxver[1] + '))',
]

minver = gtk_min_req_version.split('.')
maxver = gtk_max_allowed_version.split('.')

gtk_version_cxxflags = [
  '-DGDK_VERSION_MIN_REQUIRED=(G_ENCODE_VERSION(' + minver[0] + ',' + minver[1] + '))',
  '-DGDK_VERSION_MAX_ALLOWED=(G_ENCODE_VERSION(' + maxver[0] + ',' + maxver[1] + '))',
]

version_cxxflags = glib_version_cxxflags + gtk_version_cxxflags

# Asserts must not be disabled

assert(get_option('b_ndebug') == 'false', 'assertions may not be disabled')

# LTO very much NOT supported

assert(get_option('b_lto') == false, 'LTO not supported')

# Check for functions

libdl_dep = cxx.find_library('dl')

check_dl_functions_required = [
  'dlsym',
]

foreach func: check_dl_functions_required
  assert(cxx.has_function(func, dependencies: libdl_dep), func + ' not found')
endforeach

# Compiler flags

compiler_flags_common = [
  '-Waggregate-return',
  '-Wall',
  '-Wcast-align',
  '-Wendif-labels',
  '-Werror=init-self',
  '-Werror=missing-include-dirs',
  '-Werror=pointer-arith',
  '-Wextra',
  '-Wfloat-equal',
  '-Wformat-signedness',
  '-Wimplicit',
  '-Wlogical-op',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wnested-externs',
  '-Wno-maybe-uninitialized',
  '-Wno-missing-field-initializers',
  '-Wno-switch-enum',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Wpacked',
  '-Wshadow',
  '-Wsign-compare',
  '-Wstrict-aliasing=2',
  '-Wstrict-prototypes',
  '-Wundef',
  '-Wuninitialized',
  '-Wunsafe-loop-optimizations',
  '-Wvla',
  '-Wwrite-strings',
  '-fdiagnostics-show-option',
  '-fno-common',
  '-fstack-protector',
  '-fstack-protector-strong',
  '-fvisibility=hidden',
]

if enable_debug
  compiler_flags_common += [
    '-ggdb3',
  ]
endif

# These are currently needed but the code should be fixed instead
compiler_flags_common_undesirable = [
  '-fno-strict-aliasing'
]

compiler_flags_c_required = [
  '-fvisibility=hidden',
]

compiler_flags_c_only = [
  '-Waggregate-return',
  '-Werror=implicit-function-declaration',
  '-Werror=missing-prototypes',
  '-Wimplicit',
  '-Wimplicit-fallthrough=3',
  '-Wmissing-parameter-type',
  '-Wnested-externs',
  '-Wold-style-declaration',
  '-Wold-style-definition',
  '-Woverride-init',
  '-Wsign-compare',
  '-Wstrict-prototypes',
]

compiler_flags_cxx_only = [
  '-Wimplicit-fallthrough=5',
  '-Wnon-virtual-dtor',
  '-Wstrict-null-sentinel',
]

compiler_flags_cxx_required = [
  '-fvisibility-inlines-hidden',
  '-fvisibility=hidden',
]

global_cflags = cc.get_supported_arguments(compiler_flags_common +
                                           compiler_flags_common_undesirable +
                                           compiler_flags_c_only +
					   compiler_flags_c_required)
global_cxxflags = cxx.get_supported_arguments(compiler_flags_common +
                                              compiler_flags_common_undesirable +
                                              compiler_flags_cxx_only +
                                              compiler_flags_cxx_required)

foreach flag: compiler_flags_cxx_required
  assert(cxx.has_argument(flag), flag + ' is required but not supported')
endforeach

# These flags have to be tested together

compiler_flags_common_multi = [
  [
    '-Werror=format=2',
    '-Werror=format-nonliteral',
    '-Werror=format-security',
  ],
]

foreach flags : compiler_flags_common_multi
  if cc.has_multi_arguments(flags)
    global_cflags += flags
  endif
endforeach

# ... and now make these flags the default

add_project_arguments(global_cflags, language: 'c',)
add_project_arguments(global_cxxflags, language: 'cpp')

# Dependencies

vte_dep = dependency(
  vte_req,
  version: '>=' + vte_req_version,
  default_options: vte_default_options
)

gio_dep       = dependency('gio-2.0',      version: '>=' + glib_req_version)
gio_unix_dep  = dependency('gio-unix-2.0', version: '>=' + glib_req_version)
glib_dep      = dependency('glib-2.0',     version: '>=' + glib_req_version)
gtk_dep       = dependency('gtk+-3.0',     version: '>=' + gtk_req_version)
pcre2_dep     = dependency('libpcre2-8',   version: '>=' + pcre2_req_version)
pthreads_dep  = dependency('threads')
schemas_dep   = dependency('gsettings-desktop-schemas',
                                           version: '>=' + schemas_req_version)
uuid_dep      = dependency('uuid')

if get_option('nautilus_extension')
  libnautilus_extension_dep = dependency('libnautilus-extension-4', version: '>=' + libnautilus_ext_req_version,)
else
  libnautilus_extension_dep = dependency('', required: false,)
endif

if gtk_dep.get_variable('targets').contains('x11')
  x11_dep = dependency('x11')
else
  x11_dep = dependency('', required: false)
endif

# Write config.h

configure_file(
  output: 'config.h',
  configuration: config_h,
  install: false,
)

# Utilities

glib_compile_schemas = find_program('glib-compile-schemas')
xsltproc = find_program('xsltproc')

# Subdirs

subdir('src')
subdir('po')
subdir('data')

if get_option('docs')
  subdir('help')
  subdir('man')
endif

# Dist

meson.add_dist_script(
  find_program('meson_changelog.sh'),
)

# Simple compat Makefile

makefile_conf = configuration_data()
makefile_conf.set('srcdir', meson.current_source_dir())
makefile_conf.set('builddir', meson.current_build_dir())

configure_file(
  input: 'Makefile.meson',
  output: '@BASENAME@',
  configuration: makefile_conf,
  install: false,
)

# .gitignore everything in the build directory

configure_file(
  output: '.gitignore',
  command: ['echo', '**/**',],
  capture: true,
  install: false,
)

# Summary

output = '\n'
output += 'Configuration for gnome-terminal:\n\n'
output += '  Version:            ' + gt_version + '\n'
output += '\n'
output += '  C compiler:         ' + cc.get_id() + ' (version ' + cc.version() + ')\n'
output += '  C++ compiler:       ' + cxx.get_id() + ' (version ' + cxx.version() + ')\n'
output += '\n'
output += '  Coverage:           ' + get_option('b_coverage').to_string() + '\n'
output += '  Documentation:      ' + get_option('docs').to_string() + '\n'
output += '  Debug:              ' + enable_debug.to_string() + '\n'
output += '\n'
output += '  Prefix:             ' + get_option('prefix') + '\n'
output += '\n'
output += '  Nautilus extension: ' + get_option('nautilus_extension').to_string() + '\n'
output += '  Search provider:    ' + get_option('search_provider').to_string() + '\n'
message(output)

# Check stable/unstable status

if (gt_minor_version % 2) != 0
  if (gt_micro_version >= 99)
    warning('This is a release candidate release!\n')
  elif (gt_micro_version > 90)
    warning('This is a beta release!\n')
  else
    warning('This is an unstable development release!\n')
  endif
else
  message('This is a stable release.')
endif

# Done
