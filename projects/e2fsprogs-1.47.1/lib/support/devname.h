/*
 * devname.c --- Figure out if a pathname is ext* or something else.
 *
 * Copyright (C) 2022 Red Hat, Inc., Lukas Czerner <lczerner@redhat.com>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#ifndef DEVNAME_H_
#define DEVNAME_H_

#include "blkid/blkid.h"

char *get_devname(blkid_cache cache, const char *token, const char *value);

#endif /* DEVNAME_H_ */
