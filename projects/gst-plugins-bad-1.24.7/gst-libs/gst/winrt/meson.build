winrt_sources = [
  'gstwinrtdevicewatcher.cpp',
]

gstwinrt_dep = dependency('', required : false)

extra_c_args = [
  '-DCOBJMACROS',
]
extra_comm_args = [
  '-DGST_USE_UNSTABLE_API',
  '-DBUILDING_GST_WINRT',
  '-DG_LOG_DOMAIN="GStreamer-WinRT"'
]

if host_system != 'windows'
  subdir_done()
endif

# TODO: Need to bump mingw tool chain
if cxx.get_id() != 'msvc'
  subdir_done()
endif

runtimeobject_lib = cc.find_library('runtimeobject', required : false)
if not runtimeobject_lib.found()
  subdir_done()
endif

winapi_app = cxx.compiles('''#include <winapifamily.h>
  #if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
  #error "not winrt"
  #endif
  int main (int argc, char ** argv) {
    return 0;
  } ''',
  dependencies: runtimeobject_lib,
  name: 'building for WinRT')

if not winapi_app
  subdir_done()
endif

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
  subdir_done()
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
  message('Bumping target Windows version to Windows 10 for building gstwinrt library')
  extra_comm_args += ['-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-DNTDDI_VERSION=WDK_NTDDI_VERSION']
endif

gstwinrt = library('gstwinrt-' + api_version,
  winrt_sources,
  c_args : gst_plugins_bad_args + extra_c_args + extra_comm_args,
  cpp_args : gst_plugins_bad_args + extra_comm_args,
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  install : true,
  dependencies : [gstbase_dep, runtimeobject_lib]
)

# Still non-public api, should not install headers
gstwinrt_dep = declare_dependency(link_with : gstwinrt,
  include_directories : [libsinc],
  dependencies : [gstbase_dep, runtimeobject_lib])
