/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_SPM_H
#define RADV_SPM_H

#include "radv_device.h"
#include "radv_queue.h"
#include "radv_radeon_winsys.h"

void radv_emit_spm_setup(struct radv_device *device, struct radeon_cmdbuf *cs, enum radv_queue_family qf);

bool radv_spm_init(struct radv_device *device);

void radv_spm_finish(struct radv_device *device);

#endif /* RADV_SPM_H */
