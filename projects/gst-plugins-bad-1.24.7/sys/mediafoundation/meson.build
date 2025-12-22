mf_sources = [
  'gstmfaacdec.cpp',
  'gstmfaacenc.cpp',
  'gstmfaudiodecoder.cpp',
  'gstmfaudioencoder.cpp',
  'gstmfdevice.cpp',
  'gstmfh264enc.cpp',
  'gstmfh265enc.cpp',
  'gstmfmp3dec.cpp',
  'gstmfmp3enc.cpp',
  'gstmfplatloader.cpp',
  'gstmfsourceobject.cpp',
  'gstmftransform.cpp',
  'gstmfutils.cpp',
  'gstmfvideobuffer.cpp',
  'gstmfvideoencoder.cpp',
  'gstmfvideosrc.cpp',
  'gstmfvp9enc.cpp',
  'plugin.cpp',
]

mf_desktop_sources = [
  'gstmfcapturedshow.cpp',
  'gstmfsourcereader.cpp',
  'gstwin32devicewatcher.cpp',
]

mf_app_sources = [
  'gstmfcapturewinrt.cpp',
  'mediacapturewrapper.cpp',
]

mf_header_deps = [
  'mfidl.h',
  'mfapi.h',
  'mfreadwrite.h',
  'mferror.h',
  'strmif.h',
  'mfobjects.h',
  'codecapi.h',
]

winapi_desktop = false
winapi_app = false
have_capture_engine = false
have_mf_d3d11 = false
mf_lib_deps = []
mf_config = configuration_data()
extra_c_args = ['-DCOBJMACROS']
extra_cpp_args = []

mf_option = get_option('mediafoundation')
if host_system != 'windows' or mf_option.disabled()
  subdir_done()
endif

if cc.get_id() != 'msvc'
  if mf_option.enabled()
    error('mediafoundation plugin can only be built with MSVC')
  endif
  subdir_done()
endif

mf_lib = cc.find_library('mf', required : mf_option)
mfplat_lib = cc.find_library('mfplat', required : mf_option)
mfreadwrite_lib = cc.find_library('mfreadwrite', required : mf_option)
mfuuid_lib = cc.find_library('mfuuid', required : mf_option)
strmiids_lib = cc.find_library('strmiids', required : mf_option)
ole32_dep = cc.find_library('ole32', required : mf_option)
runtimeobject_lib = cc.find_library('runtimeobject', required : false)

have_mf_lib = mf_lib.found() and mfplat_lib.found() and mfreadwrite_lib.found() and mfuuid_lib.found() and strmiids_lib.found() and ole32_dep.found()
if not have_mf_lib
  if mf_option.enabled()
    error('The mediafoundation plugin was enabled explicitly, but required libraries were not found.')
  endif
  subdir_done()
endif

mf_lib_deps += [mf_lib, mfplat_lib, mfreadwrite_lib, mfuuid_lib, strmiids_lib, ole32_dep]

have_mf_header = true
foreach h: mf_header_deps
  if have_mf_header
    have_mf_header = cc.has_header(h)
  endif
endforeach

if not have_mf_header
  if mf_option.enabled()
    error('The mediafoundation plugin was enabled explicitly, but required headers were not found.')
  endif
  subdir_done()
endif

winapi_desktop = cxx.compiles('''#include <winapifamily.h>
    #include <wrl.h>
    #if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    #error "not win32"
    #endif''',
    dependencies: mf_lib_deps,
    name: 'building for Win32')

if runtimeobject_lib.found()
  winapi_app = cxx.compiles('''#include <winapifamily.h>
      #include <windows.applicationmodel.core.h>
      #include <wrl.h>
      #if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
      #error "not winrt"
      #endif
      #ifndef WINVER
      #error "unknown minimum supported OS version"
      #endif
      #if (WINVER < 0x0A00)
      #error "Windows 10 API is not guaranteed"
      #endif''',
      dependencies: [mf_lib_deps, runtimeobject_lib],
      name: 'building for WinRT')
endif

if not winapi_desktop and not winapi_app
  error('Neither Desktop partition nor App partition')
endif

if winapi_app
  if not gstwinrt_dep.found()
    if mf_option.enabled()
      error('The mediafoundation plugin was enabled explicitly, but GstWinRt library is unavailable')
    else
      subdir_done()
    endif
  endif

  mf_sources += mf_app_sources
  mf_lib_deps += [runtimeobject_lib, gstwinrt_dep]
endif

if winapi_desktop
  mf_sources += mf_desktop_sources
  # We need d3d11_4.h header for querying "ExtendedNV12SharedTextureSupported"
  # Since MFTEnum2 is desktop only we don't support d3d11 interop for UWP build
  # And because MFTEnum2 is Windows 10 API, we will load MFTEnum2 symbol
  # by using g_module_open() so that keep supporting old OS versions
  if gstd3d11_dep.found() and cc.has_header('d3d11_4.h') and cc.has_header('d3d10.h')
    have_mf_d3d11 = true
    mf_lib_deps += [gstd3d11_dep]
    extra_c_args += ['-DGST_USE_UNSTABLE_API']
    extra_cpp_args += ['-DGST_USE_UNSTABLE_API']
    message ('Enable D3D11 interop for MediaFoundation plugin')
  endif
endif

mf_config.set10('GST_MF_WINAPI_APP', winapi_app)
mf_config.set10('GST_MF_WINAPI_DESKTOP', winapi_desktop)
mf_config.set10('GST_MF_HAVE_D3D11', have_mf_d3d11)

configure_file(
  output: 'gstmfconfig.h',
  configuration: mf_config,
)

# Work around for Windows SDK header issue
# https://docs.microsoft.com/en-us/cpp/build/reference/permissive-standards-conformance?view=msvc-160#windows-header-issues
extra_cpp_args += cxx.get_supported_arguments(['/Zc:twoPhase-'])

gstmediafoundation = library('gstmediafoundation',
  mf_sources,
  c_args : gst_plugins_bad_args + extra_c_args,
  cpp_args : gst_plugins_bad_args + extra_cpp_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep, gstaudio_dep, gstpbutils_dep, gmodule_dep] + mf_lib_deps,
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstmediafoundation]
