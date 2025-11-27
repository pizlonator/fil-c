giscanner_files = [
  '__init__.py',
  'annotationmain.py',
  'annotationparser.py',
  'ast.py',
  'cachestore.py',
  'ccompiler.py',
  'codegen.py',
  'docmain.py',
  'docwriter.py',
  'dumper.py',
  'introspectablepass.py',
  'girparser.py',
  'girwriter.py',
  'gdumpparser.py',
  'maintransformer.py',
  'mdextensions.py',
  'message.py',
  'msvccompiler.py',
  'pkgconfig.py',
  'shlibs.py',
  'scannermain.py',
  'sectionparser.py',
  'sourcescanner.py',
  'testcodegen.py',
  'transformer.py',
  'utils.py',
  'xmlwriter.py',
]

pkglibdir = join_paths(get_option('libdir'), meson.project_name())
giscannerdir = join_paths(pkglibdir, 'giscanner')

giscanner_built_files = []
giscanner_conf_data = configuration_data()
giscanner_conf_data.set('VERSION', meson.project_version())
giscanner_built_files += configure_file(input: '_version.py.in',
                                        output: '_version.py',
                                        install_dir : giscannerdir,
                                        configuration: giscanner_conf_data)
foreach f : giscanner_files
  giscanner_built_files += configure_file(input : f, output : f,
                                          install_dir : giscannerdir,
                                          configuration : giscanner_conf_data)
endforeach


# Copy to builddir for use with dumper.py when running uninstalled
gdump = fs.copyfile('../girepository/gdump.c', 'gdump.c')

install_subdir('doctemplates', install_dir: giscannerdir)

doc_templates = []
subdir('doctemplates/devdocs')
subdir('doctemplates/mallard')

flex = find_program('flex', 'win_flex')
bison = find_program('bison', 'win_bison')

scannerparser = custom_target('scannerparser',
    input: 'scannerparser.y',
   output: ['scannerparser.c', 'scannerparser.h'],
  command: [bison, '@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@']

)

scannerlexer = custom_target('scannerlexer',
    input: 'scannerlexer.l',
   output: 'scannerlexer.c',
  command: [flex, '-o', '@OUTPUT@', '@INPUT@']
)

giscanner_args = []
if not cc.has_header('unistd.h')
  giscanner_args += '-DYY_NO_UNISTD_H'
endif

custom_c_args = []
if cc.get_id() != 'msvc'
  custom_c_args = cc.get_supported_arguments([
    '-Wno-missing-field-initializers',
    '-Wno-unused-parameter',
    '-Wno-misleading-indentation',
  ])
endif

giscanner_lib = static_library('giscanner',
  sources: [
    'sourcescanner.c',
    scannerparser,
    scannerlexer,
  ],
  c_args: giscanner_args + custom_c_args,
  include_directories : configinc,
  dependencies: [glib_dep, gobject_dep, gio_dep, gmodule_dep],
)

# https://github.com/mesonbuild/meson/issues/4117
if host_machine.system() == 'windows'
  python_ext_dep = python.dependency()
else
  python_ext_dep = python.dependency().partial_dependency(compile_args: true)
endif

if not python_ext_dep.found()
  # For example if Python is 32bit but we require a 64bit variant
  error('Python installation not useable')
endif

giscanner_pymod = python.extension_module('_giscanner', ['giscannermodule.c'],
  link_with: giscanner_lib,
  c_args: gi_hidden_visibility_cflags + custom_c_args,
  include_directories : configinc,
  dependencies: [
    glib_dep,
    gobject_dep,
    gio_dep,
    gmodule_dep,
    python_ext_dep,
  ],
  install: true,
  install_dir: giscannerdir,
)
