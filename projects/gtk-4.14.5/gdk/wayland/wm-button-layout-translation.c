/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author:  Florian Müllner <fmuellner@gnome.org>
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "wm-button-layout-translation.h"

static void
translate_buttons (char *layout, int *len_p)
{
  char *strp = layout, *button;
  int len = 0;

  if (!layout || !*layout)
    goto out;

  while ((button = strsep (&strp, ",")))
    {
      const char *gtkbutton;

      if (strcmp (button, "menu") == 0)
        gtkbutton = "icon";
      else if (strcmp (button, "appmenu") == 0)
        gtkbutton = "menu";
      else if (strcmp (button, "minimize") == 0)
        gtkbutton = "minimize";
      else if (strcmp (button, "maximize") == 0)
        gtkbutton = "maximize";
      else if  (strcmp (button, "close") == 0)
        gtkbutton = "close";
      else
        continue;

      if (len)
        layout[len++] = ',';

      strcpy (layout + len, gtkbutton);
      len += strlen (gtkbutton);
    }
  layout[len] = '\0';

out:
  if (len_p)
    *len_p = len;
}

void
translate_wm_button_layout_to_gtk (char *layout)
{
  char *strp = layout, *left_buttons, *right_buttons;
  int left_len, right_len = 0;

  left_buttons = strsep (&strp, ":");
  right_buttons = strp;

  translate_buttons (left_buttons, &left_len);
  memmove (layout, left_buttons, left_len);

  if (strp == NULL)
    goto out; /* no ":" in layout */

  layout[left_len++] = ':';

  translate_buttons (right_buttons, &right_len);
  memmove (layout + left_len, right_buttons, right_len);

out:
  layout[left_len + right_len] = '\0';
}
