/*
 * Copyright (C) 2002,2003 Red Hat, Inc.
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

#include "config.h"

#include <string.h>

#include <glib.h>
#include "debug.h"

guint _vte_debug_flags;

void
_vte_debug_init(void)
{
#if VTE_DEBUG
  const GDebugKey keys[] = {
    { "misc",         VTE_DEBUG_MISC         },
    { "io",           VTE_DEBUG_IO           },
    { "adj",          VTE_DEBUG_ADJ          },
    { "updates",      VTE_DEBUG_UPDATES      },
    { "events",       VTE_DEBUG_EVENTS       },
    { "parser",       VTE_DEBUG_PARSER       },
    { "signals",      VTE_DEBUG_SIGNALS      },
    { "selection",    VTE_DEBUG_SELECTION    },
    { "substitution", VTE_DEBUG_SUBSTITUTION },
    { "ring",         VTE_DEBUG_RING         },
    { "pty",          VTE_DEBUG_PTY          },
    { "cursor",       VTE_DEBUG_CURSOR       },
    { "keyboard",     VTE_DEBUG_KEYBOARD     },
    { "lifecycle",    VTE_DEBUG_LIFECYCLE    },
    { "work",         VTE_DEBUG_WORK         },
    { "cells",        VTE_DEBUG_CELLS        },
    { "timeout",      VTE_DEBUG_TIMEOUT      },
    { "draw",         VTE_DEBUG_DRAW         },
    { "ally",         VTE_DEBUG_ALLY         },
    { "pangocairo",   VTE_DEBUG_PANGOCAIRO   },
    { "widget-size",  VTE_DEBUG_WIDGET_SIZE  },
    { "style",        VTE_DEBUG_STYLE        },
    { "resize",       VTE_DEBUG_RESIZE       },
    { "regex",        VTE_DEBUG_REGEX        },
    { "hyperlink",    VTE_DEBUG_HYPERLINK    },
    { "modes",        VTE_DEBUG_MODES        },
    { "emulation",    VTE_DEBUG_EMULATION    },
    { "ringview",     VTE_DEBUG_RINGVIEW     },
    { "bidi",         VTE_DEBUG_BIDI         },
    { "conversion",   VTE_DEBUG_CONVERSION   },
    { "exceptions",   VTE_DEBUG_EXCEPTIONS   },
  };

  _vte_debug_flags = g_parse_debug_string (g_getenv("VTE_DEBUG"),
                                           keys, G_N_ELEMENTS (keys));
  _vte_debug_print(0xFFFFFFFFu, "VTE debug flags = %x\n", _vte_debug_flags);
#endif /* VTE_DEBUG */
}

const char *
_vte_debug_sequence_to_string(const char *str,
                              gssize length)
{
#if VTE_DEBUG
        static const char codes[][6] = {
                "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
                "BS", "HT", "LF", "VT", "FF", "CR", "SO", "SI",
                "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
                "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US",
                "SPACE"
        };
        static GString *buf;
        gssize i;

        if (str == NULL)
                return "(nil)";

        if (length == -1)
                length = strlen(str);

        if (buf == NULL)
                buf = g_string_new(NULL);

        g_string_truncate(buf, 0);
        for (i = 0; i < length; i++) {
                guint8 c = (guint8)str[i];
                if (i > 0)
                        g_string_append_c(buf, ' ');

                if (c == '\033' /* ESC */) {
                        switch (str[++i]) {
                        case '_': g_string_append(buf, "APC"); break;
                        case '[': g_string_append(buf, "CSI"); break;
                        case 'P': g_string_append(buf, "DCS"); break;
                        case ']': g_string_append(buf, "OSC"); break;
                        case '^': g_string_append(buf, "PM"); break;
                        case '\\': g_string_append(buf, "ST"); break;
                        default: g_string_append(buf, "ESC"); i--; break;
                        }
                }
                else if (c <= 0x20)
                        g_string_append(buf, codes[c]);
                else if (c == 0x7f)
                        g_string_append(buf, "DEL");
                else if (c >= 0x80)
                        g_string_append_printf(buf, "\\%02x ", c);
                else
                        g_string_append_c(buf, c);
        }

        return buf->str;
#else
        return NULL;
#endif /* VTE_DEBUG */
}

#if VTE_DEBUG
static bool
hexdump_line(GString* str,
             size_t ofs,
             uint8_t const* buf,
             size_t len)
{
        g_string_append_printf(str, "%08x  ", (unsigned int)ofs);
        for (unsigned int i = 0; i < 16; ++i) {
                if (i < len)
                        g_string_append_printf(str, "%02x ", buf[i]);
                else
                        g_string_append(str, "   ");
                if (i == 7)
                        g_string_append_c(str, ' ');
        }

        g_string_append(str, "  |");
        for (unsigned int i = 0; i < 16; ++i) {
                g_string_append_c(str, i < len ? (g_ascii_isprint(buf[i]) ? buf[i] : '.') : ' ');
        }
        g_string_append(str, "|\n");
        return len >= 16;
}
#endif /* VTE_DEBUG */

void
_vte_debug_hexdump(char const* str,
                   uint8_t const* buf,
                   size_t len)
{
#if VTE_DEBUG
        GString* s = g_string_new(str);
        g_string_append_printf(s, " len = 0x%x = %u\n", (unsigned int)len, (unsigned int)len);

        size_t ofs = 0;
        while (hexdump_line(s, ofs, buf + ofs, len - ofs))
                ofs += 16;

        g_printerr("%s", s->str);
        g_string_free(s, true);
#endif /* VTE_DEBUG */
}
