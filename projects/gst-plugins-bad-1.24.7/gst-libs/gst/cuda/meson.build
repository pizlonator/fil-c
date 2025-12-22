cuda_sources = files([
  'gstcudabufferpool.cpp',
  'gstcudacontext.cpp',
  'gstcudaloader.cpp',
  'gstcudamemory.cpp',
  'gstcudanvrtc.cpp',
  'gstcudastream.cpp',
  'gstcudautils.cpp',
])

cuda_headers = files([
  'cuda-prelude.h',
  'gstcuda.h',
  'gstcudabufferpool.h',
  'gstcudacontext.h',
  'gstcudaloader.h',
  'gstcudamemory.h',
  'gstcudanvrtc.h',
  'gstcudastream.h',
  'gstcudautils.h',
])

gstcuda_dep = dependency('', required : false)
cuda_stubinc = include_directories('./stub')
gstcuda_stub_dep = declare_dependency(
  include_directories: cuda_stubinc
)

gstcuda_platform_dep = []
if host_system not in ['windows', 'linux']
  subdir_done()
endif

# Linux ARM would need -latomic for std::atomic<int64_t>
if host_system == 'linux' and host_machine.cpu_family() not in ['x86', 'x86_64']
  libatomic_dep = cxx.find_library('atomic', required: false)
  if not libatomic_dep.found()
    subdir_done()
  endif

  gstcuda_platform_dep += [libatomic_dep]
endif

cuda_win32_headers = [
  'initguid.h',
  'd3d11.h',
  'dxgi.h',
]

extra_args = ['-DGST_USE_UNSTABLE_API',
              '-DBUILDING_GST_CUDA',
              '-DG_LOG_DOMAIN="GStreamer-Cuda"']

if gstgl_dep.found()
  extra_args += ['-DHAVE_CUDA_GST_GL']
endif

if host_system == 'windows'
  foreach h : cuda_win32_headers
    if not cc.has_header(h)
      subdir_done()
    endif
  endforeach

  if not gstd3d11_dep.found()
    subdir_done()
  endif

  # ConvertStringSecurityDescriptorToSecurityDescriptorA
  advapi32_lib = cxx.find_library('advapi32', required: false)
  if not advapi32_lib.found()
    subdir_done()
  endif

  gstcuda_platform_dep += [advapi32_lib]

  # MinGW 32bits build workaround
  if cc.get_id() != 'msvc'
    extra_args += cc.get_supported_arguments([
      '-Wno-redundant-decls',
    ])
  endif
endif

pkg_name = 'gstreamer-cuda-' + api_version
gstcuda= library('gstcuda-' + api_version,
  cuda_sources,
  c_args : gst_plugins_bad_args + extra_args,
  cpp_args : gst_plugins_bad_args + extra_args,
  include_directories : [configinc, libsinc, cuda_stubinc],
  version : libversion,
  soversion : soversion,
  install : true,
  dependencies : [gstbase_dep, gmodule_dep, gstvideo_dep, gstglproto_dep, gstd3d11_dep, gstcuda_platform_dep]
)

gen_sources = []
library_def = {'lib': gstcuda}
if build_gir
  gir_includes = ['Gst-1.0', 'GstBase-1.0', 'GstVideo-1.0', 'CudaGst-1.0']
  if gstglproto_dep.found()
    gir_includes += ['GstGL-1.0']
  endif
  cuda_gir = {
    'sources' : files('stub/cuda.h', 'stub/cudaGL.h'),
    'namespace' : 'CudaGst',
    'nsversion' : api_version,
    'identifier_prefix' : 'CU',
    'symbol_prefix' : ['cu', 'cuda'],
    'includes' : [],
    'install' : true,
    'extra_args' : [],
    'dependencies' : [],
  }
  gir = {
    'sources' : cuda_sources + cuda_headers,
    'namespace' : 'GstCuda',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : gir_includes,
    'install' : true,
    'extra_args' : gir_init_section + ['-DGST_USE_UNSTABLE_API', '-I' + meson.current_source_dir() / 'stub'],
    'dependencies' : [gstbase_dep, gstvideo_dep, gstglproto_dep],
  }
  if not static_build
    cudagst_gir = gnome.generate_gir(gstcuda, kwargs: cuda_gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [cudagst_gir]}

    gir += {'includes': gir['includes'] + [cudagst_gir[0]]}
    gst_cuda_gir = gnome.generate_gir(gstcuda, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [gst_cuda_gir]}
    gen_sources += gst_cuda_gir
  endif

  library_def += {'gir': [gir, cuda_gir]}
endif
gst_libraries += [[pkg_name, library_def]]

pkgconfig.generate(gstcuda,
  libraries : [gstbase_dep, gmodule_dep, gstvideo_dep, gstglproto_dep, gstcuda_platform_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'Unstable library to work with CUDA inside GStreamer',
)

install_headers(cuda_headers + ['cuda-gst.h', 'gstcuda.h'], subdir : 'gstreamer-1.0/gst/cuda')
gstcuda_dep = declare_dependency(link_with : gstcuda,
  include_directories : [libsinc],
  dependencies : [gstbase_dep, gmodule_dep, gstvideo_dep, gstglproto_dep, gstcuda_platform_dep],
  sources: gen_sources)

meson.override_dependency(pkg_name, gstcuda_dep)
