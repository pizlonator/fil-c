project('gst-plugins-base', 'c',
  version : '1.24.7',
  meson_version : '>= 1.1',
  default_options : [ 'warning_level=1',
                      'buildtype=debugoptimized' ])

gst_version = meson.project_version()
version_arr = gst_version.split('.')
gst_version_major = version_arr[0].to_int()
gst_version_minor = version_arr[1].to_int()
gst_version_micro = version_arr[2].to_int()
if version_arr.length() == 4
  gst_version_nano = version_arr[3].to_int()
else
  gst_version_nano = 0
endif
gst_version_is_stable = gst_version_minor.is_even()
gst_version_is_dev = gst_version_minor % 2 == 1 and gst_version_micro < 90

host_system = host_machine.system()

have_cxx = add_languages('cpp', native: false, required: false)

if host_system in ['ios', 'darwin']
  have_objc = add_languages('objc', native: false)
else
  have_objc = false
endif

glib_req = '>= 2.64.0'
orc_req = '>= 0.4.24'

if gst_version_is_stable
  gst_req = '>= @0@.@1@.0'.format(gst_version_major, gst_version_minor)
else
  gst_req = '>= ' + gst_version
endif

api_version = '1.0'
soversion = 0
# maintaining compatibility with the previous libtool versioning
# current = minor * 100 + micro
curversion = gst_version_minor * 100 + gst_version_micro
libversion = '@0@.@1@.0'.format(soversion, curversion)
osxversion = curversion + 1

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')
static_build = get_option('default_library') == 'static'
plugins = []
gst_libraries = []

cc = meson.get_compiler('c')

if cc.get_id() == 'msvc'
  msvc_args = [
      # Ignore several spurious warnings for things gstreamer does very commonly
      # If a warning is completely useless and spammy, use '/wdXXXX' to suppress it
      # If a warning is harmless but hard to fix, use '/woXXXX' so it's shown once
      # NOTE: Only add warnings here if you are sure they're spurious
      '/wd4018', # implicit signed/unsigned conversion
      '/wd4146', # unary minus on unsigned (beware INT_MIN)
      '/wd4244', # lossy type conversion (e.g. double -> int)
      '/wd4305', # truncating type conversion (e.g. double -> float)
      cc.get_supported_arguments(['/utf-8']), # set the input encoding to utf-8
  ]

  if gst_version_is_dev
    # Enable some warnings on MSVC to match GCC/Clang behaviour
    msvc_args += cc.get_supported_arguments([
      '/we4002', # too many actual parameters for macro 'identifier'
      '/we4003', # not enough actual parameters for macro 'identifier'
      '/we4013', # 'function' undefined; assuming extern returning int
      '/we4020', # 'function' : too many actual parameters
      '/we4027', # function declared without formal parameter list
      '/we4029', # declared formal parameter list different from definition
      '/we4033', # 'function' must return a value
      '/we4045', # 'array' : array bounds overflow
      '/we4047', # 'operator' : 'identifier1' differs in levels of indirection from 'identifier2'
      '/we4053', # one void operand for '?:'
      '/we4062', # enumerator 'identifier' in switch of enum 'enumeration' is not handled
      '/we4098', # 'function' : void function returning a value
    ])
  endif
  add_project_arguments(msvc_args, language: ['c', 'cpp'])
  # Disable SAFESEH with MSVC for plugins and libs that use external deps that
  # are built with MinGW
  noseh_link_args = ['/SAFESEH:NO']
else
  noseh_link_args = []
endif

if cc.has_link_argument('-Wl,-Bsymbolic-functions')
  add_project_link_arguments('-Wl,-Bsymbolic-functions', language : 'c')
endif

# glib doesn't support unloading, which means that unloading and reloading
# any library that registers static types will fail
if cc.has_link_argument('-Wl,-z,nodelete')
  add_project_link_arguments('-Wl,-z,nodelete', language: 'c')
endif

core_conf = configuration_data()
core_conf.set('ENABLE_NLS', 1)

# Symbol visibility
if cc.has_argument('-fvisibility=hidden')
  add_project_arguments('-fvisibility=hidden', language: 'c')
  if have_objc
    add_project_arguments('-fvisibility=hidden', language: 'objc')
  endif
endif

# Disable strict aliasing
if cc.has_argument('-fno-strict-aliasing')
  add_project_arguments('-fno-strict-aliasing', language: 'c')
endif

# Define G_DISABLE_DEPRECATED for development versions
if gst_version_is_dev
  message('Disabling deprecated GLib API')
  add_project_arguments('-DG_DISABLE_DEPRECATED', language: 'c')
endif

cast_checks = get_option('gobject-cast-checks')
if cast_checks.disabled() or (cast_checks.auto() and not gst_version_is_dev)
  message('Disabling GLib cast checks')
  add_project_arguments('-DG_DISABLE_CAST_CHECKS', language: 'c')
endif

glib_asserts = get_option('glib-asserts')
if glib_asserts.disabled() or (glib_asserts.auto() and not gst_version_is_dev)
  message('Disabling GLib asserts')
  add_project_arguments('-DG_DISABLE_ASSERT', language: 'c')
endif

glib_checks = get_option('glib-checks')
if glib_checks.disabled() or (glib_checks.auto() and not gst_version_is_dev)
  message('Disabling GLib checks')
  add_project_arguments('-DG_DISABLE_CHECKS', language: 'c')
endif

# These are only needed/used by the ABI tests from core
host_defines = [
  [ 'x86', 'HAVE_CPU_I386' ],
  [ 'x86_64', 'HAVE_CPU_X86_64' ],
  [ 'arm', 'HAVE_CPU_ARM' ],
  [ 'aarch64', 'HAVE_CPU_AARCH64' ],
  [ 'mips', 'HAVE_CPU_MIPS' ],
  [ 'powerpc', 'HAVE_CPU_PPC' ],
  [ 'powerpc64', 'HAVE_CPU_PPC64' ],
  [ 'alpha', 'HAVE_CPU_ALPHA' ],
  [ 'sparc', 'HAVE_CPU_SPARC' ],
  [ 'ia64', 'HAVE_CPU_IA64' ],
  [ 'hppa', 'HAVE_CPU_HPPA' ],
  [ 'm68k', 'HAVE_CPU_M68K' ],
  [ 's390', 'HAVE_CPU_S390' ],
]
foreach h : host_defines
  if h.get(0) == host_machine.cpu_family()
    core_conf.set(h.get(1), 1)
  endif
endforeach
# FIXME: should really be called HOST_CPU or such
core_conf.set_quoted('TARGET_CPU', host_machine.cpu())

check_headers = [
  ['HAVE_DLFCN_H', 'dlfcn.h'],
  ['HAVE_EMMINTRIN_H', 'emmintrin.h'],
  ['HAVE_INTTYPES_H', 'inttypes.h'],
  ['HAVE_MEMORY_H', 'memory.h'],
  ['HAVE_NETINET_IN_H', 'netinet/in.h'],
  ['HAVE_NETINET_TCP_H', 'netinet/tcp.h'],
  ['HAVE_PROCESS_H', 'process.h'],
  ['HAVE_SMMINTRIN_H', 'smmintrin.h'],
  ['HAVE_STDINT_H', 'stdint.h'],
  ['HAVE_STRINGS_H', 'strings.h'],
  ['HAVE_STRING_H', 'string.h'],
  ['HAVE_SYS_SOCKET_H', 'sys/socket.h'],
  ['HAVE_SYS_STAT_H', 'sys/stat.h'],
  ['HAVE_SYS_TYPES_H', 'sys/types.h'],
  ['HAVE_SYS_WAIT_H', 'sys/wait.h'],
  ['HAVE_UNISTD_H', 'unistd.h'],
  ['HAVE_WINSOCK2_H', 'winsock2.h'],
  ['HAVE_XMMINTRIN_H', 'xmmintrin.h'],
  ['HAVE_LINUX_DMA_BUF_H', 'linux/dma-buf.h'],
]
foreach h : check_headers
  if cc.has_header(h.get(1))
    core_conf.set(h.get(0), 1)
  endif
endforeach

check_functions = [
  ['HAVE_DCGETTEXT', 'dcgettext', '#include<libintl.h>'],
  ['HAVE_GMTIME_R', 'gmtime_r', '#include<time.h>'],
  ['HAVE_LOCALTIME_R', 'localtime_r', '#include<time.h>'],
  ['HAVE_LRINTF', 'lrintf', '#include<math.h>'],
  ['HAVE_MMAP', 'mmap', '#include<sys/mman.h>'],
  ['HAVE_LOG2', 'log2', '#include<math.h>'],
]

libm = cc.find_library('m', required : false)
foreach f : check_functions
  if cc.has_function(f.get(1), prefix : f.get(2), dependencies : libm)
    core_conf.set(f.get(0), 1)
  endif
endforeach

core_conf.set('SIZEOF_CHAR', cc.sizeof('char'))
core_conf.set('SIZEOF_INT', cc.sizeof('int'))
core_conf.set('SIZEOF_LONG', cc.sizeof('long'))
core_conf.set('SIZEOF_SHORT', cc.sizeof('short'))
core_conf.set('SIZEOF_VOIDP', cc.sizeof('void*'))

core_conf.set_quoted('GETTEXT_PACKAGE', 'gst-plugins-base-1.0')
core_conf.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
core_conf.set_quoted('PACKAGE', 'gst-plugins-base')
core_conf.set_quoted('VERSION', gst_version)
core_conf.set_quoted('PACKAGE_VERSION', gst_version)
core_conf.set_quoted('GST_API_VERSION', api_version)
core_conf.set_quoted('GST_DATADIR', join_paths(get_option('prefix'), get_option('datadir')))
core_conf.set_quoted('GST_LICENSE', 'LGPL')

install_plugins_helper = get_option('install_plugins_helper')
if install_plugins_helper == ''
  install_plugins_helper = join_paths(get_option('prefix'),
                                      get_option('libexecdir'),
                                      'gst-install-plugins-helper')
endif
core_conf.set_quoted('GST_INSTALL_PLUGINS_HELPER', install_plugins_helper)

warning_flags = [
  '-Wmissing-declarations',
  '-Wredundant-decls',
  '-Wundef',
  '-Wwrite-strings',
  '-Wformat',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Winit-self',
  '-Wmissing-include-dirs',
  '-Waddress',
  '-Wno-multichar',
  '-Wvla',
  '-Wpointer-arith',
]

warning_c_flags = [
  '-Wmissing-prototypes',
]

warning_cxx_flags = [
  '-Waggregate-return',
]

if have_cxx
  cxx = meson.get_compiler('cpp')
  foreach extra_arg : warning_cxx_flags
    if cxx.has_argument (extra_arg)
      add_project_arguments([extra_arg], language: 'cpp')
    endif
  endforeach
endif

foreach extra_arg : warning_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
  if have_cxx and cxx.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'cpp')
  endif
endforeach

foreach extra_arg : warning_c_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
endforeach

# GStreamer package name and origin url
gst_package_name = get_option('package-name')
if gst_package_name == ''
  if gst_version_nano == 0
    gst_package_name = 'GStreamer Base Plug-ins source release'
  elif gst_version_nano == 1
    gst_package_name = 'GStreamer Base Plug-ins git'
  else
    gst_package_name = 'GStreamer Base Plug-ins prerelease'
  endif
endif
core_conf.set_quoted('GST_PACKAGE_NAME', gst_package_name)
core_conf.set_quoted('GST_PACKAGE_ORIGIN', get_option('package-origin'))

# FIXME: These should be configure options
core_conf.set_quoted('DEFAULT_VIDEOSINK', 'autovideosink')
core_conf.set_quoted('DEFAULT_AUDIOSINK', 'autoaudiosink')

# Set whether the audioresampling method should be detected at runtime
core_conf.set('AUDIORESAMPLE_FORMAT_' + get_option('audioresample_format').to_upper(), true)

gst_plugins_base_args = ['-DHAVE_CONFIG_H']
if get_option('default_library') == 'static'
  gst_plugins_base_args += ['-DGST_STATIC_COMPILATION']
endif

valgrind_dep = dependency('valgrind', required: false)
core_conf.set('HAVE_VALGRIND', valgrind_dep.found())

libdrm_dep = dependency('libdrm', version : '>= 2.4.98',
  required : get_option('drm'),
  allow_fallback: host_system not in ['darwin', 'ios', 'android', 'windows']
)

core_conf.set('HAVE_LIBDRM', libdrm_dep.found())

# X11 checks are for sys/ and tests/
x11_dep = dependency('x11', required : get_option('x11'))
# GIO is used by the GIO plugin, and by the TCP, SDP, and RTSP plugins
gio_dep = dependency('gio-2.0', version: glib_req)
giounix_dep = dependency('', required: false)
if host_system != 'windows'
  giounix_dep = dependency('gio-unix-2.0')
endif
gmodule_dep = dependency('gmodule-no-export-2.0')

# some of the examples can use gdk-pixbuf and GTK+3
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', required : get_option('examples'))
gtk_dep = dependency('gtk+-3.0', version : '>= 3.10', required : get_option('examples'))
# TODO: https://github.com/mesonbuild/meson/issues/3941
if not get_option('x11').disabled()
  gtk_x11_dep = dependency('gtk+-x11-3.0', version : '>= 3.10', required : get_option('examples'))
else
  gtk_x11_dep = dependency('', required : false)
endif
# gtk+ quartz backend is only available on macOS
if host_system == 'darwin'
  gtk_quartz_dep = dependency('gtk+-quartz-3.0', version : '>= 3.10', required : get_option('examples'))
else
  gtk_quartz_dep = dependency('', required : false)
endif

core_conf.set('HAVE_X11', x11_dep.found())
core_conf.set('HAVE_GIO_UNIX_2_0', giounix_dep.found())

if gio_dep.type_name() == 'pkgconfig'
    core_conf.set_quoted('GIO_MODULE_DIR',
        gio_dep.get_variable('giomoduledir'))
    core_conf.set_quoted('GIO_LIBDIR',
        gio_dep.get_variable('libdir'))
    core_conf.set_quoted('GIO_PREFIX',
        gio_dep.get_variable('prefix'))
else
    core_conf.set_quoted('GIO_MODULE_DIR', join_paths(get_option('prefix'),
      get_option('libdir'), 'gio/modules'))
    core_conf.set_quoted('GIO_LIBDIR', join_paths(get_option('prefix'),
      get_option('libdir')))
    core_conf.set_quoted('GIO_PREFIX', join_paths(get_option('prefix')))
endif

configinc = include_directories('.')
libsinc = include_directories('gst-libs')

# To use the subproject make subprojects directory
# and put gstreamer meson git there (symlinking is fine)
gst_dep = dependency('gstreamer-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_dep'])
gst_base_dep = dependency('gstreamer-base-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_base_dep'])
gst_net_dep = dependency('gstreamer-net-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_net_dep'])
gst_check_dep = dependency('gstreamer-check-1.0', version : gst_req,
  required : get_option('tests'),
  fallback : ['gstreamer', 'gst_check_dep'])
gst_controller_dep = dependency('gstreamer-controller-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_controller_dep'])

have_orcc = false
orcc_args = []
orc_targets = []
# Used by various libraries/elements that use Orc code
orc_dep = dependency('orc-0.4', version : orc_req, required : get_option('orc'),
    fallback : ['orc', 'orc_dep'])
orcc = find_program('orcc', required : get_option('orc'))
if orc_dep.found() and orcc.found()
  have_orcc = true
  orcc_args = [orcc, '--include', 'glib.h']
  core_conf.set('HAVE_ORC', 1)
else
  message('Orc Compiler not found or disabled, will use backup C code')
  core_conf.set('DISABLE_ORC', 1)
endif

# Used to build SSE* things in audio-resampler
sse_args = '-msse'
sse2_args = '-msse2'
sse41_args = '-msse4.1'

have_sse = cc.has_argument(sse_args)
have_sse2 = cc.has_argument(sse2_args)
have_sse41 = cc.has_argument(sse41_args)

if host_machine.cpu_family() == 'arm'
  if cc.compiles('''
#include <arm_neon.h>
int32x4_t testfunc(int16_t *a, int16_t *b) {
  asm volatile ("vmull.s16 q0, d0, d0" : : : "q0");
  return vmull_s16(vld1_s16(a), vld1_s16(b));
}
''', name : 'NEON support')
    core_conf.set('HAVE_ARM_NEON', true)
  endif
endif

# Disable compiler warnings for unused variables and args if gst debug system is disabled
if gst_dep.type_name() == 'internal'
  gst_proj = subproject('gstreamer')
  gst_debug_disabled = not gst_proj.get_variable('gst_debug')
else
  # We can't check that in the case of subprojects as we won't
  # be able to build against an internal dependency (which is not built yet)
  gst_debug_disabled = cc.has_header_symbol('gst/gstconfig.h', 'GST_DISABLE_GST_DEBUG', dependencies: gst_dep)
endif

if gst_debug_disabled
  message('GStreamer debug system is disabled')
  if cc.get_argument_syntax() == 'msvc'
    msvc_args = cc.get_supported_arguments([
      '/wd4101', # 'identifier' : unreferenced local variable
      '/wd4189', # 'identifier' : local variable is initialized but not referenced
    ])
    add_project_arguments(msvc_args, language: 'c')
  elif cc.has_argument('-Wno-unused')
    add_project_arguments(['-Wno-unused'], language: 'c')
  endif
else
  if cc.get_argument_syntax() == 'msvc' and gst_version_is_dev
    msvc_args = cc.get_supported_arguments([
      '/we4101', # 'identifier' : unreferenced local variable
      '/we4189', # 'identifier' : local variable is initialized but not referenced
    ])
    add_project_arguments(msvc_args, language: 'c')
  endif
  message('GStreamer debug system is enabled')
endif

if cc.has_member('struct tcp_info', '__tcpi_reordering', prefix: '#include <netinet/tcp.h>')
  core_conf.set('HAVE_BSD_TCP_INFO', true)
endif

if cc.has_member('struct tcp_info', 'tcpi_reordering', prefix: '#include <netinet/tcp.h>')
  core_conf.set('HAVE_LINUX_TCP_INFO', true)
endif

gir = find_program('g-ir-scanner', required : get_option('introspection'))
gnome = import('gnome')
build_gir = gir.found() and (not meson.is_cross_build() or get_option('introspection').enabled())
gir_init_section = [ '--add-init-section=extern void gst_init(gint*,gchar**);' + \
    'g_setenv("GST_REGISTRY_DISABLE", "yes", TRUE);' + \
    'g_setenv("GST_REGISTRY_1.0", "@0@", TRUE);'.format(meson.current_build_dir() + '/gir_empty_registry.reg') + \
    'g_setenv("GST_PLUGIN_PATH_1_0", "", TRUE);' + \
    'g_setenv("GST_PLUGIN_SYSTEM_PATH_1_0", "", TRUE);' + \
    'gst_init(NULL,NULL);', '--quiet']

pkgconfig = import('pkgconfig')
plugins_pkgconfig_install_dir = join_paths(plugins_install_dir, 'pkgconfig')
if get_option('default_library') == 'shared'
  # If we don't build static plugins there is no need to generate pc files
  plugins_pkgconfig_install_dir = disabler()
endif

pkgconfig_variables = [
  'exec_prefix=${prefix}',
  'toolsdir=${exec_prefix}/bin',
  'pluginsdir=${libdir}/gstreamer-1.0',
  'girdir=${datadir}/gir-1.0',
  'typelibdir=${libdir}/girepository-1.0',
  'pluginscannerdir=${libexecdir}/gstreamer-1.0'
]
pkgconfig_subdirs = ['gstreamer-1.0']

meson_pkg_config_file_fixup_script = find_program('scripts/meson-pkg-config-file-fixup.py')

python3 = import('python').find_installation()
subdir('gst-libs')
subdir('gst')
subdir('ext')
subdir('sys')
subdir('tools')
subdir('tests')

# xgettext is optional (on Windows for instance)
if find_program('xgettext', required : get_option('nls')).found()
  subdir('po')
endif
subdir('docs')
subdir('scripts')

base_libraries = ['allocators', 'app', 'audio', 'fft', 'pbutils', 'riff', 'rtp', 'rtsp', 'sdp', 'tag', 'video']
if build_gstgl
  core_conf.set('HAVE_GL', 1)
  base_libraries += 'gl'
endif

pkgconfig_plugins_base_libs_variables = [
  'libraries=' + ' '.join(base_libraries),
]

pkgconfig.generate(
  libraries : [gst_dep],
  variables : pkgconfig_variables + pkgconfig_plugins_base_libs_variables,
  uninstalled_variables : pkgconfig_plugins_base_libs_variables,
  subdirs : pkgconfig_subdirs,
  name : 'gstreamer-plugins-base-1.0',
  description : 'Streaming media framework, base plugins libraries',
)

# Desperate times, desperate measures... fix up escaping of our variables
run_command(meson_pkg_config_file_fixup_script,
  'gstreamer-plugins-base-1.0', 'libraries',
  check: true)

if have_orcc
  update_orc_dist_files = find_program('scripts/update-orc-dist-files.py')

  orc_update_targets = []
  foreach t : orc_targets
    orc_name = t.get('name')
    orc_file = t.get('orc-source')
    header = t.get('header')
    source = t.get('source')
    # alias_target() only works with build targets, so can't use run_target() here
    orc_update_targets += [
      custom_target('update-orc-@0@'.format(orc_name),
        input: [header, source],
        command: [update_orc_dist_files, orc_file, header, source],
        output: ['@0@-dist.c'.format(orc_name)]) # not entirely true
    ]
  endforeach

  if orc_update_targets.length() > 0
    update_orc_dist_target = alias_target('update-orc-dist', orc_update_targets)
  endif
endif

# Set release date
if gst_version_nano == 0
  extract_release_date = find_program('scripts/extract-release-date-from-doap-file.py')
  run_result = run_command(extract_release_date, gst_version, files('gst-plugins-base.doap'), check: true)
  release_date = run_result.stdout().strip()
  core_conf.set_quoted('GST_PACKAGE_RELEASE_DATETIME', release_date)
  message('Package release date: ' + release_date)
endif

if gio_dep.version().version_compare('< 2.67.4')
  core_conf.set('g_memdup2(ptr,sz)', '(G_LIKELY(((guint64)(sz)) < G_MAXUINT)) ? g_memdup(ptr,sz) : (g_abort(),NULL)')
endif

# Use core_conf after all subdirs have set values
configure_file(output : 'config.h', configuration : core_conf)

meson.add_dist_script('scripts/gen-changelog.py', meson.project_name(), '1.22.0', meson.project_version())

plugin_names = []
gst_plugins = []
foreach plugin: plugins
  pkgconfig.generate(plugin, install_dir: plugins_pkgconfig_install_dir)
  dep = declare_dependency(link_with: plugin, variables: {'full_path': plugin.full_path()})
  meson.override_dependency(plugin.name(), dep)
  gst_plugins += [dep]
  if plugin.name().startswith('gst')
    plugin_names += [plugin.name().substring(3)]
  else
    plugin_names += [plugin.name()]
  endif
endforeach

summary({
    'Plugins': plugin_names,
}, list_sep: ', ')
