#
# Makefile for libcap documentation
#

topdir=$(shell pwd)/..
include $(topdir)/Make.Rules

MAN1S = capsh.1
MAN3S = cap_init.3 cap_free.3 cap_dup.3 \
	cap_clear.3 cap_clear_flag.3 cap_get_flag.3 cap_set_flag.3 \
	cap_fill.3 cap_fill_flag.3 cap_max_bits.3 \
	cap_compare.3 cap_get_proc.3 cap_get_pid.3 cap_set_proc.3 \
	cap_get_file.3 cap_get_fd.3 cap_set_file.3 cap_set_fd.3 \
	cap_set_nsowner.3 cap_get_nsowner.3 \
	cap_copy_ext.3 cap_size.3 cap_copy_int.3 cap_mode.3 \
	cap_copy_int_check.3 cap_set_syscall.3 \
	cap_from_text.3 cap_to_text.3 cap_from_name.3 cap_to_name.3 \
	capsetp.3 capgetp.3 libcap.3 \
	cap_get_bound.3 cap_drop_bound.3 \
	cap_get_mode.3 cap_set_mode.3 cap_mode_name.3 \
	cap_get_secbits.3 cap_set_secbits.3 \
	cap_setuid.3 cap_setgroups.3 \
	cap_launch.3 cap_func_launcher.3 cap_launcher_callback.3 \
	cap_launcher_set_chroot.3 cap_launcher_set_mode.3 \
	cap_launcher_setgroups.3 cap_launcher_setuid.3 \
	cap_launcher_set_iab.3 cap_new_launcher.3 \
	cap_iab.3 cap_iab_init.3 cap_iab_dup.3 cap_iab_compare.3 \
	cap_iab_get_proc.3 cap_iab_get_pid.3 cap_iab_set_proc.3 \
	cap_iab_to_text.3 cap_iab_from_text.3 cap_iab_get_vector.3 \
	cap_iab_set_vector.3 cap_iab_fill.3 cap_proc_root.3 \
	cap_prctl.3 cap_prctlw.3 \
	psx_syscall.3 psx_syscall3.3 psx_syscall6.3 psx_set_sensitivity.3 \
	psx_load_syscalls.3 __psx_syscall.3 \
	libpsx.3
MAN5S = capability.conf.5
MAN8S = getcap.8 setcap.8 getpcaps.8 captree.8 pam_cap.8

MANS = $(MAN1S) $(MAN3S) $(MAN5S) $(MAN8S)

all: $(MANS)

test:
	@echo no doc tests available

sudotest:
	@echo no doc sudotests available

.PHONY: html
html:
	mkdir -p html
	for man in $(MANS) ; \
	do \
		$(BUILD_EGREP) '^\.so man' $$man > /dev/null || \
		groff -man -Thtml $$man > html/$$man.html ; \
	done

install:
	mkdir -p -m 755 $(FAKEROOT)$(MANDIR)/man1 $(FAKEROOT)$(MANDIR)/man3 $(FAKEROOT)$(MANDIR)/man5 $(FAKEROOT)$(MANDIR)/man8
	for man in \
		$(FAKEROOT)$(MANDIR)/man1 $(MAN1S) \
		$(FAKEROOT)$(MANDIR)/man3 $(MAN3S) \
		$(FAKEROOT)$(MANDIR)/man5 $(MAN5S) \
		$(FAKEROOT)$(MANDIR)/man8 $(MAN8S) \
		; \
	do \
		case $$man in \
		/*)	sub=$$man ; continue ;; \
		esac; \
		install -m 644 $$man $$sub ; \
	done

clean:
	$(LOCALCLEAN)
	rm -rf html
