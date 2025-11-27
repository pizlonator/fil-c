/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: A parser for the XML GIR format
 *
 * Copyright (C) 2005 Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __G_GIR_PARSER_H__
#define __G_GIR_PARSER_H__

#include <glib.h>

G_BEGIN_DECLS

#include "girmodule.h"

typedef struct _GIrParser GIrParser;

GIrParser *_g_ir_parser_new          (void);
void       _g_ir_parser_free         (GIrParser          *parser);
void       _g_ir_parser_set_includes (GIrParser          *parser,
				      const gchar *const *includes);

GIrModule *_g_ir_parser_parse_string (GIrParser    *parser,
				      const gchar  *namespace,
				      const gchar  *filename,
				      const gchar  *buffer,
				      gssize        length,
				      GError      **error);
GIrModule *_g_ir_parser_parse_file   (GIrParser    *parser,
				      const gchar  *filename,
				      GError      **error);

G_END_DECLS

#endif  /* __G_GIR_PARSER_H__ */
