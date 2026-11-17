/* cc-firmware-security-utils.h
 *
 * Copyright (C) 2021 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Kate Hsuan <hpa@redhat.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* we don't need to keep this up to date and from fwupd >= 1.8.3 we only need the defines
 * for the things we actually query, e.g. FWUPD_SECURITY_ATTR_ID_UEFI_SECUREBOOT */
#define FWUPD_SECURITY_ATTR_ID_ACPI_DMAR "org.fwupd.hsi.AcpiDmar"
#define FWUPD_SECURITY_ATTR_ID_ENCRYPTED_RAM "org.fwupd.hsi.EncryptedRam"
#define FWUPD_SECURITY_ATTR_ID_FWUPD_ATTESTATION "org.fwupd.hsi.Fwupd.Attestation"
#define FWUPD_SECURITY_ATTR_ID_FWUPD_PLUGINS "org.fwupd.hsi.Fwupd.Plugins"
#define FWUPD_SECURITY_ATTR_ID_FWUPD_UPDATES "org.fwupd.hsi.Fwupd.Updates"
#define FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_ENABLED "org.fwupd.hsi.IntelBootguard.Enabled"
#define FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_VERIFIED "org.fwupd.hsi.IntelBootguard.Verified"
#define FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_ACM "org.fwupd.hsi.IntelBootguard.Acm"
#define FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_POLICY "org.fwupd.hsi.IntelBootguard.Policy"
#define FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_OTP "org.fwupd.hsi.IntelBootguard.Otp"
#define FWUPD_SECURITY_ATTR_ID_INTEL_CET_ENABLED "org.fwupd.hsi.IntelCet.Enabled"
#define FWUPD_SECURITY_ATTR_ID_INTEL_CET_ACTIVE "org.fwupd.hsi.IntelCet.Active"
#define FWUPD_SECURITY_ATTR_ID_INTEL_SMAP "org.fwupd.hsi.IntelSmap"
#define FWUPD_SECURITY_ATTR_ID_IOMMU "org.fwupd.hsi.Iommu"
#define FWUPD_SECURITY_ATTR_ID_KERNEL_LOCKDOWN "org.fwupd.hsi.Kernel.Lockdown"
#define FWUPD_SECURITY_ATTR_ID_KERNEL_SWAP "org.fwupd.hsi.Kernel.Swap"
#define FWUPD_SECURITY_ATTR_ID_KERNEL_TAINTED "org.fwupd.hsi.Kernel.Tainted"
#define FWUPD_SECURITY_ATTR_ID_MEI_MANUFACTURING_MODE "org.fwupd.hsi.Mei.ManufacturingMode"
#define FWUPD_SECURITY_ATTR_ID_MEI_OVERRIDE_STRAP "org.fwupd.hsi.Mei.OverrideStrap"
#define FWUPD_SECURITY_ATTR_ID_MEI_VERSION "org.fwupd.hsi.Mei.Version"
#define FWUPD_SECURITY_ATTR_ID_SPI_BIOSWE "org.fwupd.hsi.Spi.Bioswe"
#define FWUPD_SECURITY_ATTR_ID_SPI_BLE "org.fwupd.hsi.Spi.Ble"
#define FWUPD_SECURITY_ATTR_ID_SPI_SMM_BWP "org.fwupd.hsi.Spi.SmmBwp"
#define FWUPD_SECURITY_ATTR_ID_SPI_DESCRIPTOR "org.fwupd.hsi.Spi.Descriptor"
#define FWUPD_SECURITY_ATTR_ID_SUSPEND_TO_IDLE "org.fwupd.hsi.SuspendToIdle"
#define FWUPD_SECURITY_ATTR_ID_SUSPEND_TO_RAM "org.fwupd.hsi.SuspendToRam"
#define FWUPD_SECURITY_ATTR_ID_TPM_EMPTY_PCR "org.fwupd.hsi.Tpm.EmptyPcr"
#define FWUPD_SECURITY_ATTR_ID_TPM_RECONSTRUCTION_PCR0 "org.fwupd.hsi.Tpm.ReconstructionPcr0"
#define FWUPD_SECURITY_ATTR_ID_TPM_VERSION_20 "org.fwupd.hsi.Tpm.Version20"
#define FWUPD_SECURITY_ATTR_ID_UEFI_SECUREBOOT "org.fwupd.hsi.Uefi.SecureBoot"
#define FWUPD_SECURITY_ATTR_ID_INTEL_DCI_ENABLED "org.fwupd.hsi.IntelDci.Enabled"
#define FWUPD_SECURITY_ATTR_ID_INTEL_DCI_LOCKED "org.fwupd.hsi.IntelDci.Locked"
#define FWUPD_SECURITY_ATTR_ID_UEFI_PK "org.fwupd.hsi.Uefi.Pk"
#define FWUPD_SECURITY_ATTR_ID_PREBOOT_DMA_PROTECTION "org.fwupd.hsi.PrebootDma"
#define FWUPD_SECURITY_ATTR_ID_SUPPORTED_CPU "org.fwupd.hsi.SupportedCpu"
#define FWUPD_SECURITY_ATTR_ID_PLATFORM_DEBUG_LOCKED "org.fwupd.hsi.PlatformDebugLocked"
#define FWUPD_SECURITY_ATTR_ID_AMD_ROLLBACK_PROTECTION "org.fwupd.hsi.Amd.RollbackProtection"
#define FWUPD_SECURITY_ATTR_ID_AMD_SPI_WRITE_PROTECTION "org.fwupd.hsi.Amd.SpiWriteProtection"
#define FWUPD_SECURITY_ATTR_ID_AMD_SPI_REPLAY_PROTECTION "org.fwupd.hsi.Amd.SpiReplayProtection"
#define FWUPD_SECURITY_ATTR_ID_PLATFORM_DEBUG_ENABLED "org.fwupd.hsi.PlatformDebugEnabled"
#define FWUPD_SECURITY_ATTR_ID_PLATFORM_FUSED "org.fwupd.hsi.PlatformFused"

typedef enum {
  SECURE_BOOT_STATE_UNKNOWN,
  SECURE_BOOT_STATE_ACTIVE,
  SECURE_BOOT_STATE_INACTIVE,
  SECURE_BOOT_STATE_PROBLEMS,
} SecureBootState;

typedef enum {
  FWUPD_SECURITY_ATTR_FLAG_NONE = 0,
  FWUPD_SECURITY_ATTR_FLAG_SUCCESS = 1 << 0,
  FWUPD_SECURITY_ATTR_FLAG_OBSOLETED = 1 << 1,
  FWUPD_SECURITY_ATTR_FLAG_RUNTIME_UPDATES = 1 << 8,
  FWUPD_SECURITY_ATTR_FLAG_RUNTIME_ATTESTATION = 1 << 9,
  FWUPD_SECURITY_ATTR_FLAG_RUNTIME_ISSUE = 1 << 10,
  FWUPD_SECURITY_ATTR_FLAG_ACTION_CONTACT_OEM = 1 << 11,
  FWUPD_SECURITY_ATTR_FLAG_ACTION_CONFIG_FW = 1 << 12,
  FWUPD_SECURITY_ATTR_FLAG_ACTION_CONFIG_OS = 1 << 13,
} FwupdSecurityAttrFlags;

typedef enum {
  FWUPD_SECURITY_ATTR_RESULT_UNKNOWN,
  FWUPD_SECURITY_ATTR_RESULT_ENABLED,
  FWUPD_SECURITY_ATTR_RESULT_NOT_ENABLED,
  FWUPD_SECURITY_ATTR_RESULT_VALID,
  FWUPD_SECURITY_ATTR_RESULT_NOT_VALID,
  FWUPD_SECURITY_ATTR_RESULT_LOCKED,
  FWUPD_SECURITY_ATTR_RESULT_NOT_LOCKED,
  FWUPD_SECURITY_ATTR_RESULT_ENCRYPTED,
  FWUPD_SECURITY_ATTR_RESULT_NOT_ENCRYPTED,
  FWUPD_SECURITY_ATTR_RESULT_TAINTED,
  FWUPD_SECURITY_ATTR_RESULT_NOT_TAINTED,
  FWUPD_SECURITY_ATTR_RESULT_FOUND,
  FWUPD_SECURITY_ATTR_RESULT_NOT_FOUND,
  FWUPD_SECURITY_ATTR_RESULT_SUPPORTED,
  FWUPD_SECURITY_ATTR_RESULT_NOT_SUPPORTED,
  FWUPD_SECURITY_ATTR_RESULT_LAST
} FwupdSecurityAttrResult;

typedef struct {
  FwupdSecurityAttrResult  result;
  FwupdSecurityAttrResult  result_fallback;
  FwupdSecurityAttrFlags   flags;
  guint32                  hsi_level;
  guint64                  timestamp;
  gchar                   *appstream_id;
  gchar                   *title;
  gchar                   *description;
} FwupdSecurityAttr;

FwupdSecurityAttr *fu_security_attr_new_from_variant  (GVariantIter *iter);
void               fu_security_attr_free              (FwupdSecurityAttr *attr);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FwupdSecurityAttr, fu_security_attr_free)

gboolean     firmware_security_attr_has_flag                    (FwupdSecurityAttr       *attr,
                                                                 FwupdSecurityAttrFlags   flag);
void         load_custom_css                                    (const char              *path);
const gchar *fwupd_security_attr_result_to_string               (FwupdSecurityAttrResult  result);
gboolean     fwupd_get_result_status                            (FwupdSecurityAttrResult  result);
void         hsi_report_title_print_padding                     (const gchar *title, GString *dst_string, gsize maxlen);

G_END_DECLS
