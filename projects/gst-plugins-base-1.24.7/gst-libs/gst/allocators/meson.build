gst_allocators_headers = files([
  'allocators.h',
  'allocators-prelude.h',
  'gstfdmemory.h',
  'gstphysmemory.h',
  'gstdmabuf.h',
  'gstdrmdumb.h',
  'gstshmallocator.h',
])
install_headers(gst_allocators_headers, subdir : 'gstreamer-1.0/gst/allocators/')

gst_allocators_sources = files([
  'gstdrmdumb.c',
  'gstdmabuf.c',
  'gstfdmemory.c',
  'gstphysmemory.c',
  'gstshmallocator.c',
])

gst_allocators_cargs = [
  gst_plugins_base_args,
  '-DBUILDING_GST_ALLOCATORS',
  '-DG_LOG_DOMAIN="GStreamer-Allocators"',
]

if cc.has_function('memfd_create')
  gst_allocators_cargs += [
    '-DHAVE_MEMFD_CREATE',
    '-D_GNU_SOURCE',
  ]
elif cc.has_function('shm_open')
  gst_allocators_cargs += [
    '-DHAVE_SHM_OPEN',
  ]
endif

gstallocators = library('gstallocators-@0@'.format(api_version),
  gst_allocators_sources,
  c_args : gst_allocators_cargs,
  include_directories: [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : [libdrm_dep, gst_dep],
)

pkg_name = 'gstreamer-allocators-1.0'
pkgconfig.generate(gstallocators,
  libraries : [libdrm_dep, gst_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'Allocators implementation',
)

library_def = {'lib': gstallocators}
allocators_gen_sources  = []
if build_gir
  gst_gir_extra_args = gir_init_section + [ '--c-include=gst/allocators/allocators.h' ]
  gir = {
    'sources' : gst_allocators_sources + gst_allocators_headers,
    'namespace' : 'GstAllocators',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0'],
    'install' : true,
    'extra_args' : gst_gir_extra_args,
    'dependencies' : [gst_dep]
  }
  library_def += {'gir': [gir]}
  if not static_build
    allocators_gir = gnome.generate_gir(gstallocators, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [allocators_gir]}
    allocators_gen_sources += allocators_gir
  endif
endif
gst_libraries += [[pkg_name, library_def]]

allocators_dep = declare_dependency(link_with: gstallocators,
  include_directories : [libsinc],
  dependencies : [gst_dep],
  sources : allocators_gen_sources)

meson.override_dependency('gstreamer-allocators-1.0', allocators_dep)
