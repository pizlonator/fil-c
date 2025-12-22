/*
 * camconditionalaccess.h - CAM (EN50221) Conditional Access Resource
 * Copyright (C) 2007 Alessandro Decina
 *
 * Authors:
 *   Alessandro Decina <alessandro.d@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CAM_CONDITIONAL_ACCESS_H
#define CAM_CONDITIONAL_ACCESS_H

#include "camapplication.h"

#define CAM_CONDITIONAL_ACCESS(obj) ((CamConditionalAccess *) obj)

typedef enum
{
  CAM_CONDITIONAL_ACCESS_PMT_FLAG_MORE = 0,
  CAM_CONDITIONAL_ACCESS_PMT_FLAG_FIRST,
  CAM_CONDITIONAL_ACCESS_PMT_FLAG_LAST,
  CAM_CONDITIONAL_ACCESS_PMT_FLAG_ONLY,
  CAM_CONDITIONAL_ACCESS_PMT_FLAG_ADD,
  CAM_CONDITIONAL_ACCESS_PMT_FLAG_UPDATE,
} CamConditionalAccessPmtFlag;

typedef struct _CamConditionalAccess CamConditionalAccess;

struct _CamConditionalAccess
{
  CamALApplication application;
  gboolean ready;
};

CamConditionalAccess *cam_conditional_access_new (void);
void cam_conditional_access_destroy (CamConditionalAccess *cas);

CamReturn cam_conditional_access_set_pmt (CamConditionalAccess *cas,
  GstMpegtsPMT *pmt, CamConditionalAccessPmtFlag flag);

#endif /* CAM_CONDITIONAL_ACCESS_H */
