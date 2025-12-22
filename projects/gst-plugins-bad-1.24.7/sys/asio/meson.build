asio_sources = [
  'gstasiodeviceprovider.cpp',
  'gstasioobject.cpp',
  'gstasioringbuffer.cpp',
  'gstasiosink.cpp',
  'gstasiosrc.cpp',
  'gstasioutils.cpp',
  'plugin.c',
]

asio_option = get_option('asio')
if asio_option.disabled() or host_system != 'windows'
  subdir_done()
endif

# FIXME: non-msvc is not tested, and unlikely supported yet because of
# tool-chain issue
if cxx.get_id() != 'msvc'
  if asio_option.enabled()
    error('asio plugin can only be built with MSVC')
  else
    subdir_done ()
  endif
endif

winapi_desktop = cxx.compiles('''#include <winapifamily.h>
    #if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    #error "not win32"
    #endif''',
    name: 'building for Win32')

if not winapi_desktop
  if asio_option.enabled()
    error('asio plugin requires WINAPI_PARTITION_DESKTOP')
  else
    subdir_done ()
  endif
endif

avrt_lib = cc.find_library('avrt', required: asio_option)
if not avrt_lib.found()
  subdir_done ()
endif

winmm_lib = cc.find_library('winmm', required: asio_option)
if not winmm_lib.found()
  subdir_done ()
endif

# Checking SDK headers. User should install ASIO sdk on system, and
# this plugin requires asio.h, asiosys.h and iasiodrv.h headers
asio_sdk_root = get_option ('asio-sdk-path')
if asio_sdk_root == ''
  if asio_option.enabled()
    error('asio sdk path is needed, pass with -Dasio-sdk-path=C:/path/to/sdk')
  else
    subdir_done ()
  endif
endif

asio_inc_dir = include_directories(join_paths(asio_sdk_root, 'common'), is_system : true)
has_asio_header = cxx.has_header('asio.h', include_directories: asio_inc_dir)
has_asiosys_header = cxx.has_header('asiosys.h', include_directories: asio_inc_dir)
has_iasiodrv_header = cxx.has_header('iasiodrv.h', include_directories: asio_inc_dir)
if not has_asio_header or not has_asiosys_header or not has_iasiodrv_header
  if asio_option.enabled()
    error('Failed to find required SDK header(s)')
  else
    subdir_done ()
  endif
endif

asio_deps = [gstaudio_dep, avrt_lib, winmm_lib]

gstasio = library('gstasio',
    asio_sources,
    include_directories : [configinc, asio_inc_dir],
    dependencies : asio_deps,
    c_args : gst_plugins_bad_args,
    cpp_args : gst_plugins_bad_args,
    install : true,
    install_dir : plugins_install_dir)
plugins += [gstasio]