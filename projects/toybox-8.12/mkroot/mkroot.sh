#!/bin/bash

# ------------------------------ Part 1: Setup -------------------------------

# Clear environment variables by restarting script w/bare minimum passed through
[ -z "$NOCLEAR" ] && exec env -i NOCLEAR=1 HOME="$HOME" PATH="$PATH" \
    LINUX="$LINUX" CROSS="$CROSS" CROSS_COMPILE="$CROSS_COMPILE" "$0" "$@"

! [ -d mkroot ] && echo "Run mkroot/mkroot.sh from toybox source dir." && exit 1

# assign command line NAME=VALUE args to env vars, the rest are packages
for i in "$@"; do
  [ "${i/=/}" != "$i" ] && export "$i" || { [ "$i" != -- ] && PKG="$PKG $i"; }
done

# Set default directory locations (overrideable from command line)
: ${TOP:=$PWD/root} ${BUILD:=$TOP/build} ${LOG:=$BUILD/log}
: ${AIRLOCK:=$BUILD/airlock} ${CCC:=$PWD/ccc} ${PKGDIR:=$PWD/mkroot/packages}

announce() { printf "\033]2;$CROSS $*\007" 2>/dev/null >/dev/tty; printf "\n=== $*\n";}
die() { echo "$@" >&2; exit 1; }

# ----- Are we cross compiling (via CROSS_COMPILE= or CROSS=)

if [ -n "$CROSS_COMPILE" ]; then
  # airlock needs absolute path
  [ -z "${X:=$(command -v "$CROSS_COMPILE"cc)}" ] && die "no ${CROSS_COMPILE}cc"
  CROSS_COMPILE="$(realpath -s "${X%cc}")"
  [ -z "$CROSS" ] && CROSS=${CROSS_COMPILE/*\//} CROSS=${CROSS/-*/}

elif [ -n "$CROSS" ]; then # CROSS=all/allnonstop/$ARCH else list known $ARCHes
  [ ! -d "$CCC" ] && die "No ccc symlink to compiler directory."
  TARGETS="$(ls "$CCC" | sed -n 's/-.*//p' | sort -u)"

  if [ "${CROSS::3}" == all ]; then # loop calling ourselves for each target
    for i in $TARGETS; do
      "$0" "$@" CROSS=$i || [ "$CROSS" == allnonstop ] || exit 1
    done; exit

  else # Find matching cross compiler under ccc/ else list available targets
    CROSS_COMPILE="$(echo "$CCC/$CROSS"-*cross/bin/"$CROSS"*-cc)" # wildcard
    [ ! -e "$CROSS_COMPILE" ] && echo $TARGETS && exit # list available targets
    CROSS_COMPILE="${CROSS_COMPILE%cc}" # trim to prefix for cc/ld/as/nm/strip
  fi
fi

# Set per-target output directory (using "host" if not cross-compiling)
: ${CROSS:=host} ${OUTPUT:=$TOP/$CROSS} ${OUTDOC:=$OUTPUT/docs}

# Verify selected compiler works
${CROSS_COMPILE}cc --static -xc - -o /dev/null <<< "int main(void){return 0;}"||
  die "${CROSS_COMPILE}cc can't create static binaries"

# ----- Create hermetic build environment

rm -rf generated
if [ -z "$NOAIRLOCK"] && [ -n "$CROSS_COMPILE" ]; then
  # When cross compiling set host $PATH to binaries with known behavior by
  # - building a host toybox later builds use as their command line
  # - cherry-picking specific commands from old path via symlink
  if [ ! -e "$AIRLOCK/toybox" ]; then
    announce "airlock" &&
    PREFIX="$AIRLOCK" KCONFIG_CONFIG=.singleconfig_airlock CROSS_COMPILE= \
      make clean defconfig toybox install_airlock && # see scripts/install.sh
    rm .singleconfig_airlock || exit 1
  fi
  export PATH="$AIRLOCK"
fi

# Create per-target work directories
TEMP="$BUILD/${CROSS}-tmp" && rm -rf "$TEMP" &&
mkdir -p "$TEMP" "$OUTPUT" "$LOG" || exit 1
[ -z "$ROOT" ] && ROOT="$OUTPUT/fs" && rm -rf "$ROOT"
LOG="$LOG/$CROSS"

# ----- log build output

# Install command line recording wrapper, logs all commands run from $PATH
if [ -z "$NOLOGPATH" ]; then
  # Move cross compiler into $PATH so calls to it get logged
  [ -n "$CROSS_COMPILE" ] && PATH="${CROSS_COMPILE%/*}:$PATH" &&
    CROSS_COMPILE=${CROSS_COMPILE##*/}
  export WRAPDIR="$BUILD/record-commands" LOGPATH="$LOG"-commands.txt
  rm -rf "$WRAPDIR" "$LOGPATH" generated/obj &&
  eval "$(WRAPDIR="$WRAPDIR" CROSS_COMPILE= NOSTRIP=1 mkroot/record-commands)"||
    exit 1
fi

# Start logging stdout/stderr
rm -f "$LOG".{n,y} || exit 1
[ -z "$NOLOG" ] && exec > >(tee "$LOG".n) 2>&1
echo "Building for $CROSS"

# ---------------------- Part 2: Create root filesystem -----------------------

# ----- Create new root filesystem's directory layout.

# FHS wants boot media opt srv usr/{local,share}, stuff under /var...
mkdir -p "$ROOT"/{dev,etc/rc,home,mnt,proc,root,sys,tmp/run,usr/{bin,sbin,lib},var} &&
chmod a+rwxt "$ROOT"/tmp && ln -s usr/{bin,sbin,lib} tmp/run "$ROOT" || exit 1

# Write init script. Runs as pid 1 from initramfs to set up and hand off system.
cat > "$ROOT"/init << 'EOF' &&
#!/bin/sh

export HOME=/home PATH=/bin:/sbin

if ! mountpoint -q dev; then
  mount -t devtmpfs dev dev
  [ $$ -eq 1 ] && ! 2>/dev/null <0 && exec 0<>/dev/console 1>&0 2>&1
  for i in ,fd /0,stdin /1,stdout /2,stderr
  do ln -sf /proc/self/fd${i/,*/} dev/${i/*,/}; done
  mkdir -p dev/shm
  chmod +t /dev/shm
fi
mountpoint -q dev/pts || { mkdir -p dev/pts && mount -t devpts dev/pts dev/pts;}
mountpoint -q proc || mount -t proc proc proc
mountpoint -q sys || mount -t sysfs sys sys
echo 0 99999 > /proc/sys/net/ipv4/ping_group_range

if [ $$ -eq 1 ]; then
  mountpoint -q mnt || [ -e /dev/?da ] && mount /dev/?da /mnt

  # Setup networking for QEMU (needs /proc)
  ifconfig lo 127.0.0.1
  ifconfig eth0 10.0.2.15
  route add default gw 10.0.2.2
  [ "$(date +%s)" -lt 1000 ] && timeout 2 sntp -sq 10.0.2.2 # Ask host
  [ "$(date +%s)" -lt 10000000 ] && sntp -sq time.google.com

  # Run package scripts (if any)
  for i in $(ls -1 /etc/rc 2>/dev/null | sort); do . /etc/rc/"$i"; done
  echo 3 > /proc/sys/kernel/printk

  [ -z "$HANDOFF" ] && [ -e /mnt/init ] && HANDOFF=/mnt/init
  [ -z "$HANDOFF" ] && HANDOFF=/bin/sh && echo -e '\e[?7hType exit when done.'

  setsid -c <>/dev/$(sed '$s@.*[ /]@@' /sys/class/tty/console/active) >&0 2>&1 \
    $HANDOFF
  reboot -f &
  sleep 5
else # for chroot
  /bin/sh
  umount /dev/pts /dev /sys /proc
fi
EOF
chmod +x "$ROOT"/init &&

# Google's nameserver, passwd+group with special (root/nobody) accounts + guest
echo "nameserver 8.8.8.8" > "$ROOT"/etc/resolv.conf &&
cat > "$ROOT"/etc/passwd << 'EOF' &&
root:x:0:0:root:/root:/bin/sh
guest:x:500:500:guest:/home/guest:/bin/sh
nobody:x:65534:65534:nobody:/proc/self:/dev/null
EOF
echo -e 'root:x:0:\nguest:x:500:\nnobody:x:65534:' > "$ROOT"/etc/group &&
# Grab toybox version git or toys.h
: ${VERSION:=$(git describe --tags --abbrev=12 2>/dev/null)} &&
: ${VERSION:=$(sed -n 's/.*TOYBOX_VERSION "\([^"]*\)".*/\1/p' toys.h)} &&
# Optional file, basically a comment
echo $'NAME="mkroot"\nVERSION="'${VERSION#* }$'"\nHOME_URL="https://landley.net/toybox"' > "$ROOT"/etc/os-release || exit 1

# Build any packages listed on command line
for i in ${PKG:+plumbing $PKG}; do
  pushd .
  announce "$i"; PATH="$PKGDIR:$PATH" source $i || die $i
  popd
done

# Build static toybox with existing .config if there is one, else defconfig+sh
if [ -z "$NOTOYBOX" ]; then
  announce toybox
  [ -n "$PENDING" ] && rm -f .config
  grep -q CONFIG_SH=y .config 2>/dev/null && CONF=silentoldconfig || unset CONF
  for i in $PENDING sh route; do XX="$XX"$'\n'CONFIG_${i^^?}=y; done
  [ -e "$ROOT"/lib/libc.so ] || export LDFLAGS=--static
  PREFIX="$ROOT" make clean \
    ${CONF:-defconfig KCONFIG_ALLCONFIG=<(echo "$XX")} toybox install || exit 1
  unset LDFLAGS
fi

# ------------------ Part 3: Build + package bootable system ------------------

# Convert comma separated values in $1 to CONFIG=$2 lines
csv2cfg() { sed -E '/^$/d;s/([^,]*)($|,)/CONFIG_\1\n/g' <<< "$1" | sed '/^$/!{/=/!s/.*/&='"$2/}";}
be2csv() { eval "echo $*" | tr ' ' ,; } # brace expansion to csv

# Set variables from $CROSS, die on unrecognized target:
# BUILTIN - if set, statically link initramfs into kernel image
# DTB     - device tree binary file in build dir (qemu -dtb $DTB)
# KARCH   - linux ARCH= build argument (selects arch/$ARCH directory in source)
# KARGS   - linux kernel command line arguments (qemu -append "console=$KARGS")
# KCONF   - kernel config options for target (expanded by csv2cfg above)
# VMLINUX - linux bootable kernel file in build dir (qemu -kernel $VMLINUX)
# QEMU    - emulator name (qemu-system-$QEMU) and arguments
get_target_config()
{
  # Target-specific info in an (alphabetical order) if/else staircase
  # Each target needs board config, serial console, RTC, ethernet, block device.

  KARGS=ttyS0 VMLINUX=vmlinux
  if [ "$CROSS" == armv5l ] || [ "$CROSS" == armv4l ]; then
    # This could use the same VIRT board as armv7, but let's demonstrate a
    # different one requiring a separate device tree binary.
    KARCH=arm KARGS=ttyAMA0 VMLINUX=zImage
    QEMU="arm -M versatilepb"
    KCONF="$(be2csv CPU_ARM926T MMU VFP ARM_THUMB AEABI ARCH_VERSATILE ATAGS \
      DEPRECATED_PARAM_STRUCT BLK_DEV_SD GPIOLIB NET_VENDOR_SMSC SMC91X \
      ARM_ATAG_DTB_COMPAT{,_CMDLINE_EXTEND} PCI{,_VERSATILE} \
      SERIAL_AMBA_PL011{,_CONSOLE} RTC_{CLASS,DRV_PL031,HCTOSYS} \
      SCSI{,_LOWLEVEL,_SYM53C8XX_{2,MMIO,DMA_ADDRESSING_MODE=0}})"
    DTB=versatile-pb.dtb
  elif [ "$CROSS" == armv7l ] || [ "$CROSS" == aarch64 ]; then
    if [ "$CROSS" == aarch64 ]; then
      QEMU="aarch64 -M virt -cpu cortex-a57" KARCH=arm64 VMLINUX=Image
    else
      QEMU="arm -M virt" KARCH=arm VMLINUX=zImage
    fi
    KARGS=ttyAMA0
    KCONF="$(be2csv MMU SOC_DRA7XX VDSO CPU_IDLE KERNEL_MODE_NEON \
      ARCH_{MULTI_V7,VIRT,OMAP2PLUS_TYPICAL,ALPINE} ARM_{THUMB,CPUIDLE,LPAE} \
      ATA{,_SFF,_BMDMA,_PIIX,_GENERIC} VIRTIO_{MENU,NET,BLK,PCI,MMIO} \
      SERIAL_AMBA_PL011{,_CONSOLE} RTC_{CLASS,HCTOSYS,DRV_PL031} \
      PATA_{,OF_}PLATFORM PCI{,_HOST_GENERIC})"
  elif [ "$CROSS" == hexagon ]; then
    QEMU_M=comet KARCH="hexagon LLVM_IAS=1" KCONF=SPI,SPI_BITBANG,IOMMU_SUPPORT
  elif [ "$CROSS" == i486 ] || [ "$CROSS" == i686 ] ||
       [ "$CROSS" == x86_64 ] || [ "$CROSS" == x32 ]; then
    if [ "$CROSS" == i486 ]; then
      QEMU="i386 -cpu 486 -global fw_cfg.dma_enabled=false" KCONF=M486
    elif [ "$CROSS" == i686 ]; then
      QEMU="i386 -cpu pentium3" KCONF=MPENTIUMII
    else
      QEMU=x86_64 KCONF=64BIT
      [ "$CROSS" == x32 ] && KCONF=X86_X32
    fi
    KARCH=x86 VMLINUX=bzImage
    KCONF+=,"$(be2csv UNWINDER_FRAME_POINTER PCI BLK_DEV_SD NET_VENDOR_INTEL \
      E1000 RTC_CLASS ATA{,_SFF,_BMDMA,_PIIX} SERIAL_8250{,_CONSOLE})"
  elif [ "$CROSS" == m68k ]; then
    QEMU_M=q800 KARCH=m68k
    KCONF="$(be2csv MMU M68040 M68KFPU_EMU MAC BLK_DEV_SD MACINTOSH_DRIVERS \
      NET_VENDOR_NATSEMI MACSONIC SCSI{,_LOWLEVEL,_MAC_ESP} \
      SERIAL_PMACZILOG{,_TTYS,_CONSOLE})"
  elif [ "$CROSS" == microblaze ]; then
    QEMU_M=petalogix-s3adsp1800 KARCH=microblaze KARGS=ttyUL0
    KCONF="$(be2csv MMU CPU_BIG_ENDIAN SERIAL_UARTLITE{,_CONSOLE} \
      XILINX_{EMACLITE,MICROBLAZE0_{FAMILY="spartan3adsp",USE_{{MSR,PCMP}_INSTR,BARREL,HW_MUL}=1}} \
      NET_VENDOR_XILINX)"
  elif [ "${CROSS#mips}" != "$CROSS" ]; then # mips mipsel mips64 mips64el
    QEMU_M=malta KARCH=mips
    KCONF="$(be2csv MIPS_MALTA CPU_MIPS32_R2 BLK_DEV_SD NET_VENDOR_AMD PCNET32 \
      PCI SERIAL_8250{,_CONSOLE} ATA{,_SFF,_BMDMA,_PIIX} POWER_RESET{,_SYSCON})"
    [ "${CROSS/64/}" == "$CROSS" ] && KCONF+=,CPU_MIPS32_R2 ||
      KCONF+=,64BIT,CPU_MIPS64_R1,MIPS32_O32
    [ "${CROSS%el}" != "$CROSS" ] && KCONF+=,CPU_LITTLE_ENDIAN
  elif [ "$CROSS" == or1k ]; then
    KARCH=openrisc QEMU_M=virt KARGS=ttyS0
    KCONF="$(be2csv ETHOC SERIO SERIAL_OF_PLATFORM SERIAL_8250{,_CONSOLE} \
      VIRTIO_{MENU,NET,BLK,PCI,MMIO} POWER_RESET{,_SYSCON{,_POWEROFF}} SYSCON_REBOOT_MODE)"
  elif [ "$CROSS" == powerpc ]; then
    KARCH=powerpc QEMU="ppc -M g3beige"
    KCONF="$(be2csv ALTIVEC PATA_MACIO BLK_DEV_SD MACINTOSH_DRIVERS SERIO \
      NET_VENDOR_{8390,NATSEMI} NE2K_PCI SERIAL_PMACZILOG{,_TTYS,_CONSOLE} \
      ATA{,_SFF,_BMDMA} ADB{,_CUDA} BOOTX_TEXT PPC_{PMAC,OF_BOOT_TRAMPOLINE})"
  elif [ "$CROSS" == powerpc64 ] || [ "$CROSS" == powerpc64le ]; then
    KARCH=powerpc QEMU="ppc64 -M pseries -vga none" KARGS=hvc0
    KCONF="$(be2csv PPC64 BLK_DEV_SD ATA NET_VENDOR_IBM IBMVETH HVC_CONSOLE \
      PPC_{PSERIES,OF_BOOT_TRAMPOLINE,TRANSACTIONAL_MEM,DISABLE_WERROR} \
      SCSI_{LOWLEVEL,IBMVSCSI})"
    [ "$CROSS" == powerpc64le ] && KCONF=$KCONF,CPU_LITTLE_ENDIAN
  elif [ "$CROSS" = riscv32 ] || [ "$CROSS" = riscv64 ]; then
    # Note: -hda file.img doesn't work, but this insane overcomplicated pile:
    # -drive file=file.img,format=raw,id=hd0 -device virtio-blk-device,drive=hd0
    QEMU_M="virt -netdev user,id=net0 -device virtio-net-device,netdev=net0"
    KARCH=riscv VMLINUX=Image
    # Probably only about half of these kernel symbols are actually needed?
    KCONF="$(be2csv MMU SOC_VIRT NONPORTABLE CMODEL_MEDANY \
      RISCV_ISA_{ZICBO{M,Z},FALLBACK} FPU PCI{,_HOST_GENERIC} BLK_DEV_SD \
      SCSI_{PROC_FS,LOWLEVEL,VIRTIO} VIRTIO_{MENU,NET,BLK,PCI} SERIO_SERPORT \
      SERIAL_{EARLYCON,8250{,_CONSOLE,_PCI},OF_PLATFORM} HW_RANDOM{,_VIRTIO} \
      RTC_{CLASS,HCTOSYS} DMADEVICES VIRTIO_{MENU,PCI{,_LEGACY},INPUT,MMIO})"
    [ "$CROSS" = riscv32 ] && KCONF+=,ARCH_RV32I
  elif [ "$CROSS" = s390x ]; then
    KARCH=s390 VMLINUX=bzImage
    KCONF="$(be2csv MARCH_Z900 PACK_STACK S390_GUEST VIRTIO_{NET,BLK} \
      SCLP_VT220_{TTY,CONSOLE})"
  elif [ "$CROSS" == sh2eb ]; then
    BUILTIN=1 KARCH=sh
    KCONF="$(be2csv CPU_{SUBTYPE_J2,BIG_ENDIAN} SH_JCORE_SOC SMP JCORE_EMAC \
      FLATMEM_MANUAL MEMORY_START=0x10000000 CMDLINE_OVERWRITE DNOTIFY FUSE_FS \
      INOTIFY_USER SPI{,_JCORE} SERIAL_UARTLITE{,_CONSOLE} PWRSEQ_SIMPLE \
      MMC{,_BLOCK,_SPI} UIO{,_PDRV_GENIRQ} MTD{,_SPI_NOR,_SST25L,_OF_PARTS} \
      BINFMT_{ELF_FDPIC,MISC} I2C{,_HELPER_AUTO})"
    KCONF+=,CMDLINE=\"console=ttyUL0\ earlycon\"
  elif [ "$CROSS" == sh4 ] || [ "$CROSS" == sh4eb ]; then
    QEMU_M="r2d -serial null -serial mon:stdio" KARCH=sh
    KARGS="ttySC1 noiotrap" VMLINUX=zImage
    KCONF="$(be2csv CPU_SUBTYPE_SH7751R MMU VSYSCALL SH_{FPU,RTS7751R2D} PCI \
      RTS7751R2D_PLUS SERIAL_SH_SCI{,_CONSOLE} NET_VENDOR_REALTEK 8139CP \
      BLK_DEV_SD ATA{,_SFF,_BMDMA} PATA_PLATFORM BINFMT_ELF_FDPIC \
      CMDLINE_FROM_BOOTLOADER MEMORY_START=0x0c000000)"
#see also SPI{,_SH_SCI} MFD_SM501 RTC_{CLASS,DRV_{R9701,SH},HCTOSYS}
    [ "$CROSS" == sh4eb ] && KCONF+=,CPU_BIG_ENDIAN
  else die "Unknown \$CROSS=$CROSS"
  fi
  : ${QEMU:=$CROSS ${QEMU_M:+-M $QEMU_M}}
}

# Linux kernel .config symbols common to all architectures
: ${GENERIC_KCONF:=$(be2csv PANIC_TIMEOUT=1 NO_HZ_IDLE HIGH_RES_TIMERS RD_GZIP \
  BINFMT_{ELF,SCRIPT} BLK_DEV{,_INITRD,_LOOP} EXT4_{FS,USE_FOR_EXT2} \
  VFAT_FS FAT_DEFAULT_UTF8 MISC_FILESYSTEMS NLS_{CODEPAGE_437,ISO8859_1} \
  SQUASHFS{,_XATTR,_ZLIB} TMPFS{,_POSIX_ACL} DEVTMPFS{,_MOUNT} \
  NET{,DEVICES,_CORE,CONSOLE} PACKET UNIX INET IPV6 ETHERNET \
  COMPAT_32BIT_TIME EARLY_PRINTK IKCONFIG{,_PROC})}

# ----- Build kernel for target

INITRAMFS=initramfs.cpio.gz
if [ -z "$LINUX" ] || [ ! -d "$LINUX/kernel" ]; then
  echo 'No $LINUX directory, kernel build skipped.'
else
  # Which architecture are we building a kernel for?
  LINUX="$(realpath "$LINUX")"
  [ "$CROSS" == host ] && CROSS="$(uname -m)"
  get_target_config

  # Write the qemu launch script
  if [ -n "$QEMU" ]; then
    [ -z "$BUILTIN" ] && INITRD='-initrd "$DIR"/'"$INITRAMFS"
    { echo DIR='"$(dirname $0)";' qemu-system-"$QEMU" -m 256 '"$@"' $QEMU_MORE \
        -nographic -no-reboot -kernel '"$DIR"'/linux-kernel $INITRD \
        ${DTB:+-dtb '"$DIR"'/linux.dtb} \
        "-append \"HOST=$CROSS console=$KARGS \$KARGS\"" &&
      echo "echo -e '\\e[?7h'"
    } > "$OUTPUT"/run-qemu.sh &&
    chmod +x "$OUTPUT"/run-qemu.sh || exit 1
  fi

  announce "linux-$KARCH"
  pushd "$LINUX" && make distclean && popd &&
  cp -sfR "$LINUX" "$TEMP/linux" && pushd "$TEMP/linux" &&

  # Write microconfig (minimal symbol name/value list in CSV format)
  mkdir -p "$OUTDOC" &&
  for i in "$GENERIC_KCONF" "$KCONF" ${MODULES+MODULES,MODULE_UNLOAD} "$KEXTRA"
  do echo "$i"; done > "$OUTDOC"/linux-microconfig &&

  # expand to miniconfig (symbol list to switch on after running "allnoconfig")
  {
    echo "# make ARCH=$KARCH allnoconfig KCONFIG_ALLCONFIG=linux-miniconfig"
    echo "# make ARCH=$KARCH -j \$(nproc)"
    echo "# boot $VMLINUX${DTB:+ dtb $DTB} console=$KARGS"
    echo
    while read i; do
      echo "# architecture ${X:-independent}"
      csv2cfg "$i" y
      X=${X:+extra} X=${X:-specific}
    done < "$OUTDOC"/linux-microconfig
    [ -n "$BUILTIN" ] && echo -e CONFIG_INITRAMFS_SOURCE="\"$OUTPUT/fs\""
    for i in $MODULES; do csv2cfg "$i" m; done
  } > "$OUTDOC/linux-miniconfig" &&

  # Expand miniconfig to full .config
  make ARCH=$KARCH allnoconfig KCONFIG_ALLCONFIG="$OUTDOC/linux-miniconfig" &&
  cp .config "$OUTDOC/linux-fullconfig" &&

  # Build kernel. Copy config, device tree binary, and kernel binary to output
  make ARCH=$KARCH CROSS_COMPILE="$CROSS_COMPILE" -j $(nproc) all || exit 1
  [ -n "$DTB" ] && { cp "$(find -name $DTB)" "$OUTPUT/linux.dtb" || exit 1 ;}
  if [ -n "$MODULES" ]; then
    make ARCH=$KARCH INSTALL_MOD_PATH=modz modules_install &&
      (cd modz && find lib/modules | cpio -o -H newc -R +0:+0 ) | gzip \
       > "$OUTDOC/modules.cpio.gz" || exit 1
  fi
  [ ! -e "$VMLINUX" ] && VMLINUX=arch/$KARCH/boot/$VMLINUX
  cp "$VMLINUX" "$OUTPUT"/linux-kernel && cd .. && rm -rf linux && popd ||exit 1
fi

# clean up and package root filesystem for initramfs.
announce initramfs
[ -z "$BUILTIN" ] && DIR="$OUTPUT" || DIR="$OUTDOC"
{ (cd "$ROOT" && find . -printf '%P\n' | cpio -o -H newc -R +0:+0 ) || exit 1
  ! test -e "$OUTDOC/modules.cpio.gz" || zcat $_;} | gzip \
  > "$DIR/$INITRAMFS" || exit 1

mv "$LOG".{n,y} && echo "Output is in $OUTPUT"
rmdir "$TEMP" 2>/dev/null || exit 0 # remove if empty, not an error
