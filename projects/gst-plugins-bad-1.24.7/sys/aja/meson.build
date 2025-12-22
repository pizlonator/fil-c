aja_option = get_option('aja') \
    .require(host_system == 'linux',
             error_message: 'AJA plugin is currently only available on Linux')
if aja_option.disabled()
  subdir_done()
endif

common_flags = [
  '-DAJALinux=1',
  '-DAJA_LINUX=1',
]

thread_dep = dependency('threads')
rt_dep = cxx.find_library('rt', required : false)

aja_sdk_dir = get_option('aja-sdk-dir')
if aja_sdk_dir == ''
  ajantv2_dep = dependency('libajantv2',
                           include_type: 'system',
                           required: aja_option,
                           allow_fallback: true,
                           default_options: ['warning_level=0'])
  if not ajantv2_dep.found()
    subdir_done()
  endif
  aja_includedirs = []
else
  aja_includedirs = include_directories(
    f'@aja_sdk_dir@/ajalibraries',
    f'@aja_sdk_dir@/ajalibraries/ajantv2/includes',
    f'@aja_sdk_dir@/ajalibraries/ajantv2/src/lin',
    is_system: true,
  )

  message(f'Looking for AJA SDK in @aja_sdk_dir@')
  if not cxx.has_header('ajabase/common/videotypes.h',
                        include_directories: aja_includedirs,
                        required: aja_option)
    subdir_done()
  endif

  ajantv2_lib = cxx.find_library('ajantv2',
    # If the header is found, this should also be
    required : true,
    dirs : [f'@aja_sdk_dir@/lib'],
  )
  ajantv2_dep = declare_dependency(
    dependencies: ajantv2_lib,
    include_directories: aja_includedirs)
endif

gstaja = library('gstaja',
  ['plugin.cpp',
   'gstajacommon.cpp',
   'gstajasink.cpp',
   'gstajasinkcombiner.cpp',
   'gstajasrc.cpp',
   'gstajasrcdemux.cpp',
   'gstajadeviceprovider.cpp',
  ],
  override_options : ['cpp_std=c++11'],
  cpp_args : gst_plugins_bad_args + common_flags,
  include_directories : [configinc],
  link_args : noseh_link_args,
  dependencies : [gstvideo_dep, gstaudio_dep, gstbase_dep, gst_dep, ajantv2_dep, thread_dep, rt_dep],
  install : true,
  install_dir : plugins_install_dir,
)

plugins += [gstaja]
