dshow_sources = [
  'dshowdeviceprovider.cpp',
  'dshowvideofakesrc.cpp',
  'dshowvideosink.cpp',
  'gstdshow.cpp',
  'gstdshowaudiodec.cpp',
  'gstdshowaudiosrc.cpp',
  'gstdshowfakesink.cpp',
  'gstdshowfakesrc.cpp',
  'gstdshowutil.cpp',
  'gstdshowvideodec.cpp',
  'gstdshowvideosrc.cpp',
  'plugin.cpp',
]

dshow_option = get_option('directshow')

if cxx.get_id() != 'msvc' or dshow_option.disabled()
  if get_option('directshow').enabled()
    error('directshow plugins can only be built with MSVC')
  endif
  subdir_done()
endif

if host_system == 'windows'
  # Check whether we're building for UWP apps
  code = '''
  #include <windows.h>
  #if !(WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) && !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP))
  #error "Not building for UWP"
  #endif'''
  if cxx.compiles(code, name: 'building for UWP')
    if dshow_option.enabled()
      error('directshow plugins cannot be built for UWP')
    endif
    subdir_done()
  endif
endif

subdir('strmbase')

winmm_lib = cxx.find_library('winmm', required: dshow_option)
dmoguids_lib = cxx.find_library('dmoguids', required: dshow_option)
wmcodecdspuuid_lib = cxx.find_library('wmcodecdspuuid', required: dshow_option)
mfuuid_dep = cxx.find_library('mfuuid', required: dshow_option)
rpcrt4_dep = cxx.find_library('rpcrt4', required: dshow_option)

dshow_deps = [
  strmbase_dep,
  winmm_lib,
  dmoguids_lib,
  wmcodecdspuuid_lib,
  mfuuid_dep,
  rpcrt4_dep
]

foreach dep: dshow_deps
  if not dep.found()
    subdir_done()
  endif
endforeach

gstdirectshow = library('gstdirectshow',
  dshow_sources,
  c_args: gst_plugins_bad_args,
  cpp_args: gst_plugins_bad_args,
  include_directories: [configinc],
  dependencies: [gstvideo_dep, gstaudio_dep] + dshow_deps,
  install: true,
  install_dir: plugins_install_dir,
  override_options: ['cpp_std=none']
)
plugins += [gstdirectshow]
