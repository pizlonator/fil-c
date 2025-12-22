amf_sources = [
  'gstamfencoder.cpp',
  'gstamfh264enc.cpp',
  'gstamfh265enc.cpp',
  'gstamfav1enc.cpp',
  'gstamfutils.cpp',
  'plugin.cpp',
]

amf_option = get_option('amfcodec')
if amf_option.disabled()
  subdir_done()
endif

platform_deps = []
extra_args = ['-DGST_USE_UNSTABLE_API']
extra_cpp_args = []
if host_system == 'windows'
  if not gstd3d11_dep.found()
    if amf_option.enabled()
      error('The amf was enabled explicitly, but required d3d11 was not found')
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
    if amf_option.enabled()
      error('amf plugin does not support UWP')
    else
      subdir_done()
    endif
  endif

  # Encoder needs to do sleep() by API design
  winmm_lib = cc.find_library('winmm', required: amf_option)
  if not winmm_lib.found() or not cc.has_header('mmsystem.h')
    if amf_option.enabled()
      error('Required winmm dependency is not available')
    else
      subdir_done()
    endif
  endif

  platform_deps += [gstd3d11_dep, winmm_lib]
else
  if amf_option.enabled()
    error('amf plugin supports only Windows')
  else
    subdir_done()
  endif
endif

if cxx.get_id() != 'msvc'
  extra_cpp_args = cxx.get_supported_arguments([
    # and MinGW 32bits compiler seems to be complaining about redundant-decls
    '-Wno-redundant-decls',
    # Various SDK header issue
    '-Wno-class-memaccess',
    '-Wno-attributes',
    '-Wno-format',
    '-Wno-format-nonliteral',
    '-Wno-format-security',
  ])
endif

gstamfcodec = library('gstamfcodec',
  amf_sources,
  c_args : gst_plugins_bad_args + extra_args,
  cpp_args : gst_plugins_bad_args + extra_args + extra_cpp_args,
  include_directories : [configinc, include_directories('include')],
  dependencies : [gstbase_dep, gstvideo_dep, gstpbutils_dep, gstcodecparsers_dep, gmodule_dep] + platform_deps,
  install : true,
  install_dir : plugins_install_dir,
)

plugins += [gstamfcodec]
