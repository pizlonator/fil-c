service_conf = configuration_data()
service_conf.set('libexecdir', libexecdir)

service = 'org.gtk.GLib.PACRunner.service'

configure_file(
  input: service + '.in',
  output: service,
  install: true,
  install_dir: join_paths(datadir, 'dbus-1', 'services'),
  configuration: service_conf
)

service = 'glib-pacrunner.service'

configure_file(
  input: service + '.in',
  output: service,
  install: true,
  install_dir: join_paths('lib', 'systemd', 'user'),
  configuration: service_conf
)

sources = files(
  'glibproxyresolver.c',
  'libproxy-module.c'
)

deps = [
  gio_dep,
  glib_dep,
  gmodule_dep,
  gobject_dep,
  libproxy_dep
]

giomodules += 'giolibproxy'

module = shared_module(
  'giolibproxy',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: '-DGLIBPROXY_MODULE',
  link_args: module_ldflags,
  link_depends: symbol_map,
  name_suffix: module_suffix,
  install: build_shared,
  install_dir: gio_module_dir
)

if build_static
  giolibproxy_lib = static_library('giolibproxy',
    objects: module.extract_all_objects(recursive: true),
    dependencies: deps,
    install: true,
    install_dir: gio_module_dir
  )
  pkg.generate(module)

  giolibproxy_dep = declare_dependency(link_with: giolibproxy_lib)
  meson.override_dependency('giolibproxy', giolibproxy_dep)
endif

sources = files(
  'glibproxyresolver.c',
  'glibpacrunner.c'
)

executable(
  'glib-pacrunner',
  sources,
  include_directories: top_inc,
  dependencies: deps,
  install: true,
  install_dir: libexecdir
)

proxy_test_programs += [['environment', 'libproxy', deps]]

if meson.version().version_compare('>=0.58')
  env = environment()
  env.prepend('GIO_EXTRA_MODULES', meson.current_build_dir())
  meson.add_devenv(env)
endif
