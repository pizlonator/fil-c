if get_option('build-python').disabled()
  subdir_done()
endif

pylibmount_sources = '''
  pylibmount.c
  pylibmount.h
  fs.c
  tab.c
'''.split()

if LINUX
  pylibmount_sources += 'context.c'
endif

python_module = import('python')
python = python_module.find_installation(
    get_option('python'),
    required : true,
    disabler : true)
if meson.version().version_compare('<1.4.1')
  cc.has_header('Python.h',
    include_directories : include_directories(python.get_path('include')),
    required : true)
endif
python.extension_module(
  'pylibmount',
  pylibmount_sources,
  include_directories : [dir_include],
  subdir : 'libmount',
  dependencies : [mount_dep, python.dependency()],
  c_args : [
    '-Wno-cast-function-type',

    # https://github.com/util-linux/util-linux/issues/2366
    python.language_version().version_compare('>=3.12') ?
      [ '-Wno-error=redundant-decls' ] : [],
  ],
  install : true)

python.install_sources(
  '__init__.py',
  subdir : 'libmount',
  pure : false)
