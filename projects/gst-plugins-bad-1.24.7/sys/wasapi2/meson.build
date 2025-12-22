wasapi2_sources = [
  'gstwasapi2src.c',
  'gstwasapi2sink.c',
  'gstwasapi2util.c',
  'gstwasapi2client.cpp',
  'gstwasapi2device.c',
  'gstwasapi2ringbuffer.cpp',
  'plugin.c',
]

mmdeviceapi_symbols = [
  'ActivateAudioInterfaceAsync',
  'DEVINTERFACE_AUDIO_RENDER',
  'DEVINTERFACE_AUDIO_CAPTURE',
]

wasapi2_option = get_option('wasapi2')
if host_system != 'windows'
  if wasapi2_option.disabled()
    subdir_done()
  elif wasapi2_option.enabled()
    error('Cannot build wasapi2 plugin when not building for Windows')
  endif
endif

ole32_dep = cc.find_library('ole32', required : get_option('wasapi2'))
ksuser_dep = cc.find_library('ksuser', required : get_option('wasapi2'))
runtimeobject_dep = cc.find_library('runtimeobject', required : get_option('wasapi2'))
mmdeviceapi_dep = cc.find_library('mmdevapi', required : get_option('wasapi2'))
mfplat_dep = cc.find_library('mfplat', required : get_option('wasapi2'))
wasapi2_dep = [ole32_dep, ksuser_dep, runtimeobject_dep, mmdeviceapi_dep, mfplat_dep]
extra_args = ['-DGST_USE_UNSTABLE_API']

foreach dep: wasapi2_dep
  if not dep.found()
    if wasapi2_option.enabled()
      error('wasapi2 plugin was enabled explicitly, but required dependencies were not found')
    else
      subdir_done()
    endif
  endif
endforeach

if not cxx.has_header_symbol ('audioclient.h', 'IAudioClient3', dependencies : wasapi2_dep)
  if wasapi2_option.enabled()
    error('wasapi2 plugin was enabled explicitly, but IAudioClient3 is unavailable')
  else
    subdir_done()
  endif
endif

foreach symbol: mmdeviceapi_symbols
  if not cxx.has_header_symbol ('mmdeviceapi.h', symbol, dependencies : wasapi2_dep)
    if wasapi2_option.enabled()
      error('wasapi2 plugin was enabled explicitly, but @1@ is unavailable'.format(symbol))
    else
      subdir_done()
    endif
  endif
endforeach

winapi_app = cxx.compiles('''#include <winapifamily.h>
  #include <windows.applicationmodel.core.h>
  #include <wrl.h>
  #include <wrl/wrappers/corewrappers.h>
  #include <audioclient.h>
  #include <mmdeviceapi.h>
  #if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
  #error "not winrt"
  #endif
  int main (int argc, char ** argv) {
    IAudioClient3 *client = NULL;
    return 0;
  } ''',
  dependencies: wasapi2_dep,
  name: 'building for WINAPI_PARTITION_APP')

if not winapi_app
  if wasapi2_option.enabled()
    error('wasapi2 plugin was enabled explicitly, but build target is not include WINAPI_PARTITION_APP')
  else
    subdir_done()
  endif
endif

winapi_desktop = cxx.compiles('''#include <winapifamily.h>
    #if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    #error "not win32"
    #endif''',
    name: 'building for WINAPI_PARTITION_DESKTOP')

if winapi_app and not winapi_desktop
  extra_args += ['-DGST_WASAPI2_WINAPI_ONLY_APP']
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
  if wasapi2_option.enabled()
    error('wasapi2 plugin was enabled explicitly, but Windows 10 SDK is unavailable')
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
  message('Bumping target Windows version to Windows 10 for building wasapi2 plugin')
  extra_args += ['-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-DNTDDI_VERSION=WDK_NTDDI_VERSION']
endif

if not gstwinrt_dep.found()
  if wasapi2_option.enabled()
    error('wasapi2 plugin was enabled explicitly, but GstWinRt library is unavailable')
  else
    subdir_done()
  endif
endif

# Work around for Windows SDK header issue
# https://docs.microsoft.com/en-us/cpp/build/reference/permissive-standards-conformance?view=msvc-160#windows-header-issues
extra_cpp_args = cxx.get_supported_arguments(['/Zc:twoPhase-'])

gstwasapi2 = library('gstwasapi2',
  wasapi2_sources,
  c_args : gst_plugins_bad_args + ['-DCOBJMACROS'] + extra_args,
  cpp_args : gst_plugins_bad_args + extra_args + extra_cpp_args,
  include_directories : [configinc],
  dependencies : [gstaudio_dep, gstwinrt_dep] + wasapi2_dep,
  install : true,
  install_dir : plugins_install_dir)
plugins += [gstwasapi2]
