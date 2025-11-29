toml_conf = configuration_data()
toml_conf.set('version', meson.project_version())

gidocgen = find_program('gi-docgen', required: get_option('documentation'), native: true)

gidocgen_common_args = [
  '--quiet',
  '--no-namespace-dir',
]

if get_option('werror')
  gidocgen_common_args += ['--fatal-warnings']
endif

docs_dir = gtk_datadir / 'doc'

if get_option('documentation') and not build_gir
  error('API reference requires introspection.')
endif

subdir('gdk')
subdir('gsk')
subdir('gtk')
