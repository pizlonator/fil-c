libgudev_public_h = [
  'gudev.h',
  'gudevclient.h',
  'gudevdevice.h',
  'gudevenumerator.h',
  'gudevenums.h',
  'gudevtypes.h',
]

libgudev_private_h = [
  'gudevprivate.h',
]

libgudev_sources = [
  'gudevclient.c',
  'gudevdevice.c',
  'gudevenumerator.c',
]

libgudev_deps = [
  glib_dep,
  gobject_dep,
  libudev_dep,
]

libgudev_c_args = [
  '-DHAVE_CONFIG_H',
  '-D_POSIX_PTHREAD_SEMANTICS',
  '-D_REENTRANT',
  '-D_GUDEV_COMPILATION',
  '-D_GUDEV_WORK_AROUND_DEV_T_BUG',
  '-DG_LOG_DOMAIN="GUdev"',
]

libgudev_link_args = [
  '-export-dynamic',
  '-Wl,--version-script,@0@/libgudev-1.0.sym'.format(top_srcdir),
]

libgudev_enums = gnome.mkenums(
  'gudevenumtypes',
  sources: libgudev_public_h,
  h_template: 'gudevenumtypes.h.template',
  c_template: 'gudevenumtypes.c.template',
  install_header: true,
  install_dir: join_paths(
    pkgincludedir,
    'gudev',
  )
)

libgudev_enums_h = libgudev_enums.get(1)

libgudev_sources += [
  libgudev_enums,
]

install_headers(
  libgudev_public_h,
  install_dir: join_paths(
    pkgincludedir,
    'gudev',
  )
)

libgudev = library(
  package_string,
  libgudev_sources,
  version: lib_version,
  include_directories: rootdir,
  dependencies: libgudev_deps,
  c_args: libgudev_c_args,
  link_args: libgudev_link_args,
  install: true,
  install_dir: libdir,
)

libgudev_dep = declare_dependency(
  link_with: libgudev,
  include_directories: rootdir,
  dependencies: libgudev_deps,
  sources: [
    libgudev_enums_h,
  ]
)

pkg.generate(
  name: package_string,
  description: 'GObject bindings for libudev',
  requires: ['glib-2.0', 'gobject-2.0'],
  version: version,
  subdirs: package_string,
  libraries: libgudev,
  install_dir: pkgconfigdir,
  variables: [
    'exec_prefix=${prefix}',
  ]
)

if gir_dep.found()
  libgudev_gir = gnome.generate_gir(
    libgudev,
    sources: libgudev_sources + libgudev_public_h,
    nsversion: api_version,
    namespace: 'GUdev',
    symbol_prefix: 'g_udev',
    identifier_prefix: 'GUdev',
    header: 'gudev/gudev.h',
    export_packages: [package_string],
    includes: ['GObject-2.0'],
    link_with: libgudev,
    install: true,
    install_dir_gir: girdir,
    install_dir_typelib: typelibdir,
    extra_args: [
      '-D_GUDEV_COMPILATION',
      '-D_GUDEV_WORK_AROUND_DEV_T_BUG',
    ]
  )

  if vapigen_dep.found()
    libgudev_vapi = gnome.generate_vapi(
      package_string,
      sources: libgudev_gir.get(0),
      install: true,
      install_dir: vapidir,
      packages: [
        'gobject-2.0',
      ]
    )
  endif
endif

