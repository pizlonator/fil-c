mkfs_sources = files(
  'mkfs.c',
)

mkfs_bfs_sources = files(
  'mkfs.bfs.c',
)

isosize_sources = files(
  'isosize.c',
)

mkswap_sources = files(
  'mkswap.c',
) + \
  ismounted_c
if lib_selinux.found()
  mkswap_sources += selinux_utils_c
endif

swaplabel_sources = files(
  'swaplabel.c',
) + \
  swapprober_c

fsck_sources = files(
  'fsck.c',
) + \
  monotonic_c

mkfs_minix_sources = files(
  'mkfs.minix.c',
  'minix_programs.h',
) + \
  ismounted_c

fsck_minix_sources = files(
  'fsck.minix.c',
  'minix_programs.h',
) + \
  ismounted_c

mkfs_cramfs_sources = files(
  'mkfs.cramfs.c',
  'cramfs.h',
  'cramfs_common.c',
)

fsck_cramfs_sources = files(
  'fsck.cramfs.c',
  'cramfs.h',
  'cramfs_common.c',
)

raw_sources = files(
  'raw.c',
)

fdformat_sources = files(
  'fdformat.c',
)

blockdev_sources = files(
  'blockdev.c',
)

fdisk_sources = files(
  'fdisk.c',
  'fdisk.h',
  'fdisk-menu.c',
  'fdisk-list.c',
  'fdisk-list.h') + \
  pager_c

sfdisk_sources = files(
  'sfdisk.c',
  'fdisk-list.c',
  'fdisk-list.h')

cfdisk_sources = files(
  'cfdisk.c',
)

addpart_sources = files(
  'addpart.c',
)
delpart_sources = files(
  'delpart.c',
)
resizepart_sources = files(
  'resizepart.c',
)
partx_sources = files(
  'partx.c',
)
