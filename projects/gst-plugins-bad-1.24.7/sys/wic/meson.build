wic_sources = [
  'gstwicdecoder.cpp',
  'gstwicimagingfactory.cpp',
  'gstwicjpegdec.cpp',
  'gstwicpngdec.cpp',
  'gstwicutils.cpp',
  'plugin.cpp',
]

extra_args = []
wic_deps = []

wic_option = get_option('wic')
if host_system != 'windows' or wic_option.disabled()
  subdir_done()
endif

if cc.get_id() != 'msvc'
  if wic_option.enabled()
    error('wic plugin can only be built with MSVC')
  endif
  subdir_done()
endif

windowscodecs_lib = cc.find_library('windowscodecs', required : wic_option)
have_wic = windowscodecs_lib.found() and cc.has_header('wincodec.h') and cc.has_header('wincodecsdk.h')
if not have_wic
  if wic_option.enabled()
    error('The wic plugin was enabled explicitly, but required libraries were not found.')
  endif
  subdir_done()
endif

wic_deps += [windowscodecs_lib]

win10_sdk = cxx.compiles('''#include <windows.h>
    #ifndef WDK_NTDDI_VERSION
    #error "unknown Windows SDK version"
    #endif
    #if (WDK_NTDDI_VERSION < 0x0A000000)
    #error "Not a Windows 10 SDK"
    #endif
    ''',
    name: 'building with Windows 10 SDK')

if not win10_sdk
  if wic_option.enabled()
    error('wic plugin was enabled explicitly, but Windows 10 SDK is unavailable')
  else
    subdir_done()
  endif
endif

building_for_win10 = cxx.compiles('''#include <windows.h>
    #ifndef WINVER
    #error "unknown minimum supported OS version"
    #endif
    #if (WINVER < 0x0A00)
    #error "Windows 10 API is not guaranteed"
    #endif
    ''',
    name: 'building for Windows 10')

if not building_for_win10
  message('Bumping target Windows version to Windows 10 for building wic plugin')
  extra_args += ['-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-DNTDDI_VERSION=WDK_NTDDI_VERSION']
endif

gstwic = library('gstwic',
  wic_sources,
  c_args : gst_plugins_bad_args + extra_args,
  cpp_args : gst_plugins_bad_args + extra_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep] + wic_deps,
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstwic]
