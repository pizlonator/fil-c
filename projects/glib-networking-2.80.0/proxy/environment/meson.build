service_conf = configuration_data()
service_conf.set('libexecdir', libexecdir)

sources = files(
  'genvironmentproxyresolver.c',
  'environmentproxy-module.c'
)

deps = [
  gio_dep,
  glib_dep,
  gmodule_dep,
  gobject_dep
]

giomodules += 'gioenvironmentproxy'

module = shared_module(
  'gioenvironmentproxy',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: '-DGENVIRONMENTPROXY_MODULE',
  link_args: module_ldflags,
  link_depends: symbol_map,
  name_suffix: module_suffix,
  install: build_shared,
  install_dir: gio_module_dir
)

if build_static
  gioenvironmentproxy_lib = static_library('gioenvironmentproxy',
    objects: module.extract_all_objects(recursive: true),
    dependencies: deps,
    install: true,
    install_dir: gio_module_dir
  )
  pkg.generate(module)

  gioenvironmentproxy_dep = declare_dependency(
    link_with: gioenvironmentproxy_lib
  )
  meson.override_dependency('gioenvironmentproxy', gioenvironmentproxy_dep)
endif

proxy_test_programs += [['environment', 'environment', deps]]

if meson.version().version_compare('>=0.58')
  env = environment()
  env.prepend('GIO_EXTRA_MODULES', meson.current_build_dir())
  meson.add_devenv(env)
endif
