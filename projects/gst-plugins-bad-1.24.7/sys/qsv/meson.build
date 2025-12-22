qsv_sources = [
  'gstqsvallocator.cpp',
  'gstqsvav1enc.cpp',
  'gstqsvdecoder.cpp',
  'gstqsvencoder.cpp',
  'gstqsvh264dec.cpp',
  'gstqsvh264enc.cpp',
  'gstqsvh265dec.cpp',
  'gstqsvh265enc.cpp',
  'gstqsvjpegdec.cpp',
  'gstqsvjpegenc.cpp',
  'gstqsvutils.cpp',
  'gstqsvvp9dec.cpp',
  'gstqsvvp9enc.cpp',
  'plugin.cpp',
]

qsv_d3d11_sources = [
  'gstqsvallocator_d3d11.cpp',
]

qsv_va_sources = [
  'gstqsvallocator_va.cpp',
]

extra_args = [
  '-DGST_USE_UNSTABLE_API',
]

qsv_option = get_option('qsv')
if qsv_option.disabled()
  subdir_done()
endif

qsv_platform_deps = []
if host_system == 'windows'
  if not gstd3d11_dep.found()
    if qsv_option.enabled()
      error('The qsv was enabled explicitly, but required d3d11 was not found')
    else
      subdir_done()
    endif
  endif

  code = '''
  #include <windows.h>
  #if !(WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) && !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP))
  #error "Not building for UWP"
  #endif'''
  if cc.compiles(code, name : 'building for UWP')
    if qsv_option.enabled()
      error('qsv plugin does not support UWP')
    else
      subdir_done()
    endif
  endif

  qsv_sources += qsv_d3d11_sources
  qsv_platform_deps += [gstd3d11_dep]
elif host_system == 'linux'
  if not gstva_dep.found()
    if qsv_option.enabled()
      error('The qsv was enabled explicitly, but required va was not found')
    else
      subdir_done()
    endif
  endif
  qsv_sources += qsv_va_sources
  qsv_platform_deps += [gstva_dep]
else
  if qsv_option.enabled()
    error('QSV plugin supports only Windows or Linux')
  else
    subdir_done()
  endif
endif

# suppress deprecated use of MFXInitEx. We don't use the method,
# but used in "mfxvideo++.h"
# and MinGW 32bits compiler seems to be complaining about redundant-decls
if cc.get_id() != 'msvc'
  extra_args += cc.get_supported_arguments([
    '-Wno-redundant-decls',
    '-Wno-deprecated-declarations',
  ])
endif

subdir('libmfx')

gstqsv = library('gstqsv',
  qsv_sources,
  c_args : gst_plugins_bad_args + extra_args,
  cpp_args : gst_plugins_bad_args + extra_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep, gstcodecparsers_dep, libmfx_internal_dep] + qsv_platform_deps,
  install : true,
  install_dir : plugins_install_dir,
)

plugins += [gstqsv]
