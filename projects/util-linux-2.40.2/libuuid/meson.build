dir_libuuid = include_directories('src')

lib_uuid_sources = '''
  src/uuidP.h
  src/clear.c
  src/compare.c
  src/copy.c
  src/gen_uuid.c
  src/isnull.c
  src/pack.c
  src/parse.c
  src/uuidd.h
  src/uuid_time.c
'''.split()

predefined_c = files('src/predefined.c')
unpack_c = files('src/unpack.c')
unparse_c = files('src/unparse.c')

libuuid_sym = 'src/libuuid.sym'
libuuid_sym_path = '@0@/@1@'.format(meson.current_source_dir(), libuuid_sym)

libuuid_link_depends = []
libuuid_link_args = []

if cc.has_link_argument('-Wl,--version-script=@0@'.format(libuuid_sym_path))
	libuuid_link_depends += [libuuid_sym]
	libuuid_link_args += ['-Wl,--version-script=@0@'.format(libuuid_sym_path)]
endif

lib_uuid = both_libraries(
  'uuid',
  list_h,
  lib_uuid_sources,
  predefined_c,
  unpack_c,
  unparse_c,
  randutils_c,
  md5_c,
  sha1_c,
  include_directories : [dir_include, dir_libuuid],
  link_depends : libuuid_link_depends,
  version : libuuid_version,
  link_args : libuuid_link_args,
  dependencies : [socket_libs,
                  build_libuuid ? [] : disabler()],
  install : build_libuuid)
uuid_dep = declare_dependency(link_with: lib_uuid, include_directories: dir_libuuid)

if build_libuuid
  pkgconfig.generate(lib_uuid,
                     description : 'Universally unique id library',
                     subdirs : 'uuid',
                     version : pc_version)
  install_headers('src/uuid.h', subdir : 'uuid')
  if meson.version().version_compare('>=0.54.0')
    meson.override_dependency('uuid', uuid_dep)
  endif
endif
