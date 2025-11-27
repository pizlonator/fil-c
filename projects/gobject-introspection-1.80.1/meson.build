project('gobject-introspection', 'c',
  version: '1.80.1',
  meson_version: '>= 1.2.0',
  default_options: [
    'c_std=gnu99',
    'warning_level=2',
    'buildtype=debugoptimized',
  ],
)

host_system = host_machine.system()
host_arch = host_machine.cpu_family()
gi_versions = meson.project_version().split('.')
build_root = meson.current_build_dir()
source_root = meson.current_source_dir()

configinc = include_directories('.')

pymod = import('python')
python = pymod.find_installation(get_option('python'))

python_version = python.language_version()
python_version_req = '>=3.8'
if not python_version.version_compare(python_version_req)
  error('Requires Python @0@, @1@ found.'.format(python_version_req, python_version))
endif

if python_version.version_compare('>=3.12')
  pymod.find_installation(get_option('python'), modules: ['setuptools'])
endif

cc = meson.get_compiler('c')

project_c_args = []
if cc.get_id() != 'msvc'
    project_c_args += [
      '-Warray-bounds',
      '-Wcast-align',
      '-Wduplicated-branches',
      '-Wformat=2',
      '-Wformat-nonliteral',
      '-Wformat-security',
      '-Wimplicit-function-declaration',
      '-Winit-self',
      '-Wjump-misses-init',
      '-Wlogical-op',
      '-Wmissing-declarations',
      '-Wmissing-format-attribute',
      '-Wmissing-include-dirs',
      '-Wmissing-noreturn',
      '-Wmissing-prototypes',
      '-Wnested-externs',
      '-Wnull-dereference',
      '-Wold-style-definition',
      '-Wpacked',
      '-Wpointer-arith',
      '-Wrestrict',
      '-Wreturn-type',
      '-Wshadow',
      '-Wsign-compare',
      '-Wstrict-aliasing',
      '-Wstrict-prototypes',
      '-Wundef',
      '-Wunused-but-set-variable',
      '-Wwrite-strings',
    ]

    project_c_args += [
      '-fno-strict-aliasing',
    ]
else
    project_c_args += [
      '-FImsvc_recommended_pragmas.h',
      '-utf-8',
    ]
endif

project_c_args = cc.get_supported_arguments(project_c_args)
add_project_arguments(project_c_args, language: 'c')

if cc.get_id() == 'msvc' and project_c_args.contains('-utf-8')
  extra_giscanner_cflags = ['-utf-8']
  extra_giscanner_args = ['--cflags-begin'] + extra_giscanner_cflags + ['--cflags-end']
else
  extra_giscanner_cflags = []
  extra_giscanner_args = []
endif

config = configuration_data()

config.set('GI_MAJOR_VERSION', gi_versions[0])
config.set('GI_MINOR_VERSION', gi_versions[1])
config.set('GI_MICRO_VERSION', gi_versions[2])

config.set_quoted('GIR_SUFFIX', 'gir-1.0')
gir_dir_prefix = get_option('gir_dir_prefix')
if gir_dir_prefix == '' or gir_dir_prefix == get_option('datadir')
    gir_dir_prefix = get_option('datadir')
    gir_dir_pc_prefix = '${datadir}'
else
    gir_dir_pc_prefix = join_paths('${prefix}', gir_dir_prefix)
endif
girdir = join_paths(get_option('prefix'), gir_dir_prefix, 'gir-1.0')
config.set_quoted('GOBJECT_INTROSPECTION_DATADIR', join_paths(get_option('prefix'), get_option('datadir')))
config.set_quoted('GIR_DIR', girdir)
config.set_quoted('GOBJECT_INTROSPECTION_LIBDIR', join_paths(get_option('prefix'), get_option('libdir')))

foreach type : ['char', 'short', 'int', 'long']
  size = cc.sizeof(type)
  if size == -1
    error('Failed to get size of @0@'.format(type))
  endif
  config.set('SIZEOF_@0@'.format(type.to_upper()), size)
endforeach

add_project_arguments(['-DHAVE_CONFIG_H'], language: 'c')

gi_hidden_visibility_cflags = []
if host_system == 'windows'
  config.set('DLL_EXPORT', true)
  config.set('_GI_EXTERN', '__declspec(dllexport) extern')
  if cc.get_id() != 'msvc'
    gi_hidden_visibility_cflags += '-fvisibility=hidden'
  endif
else
  config.set('_GI_EXTERN', '__attribute__((visibility("default"))) extern')
  gi_hidden_visibility_cflags += '-fvisibility=hidden'
endif

# Reset to 0 if g-i micro version exceeds GLib's
glib_micro_version = 0  # gi_versions[2]
glib_version = '>=2.@0@.@1@'.format(gi_versions[1], glib_micro_version)

glib_dep = dependency('glib-2.0', version: glib_version,
  default_options: [
    'tests=false',
    'introspection=disabled',
  ])
gobject_dep = dependency('gobject-2.0', version: glib_version)
gio_dep = dependency('gio-2.0', version: glib_version)
gmodule_dep = dependency('gmodule-2.0', version: glib_version)
if host_system != 'windows'
  giounix_dep = dependency('gio-unix-2.0', version: glib_version)
  giowin_dep = disabler()
else
  # Don't even try to look for gio-unix-2.0 on Windows because Meson will
  # fruitlessly try to find it in the glib subproject even when we don't want
  # it to look in the subproject at all. Just use a not-found dependency.
  giounix_dep = disabler()
  # XXX: Autotools doesn't build girs for gio-win32-2.0, but maybe we should?
  giowin_dep = dependency('gio-windows-2.0', version: glib_version)
endif

# We either get libffi from the system, or from GLib's own subproject
libffi_dep = dependency('libffi')

if not (host_system == 'darwin' and host_arch == 'aarch64' or
        host_system == 'linux' and libffi_dep.version().version_compare('>= 3.4'))
  config.set('LEGACY_GIRFFI_FREE', 1)
endif

# Replicate https://github.com/mesonbuild/meson/pull/10275 setting
# the correct env in the custom_target
pkg_config = find_program('pkg-config')
g_ir_scanner_env = environment()
dep_type = glib_dep.type_name()
if dep_type == 'internal'
  g_ir_scanner_env.append('PKG_CONFIG_PATH', meson.global_build_root() + '/meson-uninstalled')
  g_ir_scanner_env.set('PKG_CONFIG', pkg_config.full_path())
endif

configure_file(
  configuration: config,
  output: 'config.h'
)

# python headers
cc.check_header('Python.h', dependencies: [python.dependency()], required: true)

# cairo
cairo_option = get_option('cairo')
if cc.get_id() == 'msvc'
  if cairo_option.disabled()
    req = cairo_option
  else
    req = false
  endif

  cairo_dep = dependency('cairo', required: req)
  cairo_gobject_dep = dependency('cairo-gobject', required: req)

  if not cairo_gobject_dep.found() or not cairo_dep.found()
    if cc.has_header('cairo.h') and cc.has_header ('cairo-gobject.h')
      cairo_dep = cc.find_library('cairo', required: cairo_option)
      cairo_gobject_dep = cc.find_library('cairo-gobject', required: cairo_option)
    endif
  endif
else
  cairo_dep = dependency('cairo', required: cairo_option)
  cairo_gobject_dep = dependency('cairo-gobject', required: cairo_option)
endif
cairo_deps_found = cairo_gobject_dep.found() and cairo_dep.found()

if not cairo_deps_found
  warning('Not building with cairo support, not all tests will be run')
endif

# doctool
doctool_option = get_option('doctool')

with_doctool = true
if doctool_option.disabled()
  with_doctool = false
else
  has_mako = run_command(python, ['-c', 'import mako'], check: false).returncode() == 0
  has_markdown = run_command(python, ['-c', 'import markdown'], check: false).returncode() == 0
  if not has_mako or not has_markdown
    if doctool_option.enabled()
      error('doctool requires markdown and mako')
    else
      with_doctool = false
    endif
  endif
endif

if not with_doctool
  warning('Not building with doctool support, not all tests will be run')
endif

fs = import('fs')

subdir('girepository')
subdir('tools')
subdir('giscanner')

# Not building gir data might be useful in cross-compilation
# scenarios where running target binaries through software emulation
# is not possible (due to the SW emulation missing support for target HW).
if get_option('build_introspection_data') == true
  subdir('gir')
else
  # for tests
  typelibs = disabler()
  gobject_gir = disabler()
  gio_gir = disabler()
endif
subdir('docs')

# The tests will also run, which is not possible if they
# were built for a different architecture.
if not meson.is_cross_build()
  subdir('tests')
endif

install_data('Makefile.introspection', install_dir: join_paths(get_option('datadir'), 'gobject-introspection-1.0'))
install_data('m4/introspection.m4', install_dir: join_paths(get_option('datadir'), 'aclocal'))

pkg = import('pkgconfig')

# We must put in the '.exe' extension for .exe executables
exe_ext = ''

if host_system == 'windows' or host_system == 'cygwin'
  exe_ext = '.exe'
endif

pkgconfig_variables = [
  'g_ir_scanner=${bindir}/g-ir-scanner',
  'g_ir_compiler=${bindir}/g-ir-compiler@0@'.format(exe_ext),
  'g_ir_generate=${bindir}/g-ir-generate@0@'.format(exe_ext),
  'gidatadir=${datadir}/gobject-introspection-1.0',
  'girdir=' + gir_dir_pc_prefix / 'gir-1.0',
  'typelibdir=${libdir}/girepository-1.0',
]

pkg.generate(girepo_lib,
  name : 'gobject-introspection',
  filebase : 'gobject-introspection-1.0',
  description : 'GObject Introspection',
  subdirs: ['gobject-introspection-1.0'],
  variables : pkgconfig_variables,
  libraries : [glib_dep, gobject_dep],
)

# This is legacy for backward compat. It is identical to the pc file generated
# above because we actually never had a pc file that has `-Wl,--export-dynamic`.
# The reason is `gmodule-2.0` was in `Requires.private` so its libs flags are
# only included when doing `pkg-config gobject-introspection-1.0 --libs --static`
# which makes no sense since we don't even build static girepository library.
# Users who need `-Wl,--export-dynamic` can add `gmodule-2.0` explicitly in their
# dependencies in addition to `gobject-introspection-1.0`.
pkg.generate(
  name : 'gobject-introspection',
  filebase : 'gobject-introspection-no-export-1.0',
  description : 'GObject Introspection',
  subdirs: ['gobject-introspection-1.0'],
  variables : pkgconfig_variables,
  libraries : [girepo_lib, glib_dep, gobject_dep],
)

# Dependency object used by Meson's GNOME module. This dependency variable must
# be named girepo_dep for backward compatibility with projects that where already
# using that name as fallback: dependency('gobject-introspection-1.0',
# fallback : ['gobject-introspection', 'girepo_dep'])
if get_option('build_introspection_data') == true
  girepo_dep = declare_dependency(
    sources: typelibs,
    dependencies: girepo_dep,
    variables: {
      'girdir': meson.current_build_dir() / 'gir',
    },
  )
  meson.override_dependency('gobject-introspection-1.0', girepo_dep)
endif
