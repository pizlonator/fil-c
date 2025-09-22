/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2008 Cai Qian <qcai@redhat.com>
 * Copyright (C) 2008-2023 Karel Zak <kzak@redhat.com>
 */
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#include "lscpu.h"

#if (defined(__x86_64__) || defined(__i386__))
# define INCLUDE_VMWARE_BDOOR
#endif

#ifdef INCLUDE_VMWARE_BDOOR
# include <stdint.h>
# include <strings.h>
# include <setjmp.h>
# ifdef HAVE_SYS_IO_H
#  include <sys/io.h>
# endif
#endif

/* Xen Domain feature flag used for /sys/hypervisor/properties/features */
#define XENFEAT_supervisor_mode_kernel		3
#define XENFEAT_mmu_pt_update_preserve_ad	5
#define XENFEAT_hvm_callback_vector			8

#define XEN_FEATURES_PV_MASK	(1U << XENFEAT_mmu_pt_update_preserve_ad)
#define XEN_FEATURES_PVH_MASK	( (1U << XENFEAT_supervisor_mode_kernel) \
								| (1U << XENFEAT_hvm_callback_vector) )
static const int hv_vendor_pci[] = {
	[VIRT_VENDOR_NONE]	= 0x0000,
	[VIRT_VENDOR_XEN]	= 0x5853,
	[VIRT_VENDOR_KVM]	= 0x0000,
	[VIRT_VENDOR_MSHV]	= 0x1414,
	[VIRT_VENDOR_VMWARE]	= 0x15ad,
	[VIRT_VENDOR_VBOX]	= 0x80ee,
};

static const int hv_graphics_pci[] = {
	[VIRT_VENDOR_NONE]	= 0x0000,
	[VIRT_VENDOR_XEN]	= 0x0001,
	[VIRT_VENDOR_KVM]	= 0x0000,
	[VIRT_VENDOR_MSHV]	= 0x5353,
	[VIRT_VENDOR_VMWARE]	= 0x0710,
	[VIRT_VENDOR_VBOX]	= 0xbeef,
};

#define WORD(x) (uint16_t)(*(const uint16_t *)(x))
#define DWORD(x) (uint32_t)(*(const uint32_t *)(x))

void *get_mem_chunk(size_t base, size_t len, const char *devmem)
{
	void *p = NULL;
	int fd;

	if ((fd = open(devmem, O_RDONLY)) < 0)
		return NULL;

	if (!(p = malloc(len)))
		goto nothing;
	if (lseek(fd, base, SEEK_SET) == -1)
		goto nothing;
	if (read_all(fd, p, len) == -1)
		goto nothing;

	close(fd);
	return p;

nothing:
	free(p);
	close(fd);
	return NULL;
}

static int hypervisor_from_dmi_table(uint32_t base, uint16_t len,
				uint16_t num, const char *devmem)
{
	uint8_t *data;
	int rc = VIRT_VENDOR_NONE;
	struct dmi_info di;

	data = get_mem_chunk(base, len, devmem);
	if (!data)
		return rc;

	memset(&di, 0, sizeof(struct dmi_info));
	rc = parse_dmi_table(len, num, data, &di);
	if (rc < 0)
		goto done;

	if (di.manufacturer && !strcmp(di.manufacturer, "innotek GmbH"))
		rc = VIRT_VENDOR_INNOTEK;
	else if (di.manufacturer && strstr(di.manufacturer, "HITACHI") &&
					di.product && strstr(di.product, "LPAR"))
		rc = VIRT_VENDOR_HITACHI;
	else if (di.vendor && !strcmp(di.vendor, "Parallels"))
		rc = VIRT_VENDOR_PARALLELS;
done:
	free(data);
	return rc;
}

static int checksum(const uint8_t *buf, size_t len)
{
	uint8_t sum = 0;
	size_t a;

	for (a = 0; a < len; a++)
		sum += buf[a];
	return (sum == 0);
}

#if defined(__x86_64__) || defined(__i386__)
static int hypervisor_decode_legacy(uint8_t *buf, const char *devmem)
{
	if (!checksum(buf, 0x0F))
		return -1;

	return hypervisor_from_dmi_table(DWORD(buf + 0x08), WORD(buf + 0x06),
			 WORD(buf + 0x0C),
		devmem);
}
#endif

static int hypervisor_decode_smbios(uint8_t *buf, const char *devmem)
{
	if (!checksum(buf, buf[0x05])
	    || memcmp(buf + 0x10, "_DMI_", 5) != 0
	    || !checksum(buf + 0x10, 0x0F))
		return -1;

	return hypervisor_from_dmi_table(DWORD(buf + 0x18), WORD(buf + 0x16),
			 WORD(buf + 0x1C),
		devmem);
}

/*
 * Probe for EFI interface
 */
#define EFI_NOT_FOUND   (-1)
#define EFI_NO_SMBIOS   (-2)
static int address_from_efi(size_t *address)
{
	FILE *tab;
	char linebuf[64];
	int ret;

	*address = 0; /* Prevent compiler warning */

	/*
	 * Linux up to 2.6.6: /proc/efi/systab
	 * Linux 2.6.7 and up: /sys/firmware/efi/systab
	 */
	if (!(tab = fopen("/sys/firmware/efi/systab", "r")) &&
	    !(tab = fopen("/proc/efi/systab", "r")))
		return EFI_NOT_FOUND;		/* No EFI interface */

	ret = EFI_NO_SMBIOS;
	while ((fgets(linebuf, sizeof(linebuf) - 1, tab)) != NULL) {
		char *addrp = strchr(linebuf, '=');
		if (!addrp)
			continue;
		*(addrp++) = '\0';
		if (strcmp(linebuf, "SMBIOS") == 0) {
			errno = 0;
			*address = strtoul(addrp, NULL, 0);
			if (errno)
				continue;
			ret = 0;
			break;
		}
	}

	fclose(tab);
	return ret;
}

static int read_hypervisor_dmi_from_devmem(void)
{
	int rc = VIRT_VENDOR_NONE;
	uint8_t *buf = NULL;
	size_t fp = 0;

	/* First try EFI (ia64, Intel-based Mac) */
	switch (address_from_efi(&fp)) {
		case EFI_NOT_FOUND:
			goto memory_scan;
		case EFI_NO_SMBIOS:
			goto done;
	}

	buf = get_mem_chunk(fp, 0x20, _PATH_DEV_MEM);
	if (!buf)
		goto done;

	rc = hypervisor_decode_smbios(buf, _PATH_DEV_MEM);
	if (rc >= VIRT_VENDOR_NONE)
		goto done;

	free(buf);
	buf = NULL;
memory_scan:
#if defined(__x86_64__) || defined(__i386__)
	/* Fallback to memory scan (x86, x86_64) */
	buf = get_mem_chunk(0xF0000, 0x10000, _PATH_DEV_MEM);
	if (!buf)
		goto done;

	for (fp = 0; fp <= 0xFFF0; fp += 16) {
		if (memcmp(buf + fp, "_SM_", 4) == 0 && fp <= 0xFFE0) {
			rc = hypervisor_decode_smbios(buf + fp, _PATH_DEV_MEM);
			if (rc < 0)
				fp += 16;

		} else if (memcmp(buf + fp, "_DMI_", 5) == 0)
			rc = hypervisor_decode_legacy(buf + fp, _PATH_DEV_MEM);

		if (rc >= VIRT_VENDOR_NONE)
			break;
	}
#endif
done:
	free(buf);
	return rc;
}

static int read_hypervisor_dmi_from_sysfw(void)
{
	static char const sys_fw_dmi_tables[] = _PATH_SYS_DMI;
	struct stat st;

	if (stat(sys_fw_dmi_tables, &st))
		return -1;

	return hypervisor_from_dmi_table(0, st.st_size, st.st_size / 4,
					 sys_fw_dmi_tables);
}

static int read_hypervisor_dmi(void)
{
	int rc;

	if (sizeof(uint8_t) != 1
	    || sizeof(uint16_t) != 2
	    || sizeof(uint32_t) != 4
	    || '\0' != 0)
		return VIRT_VENDOR_NONE;

	/* -1 : no DMI in /sys,
	 *  0 : DMI exist, nothing detected (VIRT_VENDOR_NONE)
	 * >0 : hypervisor detected
	 */
	rc = read_hypervisor_dmi_from_sysfw();
	if (rc < 0)
		rc = read_hypervisor_dmi_from_devmem();

	return rc < 0 ? VIRT_VENDOR_NONE : rc;
}

static int has_pci_device(struct lscpu_cxt *cxt,
			unsigned int vendor, unsigned int device)
{
	FILE *f;
	unsigned int num, fn, ven, dev;
	int res = 1;

	f = ul_path_fopen(cxt->procfs, "r", "bus/pci/devices");
	if (!f)
		return 0;

	 /* for more details about bus/pci/devices format see
	  * drivers/pci/proc.c in linux kernel
	  */
	while(fscanf(f, "%02x%02x\t%04x%04x\t%*[^\n]",
			&num, &fn, &ven, &dev) == 4) {

		if (ven == vendor && dev == device)
			goto found;
	}

	res = 0;
found:
	fclose(f);
	return res;
}

#if defined(__x86_64__) || defined(__i386__)
/*
 * This CPUID leaf returns the information about the hypervisor.
 * EAX : maximum input value for CPUID supported by the hypervisor.
 * EBX, ECX, EDX : Hypervisor vendor ID signature. E.g. VMwareVMware.
 */
#define HYPERVISOR_INFO_LEAF   0x40000000

static inline void cpuid(unsigned int op, unsigned int *eax,
			 unsigned int *ebx, unsigned int *ecx,
			 unsigned int *edx)
{
	__asm__(
#if defined(__PIC__) && defined(__i386__)
		/* x86 PIC cannot clobber ebx -- gcc bitches */
		"xchg %%ebx, %%esi;"
		"cpuid;"
		"xchg %%esi, %%ebx;"
		: "=S" (*ebx),
#else
		"cpuid;"
		: "=b" (*ebx),
#endif
		  "=a" (*eax),
		  "=c" (*ecx),
		  "=d" (*edx)
		: "1" (op), "c"(0));
}

static int read_hypervisor_cpuid(void)
{
	unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
	char hyper_vendor_id[13] = { 0 };

	cpuid(HYPERVISOR_INFO_LEAF, &eax, &ebx, &ecx, &edx);
	memcpy(hyper_vendor_id + 0, &ebx, 4);
	memcpy(hyper_vendor_id + 4, &ecx, 4);
	memcpy(hyper_vendor_id + 8, &edx, 4);
	hyper_vendor_id[12] = '\0';

	if (!hyper_vendor_id[0])
		goto none;

	if (!strncmp("XenVMMXenVMM", hyper_vendor_id, 12))
		return VIRT_VENDOR_XEN;
	else if (!strncmp("KVMKVMKVM", hyper_vendor_id, 9))
		return VIRT_VENDOR_KVM;
	else if (!strncmp("Microsoft Hv", hyper_vendor_id, 12))
		return VIRT_VENDOR_MSHV;
	else if (!strncmp("VMwareVMware", hyper_vendor_id, 12))
		return VIRT_VENDOR_VMWARE;
	else if (!strncmp("UnisysSpar64", hyper_vendor_id, 12))
		return VIRT_VENDOR_SPAR;
none:
	return VIRT_VENDOR_NONE;
}

#else /* ! (__x86_64__ || __i386__) */
static int read_hypervisor_cpuid(void)
{
	return 0;
}
#endif

static int is_devtree_compatible(struct lscpu_cxt *cxt, const char *str)
{
	FILE *fd = ul_path_fopen(cxt->procfs, "r", "device-tree/compatible");

	if (fd) {
		char buf[256];
		size_t i, len;

		memset(buf, 0, sizeof(buf));
		len = fread(buf, 1, sizeof(buf) - 1, fd);
		fclose(fd);

		for (i = 0; i < len;) {
			if (!strcmp(&buf[i], str))
				return 1;
			i += strlen(&buf[i]);
			i++;
		}
	}

	return 0;
}

static int read_hypervisor_powerpc(struct lscpu_cxt *cxt, int *type)
{
	int vendor = VIRT_VENDOR_NONE;

	*type = VIRT_TYPE_NONE;

	 /* IBM iSeries: legacy, para-virtualized on top of OS/400 */
	if (ul_path_access(cxt->procfs, F_OK, "iSeries") == 0) {
		vendor = VIRT_VENDOR_OS400;
		*type = VIRT_TYPE_PARA;

	/* PowerNV (POWER Non-Virtualized, bare-metal) */
	} else if (is_devtree_compatible(cxt, "ibm,powernv") != 0) {
		;

	/* PowerVM (IBM's proprietary hypervisor, aka pHyp) */
	} else if (ul_path_access(cxt->procfs, F_OK, "device-tree/ibm,partition-name") == 0
		   && ul_path_access(cxt->procfs, F_OK, "device-tree/hmc-managed?") == 0
		   && ul_path_access(cxt->procfs, F_OK, "device-tree/chosen/qemu,graphic-width") != 0) {

		char buf[256];
		vendor = VIRT_VENDOR_PHYP;
		*type = VIRT_TYPE_PARA;

		if (ul_path_scanf(cxt->procfs, "device-tree/ibm,partition-name", "%255s", buf) == 1 &&
		    !strcmp(buf, "full"))
			*type = VIRT_TYPE_NONE;

	/* Qemu */
	} else if (is_devtree_compatible(cxt, "qemu,pseries")) {
		vendor = VIRT_VENDOR_KVM;
		*type = VIRT_TYPE_PARA;
	}

	return vendor;
}

#ifdef INCLUDE_VMWARE_BDOOR

#define VMWARE_BDOOR_MAGIC          0x564D5868
#define VMWARE_BDOOR_PORT           0x5658
#define VMWARE_BDOOR_CMD_GETVERSION 10

static UL_ASAN_BLACKLIST
void vmware_bdoor(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	__asm__(
#if defined(__PIC__) && defined(__i386__)
		/* x86 PIC cannot clobber ebx -- gcc bitches */
		"xchg %%ebx, %%esi;"
		"inl (%%dx), %%eax;"
		"xchg %%esi, %%ebx;"
		: "=S" (*ebx),
#else
		"inl (%%dx), %%eax;"
		: "=b" (*ebx),
#endif
		  "=a" (*eax),
		  "=c" (*ecx),
		  "=d" (*edx)
		: "0" (VMWARE_BDOOR_MAGIC),
		  "1" (VMWARE_BDOOR_CMD_GETVERSION),
		  "2" (VMWARE_BDOOR_PORT),
		  "3" (0)
		: "memory");
}

static jmp_buf segv_handler_env;
static sigset_t oset;

static void
segv_handler(__attribute__((__unused__)) int sig,
             __attribute__((__unused__)) siginfo_t *info,
             __attribute__((__unused__)) void *ignored)
{
	siglongjmp(segv_handler_env, 1);
}

static int is_vmware_platform(void)
{
	uint32_t eax, ebx, ecx, edx;
	struct sigaction act, oact;
	sigset_t set;

	/*
	 * FIXME: Not reliable for non-root users. Note it works as expected if
	 * vmware_bdoor() is not optimized for PIE, but then it fails to build
	 * on 32bit x86 systems. See lscpu git log for more details (commit
	 * 7845b91dbc7690064a2be6df690e4aaba728fb04).     kzak [3-Nov-2016]
	 */
	if (getuid() != 0)
		return 0;

	/*
	 * The assembly routine for vmware detection works
	 * fine under vmware, even if ran as regular user. But
	 * on real HW or under other hypervisors, it segfaults (which is
	 * expected). So we temporarily install SIGSEGV handler to catch
	 * the signal. All this magic is needed because lscpu
	 * isn't supposed to require root privileges.
	 */
	if (sigsetjmp(segv_handler_env, 1)) {
		if (sigprocmask(SIG_SETMASK, &oset, NULL))
			err(EXIT_FAILURE, _("cannot restore signal mask"));
		return 0;
	}

	sigemptyset(&set);
	sigaddset(&set, SIGSEGV);
	if (sigprocmask(SIG_UNBLOCK, &set, &oset))
		err(EXIT_FAILURE, _("cannot unblock signal"));

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = segv_handler;
	act.sa_flags = SA_SIGINFO;

	if (sigaction(SIGSEGV, &act, &oact))
		err(EXIT_FAILURE, _("cannot set signal handler"));

	vmware_bdoor(&eax, &ebx, &ecx, &edx);

	if (sigaction(SIGSEGV, &oact, NULL))
		err(EXIT_FAILURE, _("cannot restore signal handler"));

	if (sigprocmask(SIG_SETMASK, &oset, NULL))
		err(EXIT_FAILURE, _("cannot restore signal mask"));

	return eax != (uint32_t)-1 && ebx == VMWARE_BDOOR_MAGIC;
}

#else /* ! INCLUDE_VMWARE_BDOOR */

static int is_vmware_platform(void)
{
	return 0;
}

#endif /* INCLUDE_VMWARE_BDOOR */
struct lscpu_virt *lscpu_read_virtualization(struct lscpu_cxt *cxt)
{
	char buf[BUFSIZ];
	struct lscpu_cputype *ct;
	struct lscpu_virt *virt;
	FILE *fd;

	DBG(VIRT, ul_debug("reading virtualization"));
	virt = xcalloc(1, sizeof(*virt));

	/* CPU flags */
	ct = lscpu_cputype_get_default(cxt);
	if (ct && ct->flags) {
		snprintf(buf, sizeof(buf), " %s ", ct->flags);
		if (strstr(buf, " svm "))
			virt->cpuflag = xstrdup("svm");
		else if (strstr(buf, " vmx "))
			virt->cpuflag = xstrdup("vmx");
	}


	/* We have to detect WSL first. is_vmware_platform() crashes on Windows 10. */
	fd = ul_path_fopen(cxt->procfs, "r", "sys/kernel/osrelease");
	if (fd) {
		if (fgets(buf, sizeof(buf), fd) && strstr(buf, "Microsoft")) {
			virt->vendor = VIRT_VENDOR_WSL;
			virt->type = VIRT_TYPE_CONTAINER;
		}
		fclose(fd);
		if (virt->type)
			goto done;
	}

	if (!cxt->noalive) {
		virt->vendor = read_hypervisor_cpuid();
		if (!virt->vendor)
			virt->vendor = read_hypervisor_dmi();
		if (!virt->vendor && is_vmware_platform())
			virt->vendor = VIRT_VENDOR_VMWARE;
	}

	if (virt->vendor) {
		virt->type = VIRT_TYPE_FULL;

		if (virt->vendor == VIRT_VENDOR_XEN) {
			uint32_t features;

			if (ul_path_scanf(cxt->rootfs, _PATH_SYS_HYP_FEATURES, "%x", &features) == 1) {
				/* Xen PV domain */
				if (features & XEN_FEATURES_PV_MASK)
					virt->type = VIRT_TYPE_PARA;
				/* Xen PVH domain */
				else if ((features & XEN_FEATURES_PVH_MASK)
								== XEN_FEATURES_PVH_MASK)
					virt->type = VIRT_TYPE_PARA;
			}
		}
	} else if ((virt->vendor = read_hypervisor_powerpc(cxt, &virt->type))) {
		;

	/* Xen para-virt or dom0 */
	} else if (ul_path_access(cxt->procfs, F_OK, "xen") == 0) {
		char xenbuf[256];
		int dom0 = 0;

		if (ul_path_scanf(cxt->procfs, "xen/capabilities", "%255s", xenbuf) == 1 &&
		    !strcmp(xenbuf, "control_d"))
			dom0 = 1;
		virt->type = dom0 ? VIRT_TYPE_NONE : VIRT_TYPE_PARA;
		virt->vendor = VIRT_VENDOR_XEN;

	/* Xen full-virt on non-x86_64 */
	} else if (has_pci_device(cxt, hv_vendor_pci[VIRT_VENDOR_XEN], hv_graphics_pci[VIRT_VENDOR_XEN])) {
		virt->vendor = VIRT_VENDOR_XEN;
		virt->type = VIRT_TYPE_FULL;
	} else if (has_pci_device(cxt, hv_vendor_pci[VIRT_VENDOR_VMWARE], hv_graphics_pci[VIRT_VENDOR_VMWARE])) {
		virt->vendor = VIRT_VENDOR_VMWARE;
		virt->type = VIRT_TYPE_FULL;
	} else if (has_pci_device(cxt, hv_vendor_pci[VIRT_VENDOR_VBOX], hv_graphics_pci[VIRT_VENDOR_VBOX])) {
		virt->vendor = VIRT_VENDOR_VBOX;
		virt->type = VIRT_TYPE_FULL;

	/* IBM PR/SM */
	} else if ((fd = ul_path_fopen(cxt->procfs, "r", "sysinfo"))) {

		virt->vendor = VIRT_VENDOR_IBM;
		virt->hypervisor = "PR/SM";
		virt->type = VIRT_TYPE_FULL;

		while (fgets(buf, sizeof(buf), fd) != NULL) {
			if (!strstr(buf, "Control Program:"))
				continue;
			virt->vendor = strstr(buf, "KVM") ? VIRT_VENDOR_KVM : VIRT_VENDOR_IBM;
			virt->hypervisor = strchr(buf, ':');

			if (virt->hypervisor) {
				virt->hypervisor++;
				normalize_whitespace((unsigned char *) virt->hypervisor);
				break;
			}
		}
		if (virt->hypervisor)
			virt->hypervisor = xstrdup(virt->hypervisor);
		fclose(fd);
	}

	/* OpenVZ/Virtuozzo - /proc/vz dir should exist
	 *		      /proc/bc should not */
	else if (ul_path_access(cxt->procfs, F_OK, "vz") == 0 &&
		 ul_path_access(cxt->procfs, F_OK, "bc") != 0) {
		virt->vendor = VIRT_VENDOR_PARALLELS;
		virt->type = VIRT_TYPE_CONTAINER;

	/* IBM */
	} else if (virt->hypervisor &&
		 (strcmp(virt->hypervisor, "PowerVM Lx86") == 0 ||
		  strcmp(virt->hypervisor, "IBM/S390") == 0)) {
		virt->vendor = VIRT_VENDOR_IBM;
		virt->type = VIRT_TYPE_FULL;

	/* User-mode-linux */
	} else if (ct && ct->modelname && strstr(ct->modelname, "UML")) {
		virt->vendor = VIRT_VENDOR_UML;
		virt->type = VIRT_TYPE_PARA;

	/* Linux-VServer */
	} else if ((fd = ul_path_fopen(cxt->procfs, "r", "self/status"))) {
		char *val = NULL;

		while (fgets(buf, sizeof(buf), fd) != NULL) {
			if (lookup(buf, "VxID", &val))
				break;
		}
		fclose(fd);

		if (val) {
			char *org = val;

			while (isdigit(*val))
				++val;
			if (!*val) {
				virt->vendor = VIRT_VENDOR_VSERVER;
				virt->type = VIRT_TYPE_CONTAINER;
			}
			free(org);
		}
	}
done:
	DBG(VIRT, ul_debugobj(virt, "virt: cpu='%s' hypervisor='%s' vendor=%d type=%d",
				virt->cpuflag,
				virt->hypervisor,
				virt->vendor,
				virt->type));

	if (!virt->cpuflag && !virt->hypervisor && !virt->vendor && !virt->type) {
		lscpu_free_virtualization(virt);
		virt = NULL;
	}
	return virt;
}

void lscpu_free_virtualization(struct lscpu_virt *virt)
{
	if (!virt)
		return;

	free(virt->cpuflag);
	free(virt->hypervisor);
	free(virt);
}

