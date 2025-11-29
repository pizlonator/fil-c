 /*
  * Copyright © 2024 Intel Corporation
  *
  * Permission is hereby granted, free of charge, to any person obtaining a
  * copy of this software and associated documentation files (the "Software"),
  * to deal in the Software without restriction, including without limitation
  * the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the
  * Software is furnished to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice (including the next
  * paragraph) shall be included in all copies or substantial portions of the
  * Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
  * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  * IN THE SOFTWARE.
  *
  */

#ifndef INTEL_DEVICE_INFO_SERIALIZE_H
#define INTEL_DEVICE_INFO_SERIALIZE_H

#include "dev/intel_device_info.h"
#include "util/parson.h"

/* Initialize a device info struct with the json-formatted data at the path.
 * Returns false if json format is incompatible (from an older version of
 * mesa)
 */
bool intel_device_info_from_json(const char *path,
                                 struct intel_device_info *devinfo);

/* Dump all device info data into a parson tree, for serializaiton.  Caller
 * must free the returned reference with json_value_free()
 */
JSON_Value *intel_device_info_dump_json(const struct intel_device_info *devinfo);

#endif /* INTEL_DEVICE_INFO_SERIALIZE_H */
