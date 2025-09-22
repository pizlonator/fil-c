dir_sys_utils = include_directories('.')

hwclock_parse_date = bison_gen.process('hwclock-parse-date.y')

lsmem_sources = files(
  'lsmem.c',
)

chmem_sources = files(
  'chmem.c',
)

choom_sources = files(
  'choom.c',
)

ipcmk_sources = files(
  'ipcmk.c',
)

ipcrm_sources = files(
  'ipcrm.c',
)

ipcs_sources = files(
  'ipcs.c',
  'ipcutils.c',
  'ipcutils.h',
)

rfkill_sources = files(
  'rfkill.c',
)

renice_sources = files(
  'renice.c',
)

setpgid_sources = files(
  'setpgid.c',
)

setsid_sources = files(
  'setsid.c',
)

readprofile_sources = files(
  'readprofile.c',
)

tunelp_sources = files(
  'tunelp.c',
)

fstrim_sources = files(
  'fstrim.c',
)

dmesg_sources = files(
  'dmesg.c',
) + \
  monotonic_c + pager_c

ctrlaltdel_sources = files(
  'ctrlaltdel.c',
)

fsfreeze_sources = files(
  'fsfreeze.c',
)

blkdiscard_sources = files(
  'blkdiscard.c',
) + \
  monotonic_c

blkzone_sources = files(
  'blkzone.c',
)

blkpr_sources = files(
  'blkpr.c',
)

ldattach_sources = files(
  'ldattach.c',
)

rtcwake_sources = files(
  'rtcwake.c',
)

setarch_sources = files(
  'setarch.c',
)

eject_sources = files(
  'eject.c',
) + \
  monotonic_c

losetup_sources = files(
  'losetup.c',
)

zramctl_sources = files(
  'zramctl.c',
) + \
  ismounted_c

prlimit_sources = files(
  'prlimit.c',
)

lsns_sources = files(
  'lsns.c',
)

mount_sources = files(
  'mount.c',
)

umount_sources = files(
  'umount.c',
)
swapon_sources = files(
  'swapon.c',
  'swapon-common.c',
  'swapon-common.h',
) + \
  swapprober_c

swapoff_sources = files(
  'swapoff.c',
  'swapon-common.c',
  'swapon-common.h',
) + \
  swapprober_c

lscpu_sources = files(
  'lscpu.c',
  'lscpu.h',
  'lscpu-cputype.c',
  'lscpu-cpu.c',
  'lscpu-topology.c',
  'lscpu-virt.c',
  'lscpu-arm.c',
  'lscpu-dmi.c',
)

chcpu_sources = files(
  'chcpu.c',
)

wdctl_sources = files(
  'wdctl.c',
)

mountpoint_sources = files(
  'mountpoint.c',
)

fallocate_sources = files(
  'fallocate.c',
)

pivot_root_sources = files(
  'pivot_root.c',
)

switch_root_sources = files(
  'switch_root.c',
)

unshare_sources = files(
  'unshare.c',
) + \
    exec_shell_c

nsenter_sources = files(
  'nsenter.c',
) + \
    exec_shell_c

setpriv_sources = files(
  'setpriv.c',
)
if LINUX and conf.get('HAVE_LINUX_LANDLOCK_H').to_string() == '1'
  setpriv_sources += files('setpriv-landlock.c')
endif

flock_sources = files(
  'flock.c',
) + \
  monotonic_c + \
  timer_c

lsipc_sources = files(
  'lsipc.c',
  'ipcutils.c',
  'ipcutils.h',
)

lsirq_sources = files(
  'lsirq.c',
  'irq-common.c',
  'irq-common.h',
)

irqtop_sources = files(
  'irqtop.c',
  'irq-common.c',
  'irq-common.h',
) + \
    monotonic_c

hwclock_sources = [
  'sys-utils/hwclock.c',
  'sys-utils/hwclock.h',
  hwclock_parse_date,
]
if use_hwclock_cmos
  hwclock_sources += [
    'sys-utils/hwclock-cmos.c',
  ]
endif
if LINUX
  hwclock_sources += [
    'sys-utils/hwclock-rtc.c',
    monotonic_c,
  ]
endif

if systemd.found()
  fstrim_service = configure_file(
    input : 'fstrim.service.in',
    output : 'fstrim.service',
    configuration : conf)

  install_data(fstrim_service,
               install_dir : systemdsystemunitdir)
  install_data('fstrim.timer',
               install_dir : systemdsystemunitdir)
endif
