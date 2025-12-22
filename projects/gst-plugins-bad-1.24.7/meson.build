project('gst-plugins-bad', 'c', 'cpp',
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
gst_version_is_dev = gst_version_minor.is_odd() and gst_version_micro < 90

glib_req = '>= 2.64.0'
orc_req = '>= 0.4.17'

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
cxx = meson.get_compiler('cpp')
host_system = host_machine.system()

if host_system in ['ios', 'darwin']
  have_objc = add_languages('objc', native: false)
  have_objcpp = add_languages('objcpp', native: false)
else
  have_objc = false
  have_objcpp = false
endif

cdata = configuration_data()
cdata.set('ENABLE_NLS', 1)

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
      '/wd5051', # attribute 'attribute-name' requires at least 'standard-level'; ignored
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
  if cxx.has_argument('-Wno-non-virtual-dtor')
    add_project_arguments('-Wno-non-virtual-dtor', language: 'cpp')
  endif

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
if cxx.has_link_argument('-Wl,-z,nodelete')
  add_project_link_arguments('-Wl,-z,nodelete', language: 'cpp')
endif

# Symbol visibility
if cc.has_argument('-fvisibility=hidden')
  add_project_arguments('-fvisibility=hidden', language: 'c')
  add_project_arguments('-fvisibility=hidden', language: 'cpp')
  if have_objc
    add_project_arguments('-fvisibility=hidden', language: 'objc')
  endif
endif

# Disable strict aliasing
if cc.has_argument('-fno-strict-aliasing')
  add_project_arguments('-fno-strict-aliasing', language: 'c')
endif
if cxx.has_argument('-fno-strict-aliasing')
  add_project_arguments('-fno-strict-aliasing', language: 'cpp')
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

check_headers = [
  ['HAVE_DLFCN_H', 'dlfcn.h'],
  ['HAVE_FCNTL_H', 'fcntl.h'],
  ['HAVE_INTTYPES_H', 'inttypes.h'],
  ['HAVE_MEMORY_H', 'memory.h'],
  ['HAVE_NETINET_IN_H', 'netinet/in.h'],
  ['HAVE_NETINET_IP_H', 'netinet/ip.h'],
  ['HAVE_NETINET_TCP_H', 'netinet/tcp.h'],
  ['HAVE_PTHREAD_H', 'pthread.h'],
  ['HAVE_STDINT_H', 'stdint.h'],
  ['HAVE_STDLIB_H', 'stdlib.h'],
  ['HAVE_STRINGS_H', 'strings.h'],
  ['HAVE_STRING_H', 'string.h'],
  ['HAVE_SYS_PARAM_H', 'sys/param.h'],
  ['HAVE_SYS_SOCKET_H', 'sys/socket.h'],
  ['HAVE_SYS_STAT_H', 'sys/stat.h'],
  ['HAVE_SYS_TIME_H', 'sys/time.h'],
  ['HAVE_SYS_TYPES_H', 'sys/types.h'],
  ['HAVE_SYS_UTSNAME_H', 'sys/utsname.h'],
  ['HAVE_UNISTD_H', 'unistd.h'],
  ['HAVE_WINDOWS_H', 'windows.h'],
  ['HAVE_WINSOCK2_H', 'winsock2.h'],
  ['HAVE_WS2TCPIP_H', 'ws2tcpip.h'],
]

foreach h : check_headers
  if cc.has_header(h.get(1))
    cdata.set(h.get(0), 1)
  endif
endforeach

check_functions = [
  ['HAVE_DCGETTEXT', 'dcgettext'],
  ['HAVE_GETPAGESIZE', 'getpagesize'],
  ['HAVE_GMTIME_R', 'gmtime_r'],
  ['HAVE_MEMFD_CREATE', 'memfd_create'],
  ['HAVE_MMAP', 'mmap'],
  ['HAVE_PIPE2', 'pipe2'],
  ['HAVE_GETRUSAGE', 'getrusage', '#include<sys/resource.h>'],
]

foreach f : check_functions
  prefix = ''
  if f.length() == 3
    prefix = f.get(2)
  endif
  if cc.has_function(f.get(1), prefix: prefix)
    cdata.set(f.get(0), 1)
  endif
endforeach

cdata.set('SIZEOF_CHAR', cc.sizeof('char'))
cdata.set('SIZEOF_INT', cc.sizeof('int'))
cdata.set('SIZEOF_LONG', cc.sizeof('long'))
cdata.set('SIZEOF_SHORT', cc.sizeof('short'))
cdata.set('SIZEOF_VOIDP', cc.sizeof('void*'))

cdata.set_quoted('VERSION', gst_version)
cdata.set_quoted('PACKAGE', 'gst-plugins-bad')
cdata.set_quoted('PACKAGE_VERSION', gst_version)
cdata.set_quoted('PACKAGE_BUGREPORT', 'https://gitlab.freedesktop.org/gstreamer/gstreamer/issues/new')
cdata.set_quoted('PACKAGE_NAME', 'GStreamer Bad Plug-ins')
cdata.set_quoted('GETTEXT_PACKAGE', 'gst-plugins-bad-1.0')
cdata.set_quoted('GST_API_VERSION', api_version)
cdata.set_quoted('GST_LICENSE', 'LGPL')
cdata.set_quoted('LIBDIR', join_paths(get_option('prefix'), get_option('libdir')))
cdata.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))

warning_flags = [
  '-Wmissing-declarations',
  '-Wredundant-decls',
  '-Wwrite-strings',
  '-Wformat',
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
  '-Wold-style-definition',
]

warning_cxx_flags = [
  '-Wformat-nonliteral',
]

foreach extra_arg : warning_c_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
endforeach

foreach extra_arg : warning_cxx_flags
  if cxx.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'cpp')
  endif
endforeach

foreach extra_arg : warning_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
  if cxx.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'cpp')
  endif
endforeach

# GStreamer package name and origin url
gst_package_name = get_option('package-name')
if gst_package_name == ''
  if gst_version_nano == 0
    gst_package_name = 'GStreamer Bad Plug-ins source release'
  elif gst_version_nano == 1
    gst_package_name = 'GStreamer Bad Plug-ins git'
  else
    gst_package_name = 'GStreamer Bad Plug-ins prerelease'
  endif
endif
cdata.set_quoted('GST_PACKAGE_NAME', gst_package_name)
cdata.set_quoted('GST_PACKAGE_ORIGIN', get_option('package-origin'))

# FIXME: This should be exposed as a configuration option
if host_system == 'linux'
  cdata.set_quoted('DEFAULT_VIDEOSRC', 'v4l2src')
elif ['darwin', 'ios'].contains(host_system)
  cdata.set_quoted('DEFAULT_VIDEOSRC', 'avfvideosrc')
  cdata.set_quoted('GST_EXTRA_MODULE_SUFFIX', '.dylib')
  # Yes, we set this for iOS too. Same as Autotools.
  cdata.set('HAVE_OSX', 1)
else
  cdata.set_quoted('DEFAULT_VIDEOSRC', 'videotestsrc')
endif

# Mandatory GST deps
gst_dep = dependency('gstreamer-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_dep'])
gstbase_dep = dependency('gstreamer-base-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_base_dep'])
gstnet_dep = dependency('gstreamer-net-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_net_dep'])
gstcontroller_dep = dependency('gstreamer-controller-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_controller_dep'])

gstpbutils_dep = dependency('gstreamer-pbutils-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'pbutils_dep'])
gstallocators_dep = dependency('gstreamer-allocators-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'allocators_dep'])
gstapp_dep = dependency('gstreamer-app-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'app_dep'])
gstaudio_dep = dependency('gstreamer-audio-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'audio_dep'])
gstfft_dep = dependency('gstreamer-fft-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'fft_dep'])
gstriff_dep = dependency('gstreamer-riff-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'riff_dep'])
gstrtp_dep = dependency('gstreamer-rtp-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'rtp_dep'])
gstrtsp_dep = dependency('gstreamer-rtsp-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'rtsp_dep'])
gstsdp_dep = dependency('gstreamer-sdp-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'sdp_dep'])
gsttag_dep = dependency('gstreamer-tag-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'tag_dep'])
gstvideo_dep = dependency('gstreamer-video-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'video_dep'])
gstcheck_dep = dependency('gstreamer-check-1.0', version : gst_req,
    required : get_option('tests'),
    fallback : ['gstreamer', 'gst_check_dep'])

# GStreamer OpenGL
gstgl_dep = dependency('gstreamer-gl-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'gstgl_dep'], required: get_option('gl'))
gstglproto_dep = dependency('gstreamer-gl-prototypes-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'gstglproto_dep'], required: get_option('gl'))
gstglx11_dep = dependency('', required : false)
gstglwayland_dep = dependency('', required : false)
gstglegl_dep = dependency('', required : false)

if gstgl_dep.found()
  if gstgl_dep.type_name() == 'pkgconfig'
    gst_gl_apis = gstgl_dep.get_variable('gl_apis').split()
    gst_gl_winsys = gstgl_dep.get_variable('gl_winsys').split()
    gst_gl_platforms = gstgl_dep.get_variable('gl_platforms').split()
  else
    gstbase = subproject('gst-plugins-base')
    gst_gl_apis = gstbase.get_variable('enabled_gl_apis')
    gst_gl_winsys = gstbase.get_variable('enabled_gl_winsys')
    gst_gl_platforms = gstbase.get_variable('enabled_gl_platforms')
  endif

  message('GStreamer OpenGL window systems: @0@'.format(' '.join(gst_gl_winsys)))
  message('GStreamer OpenGL platforms: @0@'.format(' '.join(gst_gl_platforms)))
  message('GStreamer OpenGL apis: @0@'.format(' '.join(gst_gl_apis)))

  foreach ws : ['x11', 'wayland', 'android', 'cocoa', 'eagl', 'win32', 'dispmanx']
    set_variable('gst_gl_have_window_@0@'.format(ws), gst_gl_winsys.contains(ws))
  endforeach
  # Handling viv-fb separately, because the winsys is called "viv-fb", but the
  # variable suffix must be "viv_fb" (dashes are not allowed in variable names).
  set_variable('gst_gl_have_window_viv_fb', gst_gl_winsys.contains('viv-fb'))

  foreach p : ['glx', 'egl', 'cgl', 'eagl', 'wgl']
    set_variable('gst_gl_have_platform_@0@'.format(p), gst_gl_platforms.contains(p))
  endforeach

  foreach api : ['gl', 'gles2']
    set_variable('gst_gl_have_api_@0@'.format(api), gst_gl_apis.contains(api))
  endforeach

  # Behind specific checks because meson fails at optional dependencies with a
  # fallback to the same subproject.  On the first failure, meson will never
  # check the system again even if the fallback never existed.
  # Last checked with meson 0.54.3
  if gst_gl_have_window_x11
    gstglx11_dep = dependency('gstreamer-gl-x11-1.0', version : gst_req,
        fallback : ['gst-plugins-base', 'gstglx11_dep'], required: true)
  endif
  if gst_gl_have_window_wayland
    gstglwayland_dep = dependency('gstreamer-gl-wayland-1.0', version : gst_req,
        fallback : ['gst-plugins-base', 'gstglwayland_dep'], required: true)
  endif
  if gst_gl_have_platform_egl
    gstglegl_dep = dependency('gstreamer-gl-egl-1.0', version : gst_req,
        fallback : ['gst-plugins-base', 'gstglegl_dep'], required: true)
  endif
  if gst_gl_have_window_viv_fb
    gstglviv_fb_dep = dependency('gstreamer-gl-viv-fb-1.0', version : gst_req,
        fallback : ['gst-plugins-base', 'gstglviv_fb_dep'], required: true)
  endif
endif

libm = cc.find_library('m', required : false)
gio_dep = dependency('gio-2.0', version: glib_req)
gmodule_dep = dependency('gmodule-no-export-2.0')
# gio-unix-2.0 is used by sys/bluez

# Optional dep of ext/gl and gst/librfb
x11_dep = dependency('x11', required : get_option('x11'))
if x11_dep.found()
  cdata.set('HAVE_X11', 1)
endif

#
# Solaris and Illumos distros split a lot of networking-related code
# into '-lsocket -lnsl'.  Anything that calls socketpair(), getifaddr(),
# etc. probably needs to include network_deps
#
if host_machine.system() == 'sunos'
  network_deps = [
    cc.find_library('socket', required: false),
    cc.find_library('nsl',    required: false)
  ]
else
  network_deps = []
endif

if host_machine.system() == 'windows'
  winsock2 = [cc.find_library('ws2_32')]

  building_for_win7 = cc.compiles('''#include <windows.h>
      #ifndef WINVER
      #error "unknown minimum supported OS version"
      #endif
      #if (WINVER < _WIN32_WINNT_WIN7)
      #error "Windows 7 API is not guaranteed"
      #endif
      ''',
      name: 'building for Windows 7')

  if not building_for_win7
    add_project_arguments([
      '-D_WIN32_WINNT=_WIN32_WINNT_WIN7',
      '-DWINVER=_WIN32_WINNT_WIN7',
    ], language: ['c', 'cpp'])
  endif
else
  winsock2 = []
endif

if ['darwin', 'ios'].contains(host_system)
  if not have_objc
    error('Building on MacOS/iOS/etc requires an ObjC compiler')
  endif
  if host_system == 'ios'
    cdata.set('HAVE_IOS', 1)
  endif

  avfoundation_dep = dependency('AVFoundation', required : false)
  if avfoundation_dep.found()
      cdata.set('HAVE_AVFOUNDATION', 1)
  endif

  videotoolbox_dep = dependency('VideoToolbox', required : false)
  if videotoolbox_dep.found()
      cdata.set('HAVE_VIDEOTOOLBOX', 1)
  endif

#  FIXME: framework.version() returns 'unknown'
#  if videotoolbox_dep.version().version_compare('>=10.9.6')
#    cdata.set('HAVE_VIDEOTOOLBOX_10_9_6', 1)
#  endif
endif

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
  cdata.set('HAVE_ORC', 1)
else
  message('Orc Compiler not found or disabled, will use backup C code')
  cdata.set('DISABLE_ORC', 1)
endif
cdata.set('GST_ENABLE_EXTRA_CHECKS', not get_option('extra-checks').disabled())

# Disable compiler warnings for unused variables and args if gst debug system is disabled
if gst_dep.type_name() == 'internal'
  gst_debug_disabled = not subproject('gstreamer').get_variable('gst_debug')
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
    add_project_arguments(msvc_args, language: ['c', 'cpp'])
  else
    if cc.has_argument('-Wno-unused')
      add_project_arguments('-Wno-unused', language: 'c')
    endif
    if cxx.has_argument ('-Wno-unused')
      add_project_arguments('-Wno-unused', language: 'cpp')
    endif
  endif
else
  if cc.get_argument_syntax() == 'msvc' and gst_version_is_dev
    msvc_args = cc.get_supported_arguments([
      '/we4101', # 'identifier' : unreferenced local variable
      '/we4189', # 'identifier' : local variable is initialized but not referenced
    ])
    add_project_arguments(msvc_args, language: ['c', 'cpp'])
  endif
  message('GStreamer debug system is enabled')
endif

gst_plugins_bad_args = ['-DHAVE_CONFIG_H']
configinc = include_directories('.')
libsinc = include_directories('gst-libs')

python3 = import('python').find_installation()

gir = find_program('g-ir-scanner', required : get_option('introspection'))
gnome = import('gnome')
build_gir = gir.found() and (not meson.is_cross_build() or get_option('introspection').enabled())
gir_init_section = [ '--add-init-section=extern void gst_init(gint*,gchar**);' + \
    'g_setenv("GST_REGISTRY_1.0", "@0@", TRUE);'.format(meson.current_build_dir() + '/gir_empty_registry.reg') + \
    'g_setenv("GST_PLUGIN_PATH_1_0", "", TRUE);' + \
    'g_setenv("GST_PLUGIN_SYSTEM_PATH_1_0", "", TRUE);' + \
    'gst_init(NULL,NULL);', '--quiet']

presetdir = join_paths(get_option('datadir'), 'gstreamer-' + api_version, 'presets')

pkgconfig = import('pkgconfig')
plugins_pkgconfig_install_dir = join_paths(plugins_install_dir, 'pkgconfig')
if get_option('default_library') == 'shared'
  # If we don't build static plugins there is no need to generate pc files
  plugins_pkgconfig_install_dir = disabler()
endif

pkgconfig_variables = ['exec_prefix=${prefix}',
    'toolsdir=${exec_prefix}/bin',
    'pluginsdir=${libdir}/gstreamer-1.0',
    'datarootdir=${prefix}/share',
    'girdir=${datadir}/gir-1.0',
    'typelibdir=${libdir}/girepository-1.0']

pkgconfig_subdirs = ['gstreamer-1.0']

pkgconfig.generate(
  libraries : [gst_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : 'gstreamer-plugins-bad-1.0',
  description : 'Streaming media framework, bad plugins libraries',
)

gpl_allowed = get_option('gpl').allowed()

subdir('gst-libs')
subdir('gst')
subdir('sys')
subdir('ext')
subdir('tests')
subdir('data')
subdir('tools')

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

# xgettext is optional (on Windows for instance)
if find_program('xgettext', required : get_option('nls')).found()
  subdir('po')
endif

subdir('scripts')

# Set release date
if gst_version_nano == 0
  extract_release_date = find_program('scripts/extract-release-date-from-doap-file.py')
  run_result = run_command(extract_release_date, gst_version, files('gst-plugins-bad.doap'), check: true)
  release_date = run_result.stdout().strip()
  cdata.set_quoted('GST_PACKAGE_RELEASE_DATETIME', release_date)
  message('Package release date: ' + release_date)
endif

if gio_dep.version().version_compare('< 2.67.4')
  cdata.set('g_memdup2(ptr,sz)', '(G_LIKELY(((guint64)(sz)) < G_MAXUINT)) ? g_memdup(ptr,sz) : (g_abort(),NULL)')
endif

configure_file(output : 'config.h', configuration : cdata)

meson.add_dist_script('scripts/gen-changelog.py', meson.project_name(), '1.22.0', meson.project_version())

subdir('docs')

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
      '(A)GPL license allowed': gpl_allowed,
}, list_sep: ', ')
