dir_libblkid = include_directories('.', 'src')
if not build_libblkid
  blkid_dep = disabler()
  blkid_static_dep = disabler()
  subdir_done()
endif

defs = configuration_data()
defs.set('LIBBLKID_DATE', libblkid_date)
defs.set('LIBBLKID_VERSION', pc_version)

configure_file(
  input : 'src/blkid.h.in',
  output : 'blkid.h',
  configuration : defs,
  install : build_libblkid,
  install_dir : join_paths(get_option('includedir'), 'blkid'),
)

lib_blkid_sources = '''
  src/blkidP.h
  src/init.c
  src/cache.c
  src/config.c
  src/dev.c
  src/devname.c
  src/devno.c
  src/encode.c
  src/evaluate.c
  src/getsize.c
  src/probe.c
  src/read.c
  src/resolve.c
  src/save.c
  src/tag.c
  src/verify.c
  src/version.c

  src/partitions/aix.c
  src/partitions/aix.h
  src/partitions/atari.c
  src/partitions/bsd.c
  src/partitions/dos.c
  src/partitions/gpt.c
  src/partitions/mac.c
  src/partitions/minix.c
  src/partitions/partitions.c
  src/partitions/partitions.h
  src/partitions/sgi.c
  src/partitions/solaris_x86.c
  src/partitions/sun.c
  src/partitions/ultrix.c
  src/partitions/unixware.c
  
  src/superblocks/adaptec_raid.c
  src/superblocks/apfs.c
  src/superblocks/bcache.c
  src/superblocks/befs.c
  src/superblocks/bfs.c
  src/superblocks/bitlocker.c
  src/superblocks/bluestore.c
  src/superblocks/btrfs.c
  src/superblocks/cs_fvault2.c
  src/superblocks/cramfs.c
  src/superblocks/ddf_raid.c
  src/superblocks/drbd.c
  src/superblocks/drbdproxy_datalog.c
  src/superblocks/drbdmanage.c
  src/superblocks/exfat.c
  src/superblocks/exfs.c
  src/superblocks/ext.c
  src/superblocks/f2fs.c
  src/superblocks/gfs.c
  src/superblocks/hfs.c
  src/superblocks/highpoint_raid.c
  src/superblocks/hpfs.c
  src/superblocks/iso9660.c
  src/superblocks/isw_raid.c
  src/superblocks/jfs.c
  src/superblocks/jmicron_raid.c
  src/superblocks/linux_raid.c
  src/superblocks/lsi_raid.c
  src/superblocks/luks.c
  src/superblocks/lvm.c
  src/superblocks/minix.c
  src/superblocks/mpool.c
  src/superblocks/netware.c
  src/superblocks/nilfs.c
  src/superblocks/ntfs.c
  src/superblocks/refs.c
  src/superblocks/nvidia_raid.c
  src/superblocks/ocfs.c
  src/superblocks/promise_raid.c
  src/superblocks/reiserfs.c
  src/superblocks/romfs.c
  src/superblocks/silicon_raid.c
  src/superblocks/squashfs.c
  src/superblocks/stratis.c
  src/superblocks/superblocks.c
  src/superblocks/superblocks.h
  src/superblocks/swap.c
  src/superblocks/sysv.c
  src/superblocks/ubi.c
  src/superblocks/ubifs.c
  src/superblocks/udf.c
  src/superblocks/ufs.c
  src/superblocks/vdo.c
  src/superblocks/vfat.c
  src/superblocks/via_raid.c
  src/superblocks/vmfs.c
  src/superblocks/vxfs.c
  src/superblocks/xfs.c
  src/superblocks/zfs.c
  src/superblocks/zonefs.c
  src/superblocks/erofs.c

  src/topology/topology.c
  src/topology/topology.h
'''.split()

if LINUX
  lib_blkid_sources += '''
    src/topology/dm.c
    src/topology/evms.c
    src/topology/ioctl.c
    src/topology/lvm.c
    src/topology/md.c
    src/topology/sysfs.c
  '''.split()
endif

libblkid_sym = 'src/libblkid.sym'
libblkid_sym_path = '@0@/@1@'.format(meson.current_source_dir(), libblkid_sym)

if build_libblkid and not have_dirfd and not have_ddfd
  error('neither dirfd nor ddfd are available')
endif

libblkid_link_depends = []
libblkid_link_args = []

if cc.has_link_argument('-Wl,--version-script=@0@'.format(libblkid_sym_path))
	libblkid_link_depends += [libblkid_sym]
	libblkid_link_args += ['-Wl,--version-script=@0@'.format(libblkid_sym_path)]
endif

lib_blkid = both_libraries(
  'blkid',
  list_h,
  lib_blkid_sources,
  include_directories : [dir_include, dir_libblkid],
  link_depends : libblkid_link_depends,
  version : libblkid_version,
  link_args : libblkid_link_args,
  link_with : lib_common,
  dependencies : build_libblkid ? [lib_econf] : disabler(),
  install : build_libblkid)
blkid_dep = declare_dependency(link_with: lib_blkid, include_directories: '.')

lib_blkid_static = lib_blkid.get_static_lib()
blkid_static_dep = declare_dependency(link_with: lib_blkid_static, include_directories: '.')

pkgconfig.generate(lib_blkid,
                    description : 'Block device id library',
                    subdirs : 'blkid',
                    version : pc_version)
if meson.version().version_compare('>=0.54.0')
  meson.override_dependency('blkid', blkid_dep)
endif
