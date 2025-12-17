sources = files(
  'gnutls-module.c',
  'gtlsbackend-gnutls.c',
  'gtlscertificate-gnutls.c',
  'gtlsclientconnection-gnutls.c',
  'gtlsconnection-gnutls.c',
  'gtlsdatabase-gnutls.c',
  'gtlsfiledatabase-gnutls.c',
  'gtlsserverconnection-gnutls.c'
)

incs = [top_inc]

deps = [
  gio_dep,
  glib_dep,
  gmodule_dep,
  gobject_dep,
  gnutls_dep,
  tlsbase_dep
]

giomodules += 'giognutls'

module = shared_module(
  'giognutls',
  sources: sources,
  include_directories: incs,
  dependencies: deps,
  link_args: module_ldflags,
  link_depends: symbol_map,
  name_suffix: module_suffix,
  install: build_shared,
  install_dir: gio_module_dir,
)

if build_static
  giognutls_lib = static_library('giognutls',
    objects: module.extract_all_objects(recursive: true),
    dependencies: deps,
    install: true,
    install_dir: gio_module_dir,
  )
  pkg.generate(module)

  giognutls_dep = declare_dependency(link_with: giognutls_lib)
  meson.override_dependency('giognutls', giognutls_dep)
endif

if meson.version().version_compare('>=0.58')
  env = environment()
  env.prepend('GIO_EXTRA_MODULES', meson.current_build_dir())
  meson.add_devenv(env)
endif
