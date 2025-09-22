ETCDIR=${DESTDIR}/etc
LIBDIR=${DESTDIR}/lib
SERVICEDIR=${LIBDIR}/services
MAN8=${DESTDIR}/usr/share/man/man8
SBIN=${DESTDIR}/sbin
MODE=754
DIRMODE=755
CONFMODE=644

all: links

install: all

create-dirs:
	install -d -m ${DIRMODE}  ${ETCDIR}/rc.d/rc0.d
	install -d -m ${DIRMODE}  ${ETCDIR}/rc.d/rc1.d
	install -d -m ${DIRMODE}  ${ETCDIR}/rc.d/rc2.d
	install -d -m ${DIRMODE}  ${ETCDIR}/rc.d/rc3.d
	install -d -m ${DIRMODE}  ${ETCDIR}/rc.d/rc4.d
	install -d -m ${DIRMODE}  ${ETCDIR}/rc.d/rc5.d
	install -d -m ${DIRMODE}  ${ETCDIR}/rc.d/rc6.d
	install -d -m ${DIRMODE}  ${ETCDIR}/rc.d/rcS.d
	install -d -m ${DIRMODE}  ${ETCDIR}/rc.d/init.d
	install -d -m ${DIRMODE}  ${ETCDIR}/sysconfig
	install -d -m ${DIRMODE}  ${LIBDIR}
	install -d -m ${DIRMODE}  ${SERVICEDIR}
	install -d -m ${DIRMODE}  ${MAN8}
	install -d -m ${DIRMODE}  ${SBIN}
	ln -sfn       services    ${LIBDIR}/lsb
	ln -sfn       rc.d/init.d ${ETCDIR}/init.d

files: create-dirs
	install -m ${MODE} lfs/init.d/checkfs       ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/cleanfs       ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/halt          ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/console       ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/localnet      ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/modules       ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/mountfs       ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/mountvirtfs   ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/network       ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/rc            ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/reboot        ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/sendsignals   ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/setclock      ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/swap          ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/sysctl        ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/sysklogd      ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/template      ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/udev          ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/init.d/udev_retry    ${ETCDIR}/rc.d/init.d/
	install -m ${MODE} lfs/sbin/ifup            ${SBIN}
	install -m ${MODE} lfs/sbin/ifdown          ${SBIN}
	install -m ${CONFMODE} lfs/sbin/ifup.8      ${MAN8}
	ln -sf  ifup.8                              ${MAN8}/ifdown.8
	install -m ${MODE}     lfs/lib/services/ipv4-static-route  ${SERVICEDIR}
	install -m ${MODE}     lfs/lib/services/ipv4-static        ${SERVICEDIR}
	install -m ${CONFMODE} lfs/lib/services/init-functions     ${SERVICEDIR}

	if [ ! -f ${ETCDIR}/sysconfig/createfiles ]; then \
          install -m ${CONFMODE} lfs/sysconfig/createfiles ${ETCDIR}/sysconfig/ ;\
        fi
	if [ ! -f ${ETCDIR}/sysconfig/modules     ]; then \
          install -m ${CONFMODE} lfs/sysconfig/modules     ${ETCDIR}/sysconfig/ ;\
        fi
	if [ ! -f ${ETCDIR}/sysconfig/udev_retry  ]; then \
          install -m ${CONFMODE} lfs/sysconfig/udev_retry  ${ETCDIR}/sysconfig/ ;\
        fi
	if [ ! -f ${ETCDIR}/sysconfig/rc.site     ]; then \
          install -m ${CONFMODE} lfs/sysconfig/rc.site     ${ETCDIR}/sysconfig/ ;\
        fi

links: rcS rc0 rc1 rc2 rc3 rc4 rc5 rc6

rcS: files
	ln -sf ../init.d/mountvirtfs ${ETCDIR}/rc.d/rcS.d/S00mountvirtfs
	ln -sf ../init.d/modules     ${ETCDIR}/rc.d/rcS.d/S05modules
	ln -sf ../init.d/localnet    ${ETCDIR}/rc.d/rcS.d/S08localnet
	ln -sf ../init.d/udev        ${ETCDIR}/rc.d/rcS.d/S10udev
	ln -sf ../init.d/swap        ${ETCDIR}/rc.d/rcS.d/S20swap
	ln -sf ../init.d/checkfs     ${ETCDIR}/rc.d/rcS.d/S30checkfs
	ln -sf ../init.d/mountfs     ${ETCDIR}/rc.d/rcS.d/S40mountfs
	ln -sf ../init.d/cleanfs     ${ETCDIR}/rc.d/rcS.d/S45cleanfs
	ln -sf ../init.d/udev_retry  ${ETCDIR}/rc.d/rcS.d/S50udev_retry
	ln -sf ../init.d/console     ${ETCDIR}/rc.d/rcS.d/S70console
	ln -sf ../init.d/sysctl      ${ETCDIR}/rc.d/rcS.d/S90sysctl

rc0: files
	ln -sf ../init.d/network     ${ETCDIR}/rc.d/rc0.d/K80network
	ln -sf ../init.d/sysklogd    ${ETCDIR}/rc.d/rc0.d/K90sysklogd
	ln -sf ../init.d/sendsignals ${ETCDIR}/rc.d/rc0.d/K92sendsignals
	ln -sf ../init.d/swap        ${ETCDIR}/rc.d/rc0.d/K94swap
	ln -sf ../init.d/mountfs     ${ETCDIR}/rc.d/rc0.d/K96mountfs
	ln -sf ../init.d/localnet    ${ETCDIR}/rc.d/rc0.d/K98localnet
	ln -sf ../init.d/halt        ${ETCDIR}/rc.d/rc0.d/S99halt

rc1: files
	ln -sf ../init.d/network     ${ETCDIR}/rc.d/rc1.d/K80network
	ln -sf ../init.d/sysklogd    ${ETCDIR}/rc.d/rc1.d/K90sysklogd

rc2: files
	ln -sf ../init.d/network     ${ETCDIR}/rc.d/rc2.d/S10network
	ln -sf ../init.d/sysklogd    ${ETCDIR}/rc.d/rc2.d/S20sysklogd

rc3: files
	ln -sf ../init.d/sysklogd    ${ETCDIR}/rc.d/rc3.d/S10sysklogd
	ln -sf ../init.d/network     ${ETCDIR}/rc.d/rc3.d/S20network

rc4: files
	ln -sf ../init.d/sysklogd    ${ETCDIR}/rc.d/rc4.d/S10sysklogd
	ln -sf ../init.d/network     ${ETCDIR}/rc.d/rc4.d/S20network

rc5: files
	ln -sf ../init.d/sysklogd    ${ETCDIR}/rc.d/rc5.d/S10sysklogd
	ln -sf ../init.d/network     ${ETCDIR}/rc.d/rc5.d/S20network

rc6: files
	ln -sf ../init.d/network     ${ETCDIR}/rc.d/rc6.d/K80network
	ln -sf ../init.d/sysklogd    ${ETCDIR}/rc.d/rc6.d/K90sysklogd
	ln -sf ../init.d/sendsignals ${ETCDIR}/rc.d/rc6.d/K92sendsignals
	ln -sf ../init.d/swap        ${ETCDIR}/rc.d/rc6.d/K94swap
	ln -sf ../init.d/mountfs     ${ETCDIR}/rc.d/rc6.d/K96mountfs
	ln -sf ../init.d/localnet    ${ETCDIR}/rc.d/rc6.d/K98localnet
	ln -sf ../init.d/reboot      ${ETCDIR}/rc.d/rc6.d/S99reboot

uninstall:
	rm -rf ${DESTDIR}/lib/services ${DESTDIR}/lib/lsb ${ETCDIR}/rc.d ${ETCDIR}/init.d \
          ${SBIN}/ifup ${SBIN}/ifdown ${MAN8}/ifup.8 ${MAN8}/ifdown.8 \
          ${ETCDIR}/sysconfig/rc

.PHONY: all create-dirs install files links rcS rc0 rc1 rc2 rc3 rc4 rc5 rc6 uninstall

