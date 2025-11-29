/* gtkaccessiblerangeprivate.h: Accessible range private API
 *
 * SPDX-FileCopyrightText: 2022  Emmanuele Bassi
 * SPDX-FileCopyrightText: 2022  Red Hat Inc
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gtkaccessiblerange.h"

G_BEGIN_DECLS

gboolean
gtk_accessible_range_set_current_value (GtkAccessibleRange *self,
                                        double              value);

G_END_DECLS
