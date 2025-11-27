subdir('cmph')

install_data('gdump.c', install_dir: get_option('datadir') / 'gobject-introspection-1.0')

custom_c_args = []

if cc.get_id() != 'msvc'
  custom_c_args = cc.get_supported_arguments([
    '-Wno-old-style-definition',
    '-Wno-cast-align',
  ])
endif

girepo_gthash_lib = static_library('girepository-gthash',
  sources: 'gthash.c',
  include_directories : configinc,
  c_args: gi_hidden_visibility_cflags + custom_c_args,
  dependencies: [
    cmph_dep,
    glib_dep,
    gmodule_dep,
    gobject_dep,
  ],
)

girepo_gthash_dep = declare_dependency(
  link_with: girepo_gthash_lib,
  dependencies: [glib_dep, gmodule_dep, gobject_dep],
  include_directories: include_directories('.'),
)

if cc.get_id() != 'msvc'
  custom_c_args = cc.get_supported_arguments([
    '-Wno-unused-parameter',
    '-Wno-duplicated-branches',
    '-Wno-cast-align',
  ])
endif

girepo_internals_lib = static_library('girepository-internals',
  sources: [
    'girmodule.c',
    'girnode.c',
    'giroffsets.c',
    'girparser.c',
    'girwriter.c',
  ],
  c_args: gi_hidden_visibility_cflags + custom_c_args,
  include_directories : configinc,
  dependencies: [girepo_gthash_dep, libffi_dep],
)

girepo_internals_dep = declare_dependency(
  link_with: girepo_internals_lib,
  dependencies: libffi_dep,
  include_directories: include_directories('.'),
)

gi_version_h = configure_file(
  configuration: config,
  input: 'giversion.h.in',
  output: 'giversion.h',
)

girepo_headers = [
  'giarginfo.h',
  'gibaseinfo.h',
  'gicallableinfo.h',
  'giconstantinfo.h',
  'gienuminfo.h',
  'gifieldinfo.h',
  'gifunctioninfo.h',
  'giinterfaceinfo.h',
  'giobjectinfo.h',
  'gipropertyinfo.h',
  'giregisteredtypeinfo.h',
  'girepository.h',
  'girffi.h',
  'gisignalinfo.h',
  'gistructinfo.h',
  'gitypeinfo.h',
  'gitypelib.h',
  'gitypes.h',
  'giunioninfo.h',
  'giversionmacros.h',
  'givfuncinfo.h',
  gi_version_h,
]

girepo_sources = [
  'gdump.c',
  'giarginfo.c',
  'gibaseinfo.c',
  'gicallableinfo.c',
  'giconstantinfo.c',
  'gienuminfo.c',
  'gifieldinfo.c',
  'gifunctioninfo.c',
  'ginvoke.c',
  'giinterfaceinfo.c',
  'giobjectinfo.c',
  'gipropertyinfo.c',
  'giregisteredtypeinfo.c',
  'girepository.c',
  'girffi.c',
  'gisignalinfo.c',
  'gistructinfo.c',
  'gitypeinfo.c',
  'gitypelib.c',
  'giunioninfo.c',
  'giversion.c',
  'givfuncinfo.c',
]

# Used in gir/meson.build
girepo_gir_sources = files(
  'giarginfo.c',
  'gibaseinfo.c',
  'gicallableinfo.c',
  'giconstantinfo.c',
  'gienuminfo.c',
  'gifieldinfo.c',
  'gifunctioninfo.c',
  'giinterfaceinfo.c',
  'giobjectinfo.c',
  'gipropertyinfo.c',
  'giregisteredtypeinfo.c',
  'girepository.c',
  'gisignalinfo.c',
  'gistructinfo.c',
  'gitypeinfo.c',
  'giunioninfo.c',
  'giversion.c',
  'givfuncinfo.c',
  'giarginfo.h',
  'gibaseinfo.h',
  'gicallableinfo.h',
  'giconstantinfo.h',
  'gienuminfo.h',
  'gifieldinfo.h',
  'gifunctioninfo.h',
  'giinterfaceinfo.h',
  'giobjectinfo.h',
  'gipropertyinfo.h',
  'giregisteredtypeinfo.h',
  'girepository.h',
  'gisignalinfo.h',
  'gistructinfo.h',
  'gitypeinfo.h',
  'gitypelib.h',
  'gitypes.h',
  'giunioninfo.h',
  'givfuncinfo.h',
) + [gi_version_h]


install_headers(girepo_headers, subdir: 'gobject-introspection-1.0')

if cc.get_id() != 'msvc'
  custom_c_args = cc.get_supported_arguments([
    '-Wno-unused-parameter',
    '-Wno-duplicated-branches',
    '-Wno-type-limits',
    '-Wno-cast-align',
    '-Wno-missing-field-initializers',
  ])
endif

lib_version = '1.0.0'
lib_version_arr = lib_version.split('.')
lib_version_major = lib_version_arr[0].to_int()
lib_version_minor = lib_version_arr[1].to_int()
lib_version_micro = lib_version_arr[2].to_int()

osx_current = lib_version_major + 1
lib_osx_version = [osx_current, '@0@.@1@'.format(osx_current, lib_version_minor)]

girepo_lib = shared_library('girepository-1.0',
  sources: girepo_sources,
  include_directories : configinc,
  c_args: gi_hidden_visibility_cflags + ['-DG_IREPOSITORY_COMPILATION'] +
          custom_c_args,
  dependencies: [
    glib_dep,
    gobject_dep,
    gmodule_dep,
    girepo_internals_dep,
  ],
  version: lib_version,
  darwin_versions: lib_osx_version,
  install: true,
)

girepo_dep = declare_dependency(
  link_with: girepo_lib,
  dependencies: [glib_dep, gobject_dep, gmodule_dep],
  include_directories: include_directories('.'),
)

gthash_test = executable('gthash-test', 'gthash-test.c',
  dependencies: girepo_gthash_dep,
)

test('gthash-test', gthash_test)

if giounix_dep.found() or giowin_dep.found()
  executable('gi-dump-types', 'gi-dump-types.c',
    dependencies: [girepo_dep, giounix_dep, giowin_dep])
endif
