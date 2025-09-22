dir_libfdisk = include_directories('.', 'src')

defs = configuration_data()
defs.set('LIBFDISK_VERSION', pc_version)
defs.set('LIBFDISK_MAJOR_VERSION', pc_version.split('.')[0])
defs.set('LIBFDISK_MINOR_VERSION', pc_version.split('.')[1])
defs.set('LIBFDISK_PATCH_VERSION', pc_version.split('.')[2])

configure_file(
  input : 'src/libfdisk.h.in',
  output : 'libfdisk.h',
  configuration : defs,
  install : build_libfdisk,
  install_dir : join_paths(get_option('includedir'), 'libfdisk'),
)

lib_fdisk_sources = '''
  src/fdiskP.h
  src/init.c
  src/field.c
  src/item.c
  src/test.c
  src/ask.c
  src/alignment.c
  src/label.c
  src/utils.c
  src/context.c
  src/parttype.c
  src/partition.c
  src/table.c
  src/iter.c
  src/script.c
  src/version.c
  src/wipe.c

  src/sun.c
  src/sgi.c
  src/dos.c
  src/bsd.c
  src/gpt.c
'''.split() + [
  list_h,
]

libfdisk_sym = 'src/libfdisk.sym'
libfdisk_sym_path = '@0@/@1@'.format(meson.current_source_dir(), libfdisk_sym)

lib_fdisk_includes = [dir_include,
                      dir_libblkid,
                      dir_libfdisk,
                      dir_libuuid] # XXX: should this be declared along with the lib?

lib__fdisk = static_library(
  '_fdisk',
  lib_fdisk_sources,
  include_directories : lib_fdisk_includes,
  dependencies : build_libfdisk ? [] : disabler())

lib_fdisk_static = static_library(
  'fdisk_static',
  link_whole : lib__fdisk,
  link_with : [lib_common,
               lib_uuid.get_static_lib()],
  dependencies : [blkid_static_dep],
  install : false)

lib_fdisk = library(
  'fdisk',
  link_whole : lib__fdisk,
  link_depends : libfdisk_sym,
  version : libfdisk_version,
  link_args : ['-Wl,--version-script=@0@'.format(libfdisk_sym_path)],
  link_with : [lib_common,
               lib_uuid],
  dependencies : [blkid_dep],
  install : build_libfdisk)
fdisk_dep = declare_dependency(link_with: lib_fdisk, include_directories: '.')

if build_libfdisk
  pkgconfig.generate(lib_fdisk,
                     description : 'fdisk library',
                     subdirs : 'libfdisk',
                     version : pc_version)
  if meson.version().version_compare('>=0.54.0')
    meson.override_dependency('fdisk', fdisk_dep)
  endif
endif
