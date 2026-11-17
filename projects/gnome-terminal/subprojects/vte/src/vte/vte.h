/*
 * Copyright (C) 2001,2002,2003,2009,2010 Red Hat, Inc.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#define __VTE_VTE_H_INSIDE__ 1

/* This must always be included first */
#include "vtemacros.h"

#include "vteenums.h"
#include "vteglobals.h"
#include "vtepty.h"
#include "vteregex.h"
#include "vteterminal.h"
#include "vtetypebuiltins.h"
#include "vteuuid.h"
#include "vteversion.h"

#if !defined(VTE_DISABLE_DEPRECATED) || defined(VTE_COMPILATION)
#include "vtedeprecated.h"
#endif /* !VTE_DISABLE_DEPRECATED */

#undef __VTE_VTE_H_INSIDE__
