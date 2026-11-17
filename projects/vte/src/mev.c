/*
 * Copyright (C) 2003 Red Hat, Inc.
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

#include <config.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <glib.h>
#include "caps.hh"

#if __has_include(<sys/select.h>)
#include <sys/select.h>
#endif

enum {
	tracking_x10 = 9,
	tracking_mouse = 1000,
	tracking_hilite = 1001,
	tracking_cell_motion = 1002,
	tracking_all_motion = 1003,
        tracking_focus = 1004,
        tracking_xterm_ext = 1006,
};

static int tracking_mode = 0;
static gboolean tracking_focus_mode = FALSE;

static void
decset(int mode, gboolean value)
{
	fprintf(stdout, _VTE_CAP_CSI "?%d%c", mode, value ? 'h' : 'l');
}

static void
reset_mouse_tracking_mode(void)
{
	decset(tracking_x10, FALSE);
	decset(tracking_mouse, FALSE);
	decset(tracking_hilite, FALSE);
	decset(tracking_cell_motion, FALSE);
	decset(tracking_all_motion, FALSE);
	decset(tracking_xterm_ext, FALSE);
	fflush(stdout);
}

static void
reset(void)
{
        reset_mouse_tracking_mode();
        decset(tracking_focus, FALSE);
        fflush(stdout);
}

static void
clear(void)
{
	fprintf(stdout, "%s",
		_VTE_CAP_ESC "7"
		_VTE_CAP_CSI "11;1H"
		_VTE_CAP_CSI "1J"
		_VTE_CAP_CSI "2K"
		_VTE_CAP_CSI "1;1H");
	reset_mouse_tracking_mode();
	switch (tracking_mode) {
	case tracking_x10:
		fprintf(stdout, "X10 tracking enabled.\r\n");
		decset(tracking_x10, TRUE);
		break;
	case tracking_mouse:
		fprintf(stdout, "Mouse tracking enabled.\r\n");
		decset(tracking_mouse, TRUE);
		break;
	case tracking_hilite:
		fprintf(stdout, "Hilite tracking enabled.\r\n");
		decset(tracking_hilite, TRUE);
		break;
	case tracking_cell_motion:
		fprintf(stdout, "Cell motion tracking enabled.\r\n");
		decset(tracking_cell_motion, TRUE);
		break;
	case tracking_all_motion:
		fprintf(stdout, "All motion tracking enabled.\r\n");
		decset(tracking_all_motion, TRUE);
		break;
	case tracking_xterm_ext:
		fprintf(stdout, "Xterm 1006 mouse tracking extension enabled.\r\n");
		decset(tracking_xterm_ext, TRUE);
		break;
	default:
		fprintf(stdout, "Tracking disabled.\r\n");
		break;
	}
        fprintf(stdout, "Tracking focus %s.\r\n", tracking_focus_mode ? "enabled":"disabled");
	fprintf(stdout, "A - X10.\r\n");
	fprintf(stdout, "B - Mouse tracking.\r\n");
	fprintf(stdout, "C - Hilite tracking [FIXME: NOT IMPLEMENTED].\r\n");
	fprintf(stdout, "D - Cell motion tracking.\r\n");
	fprintf(stdout, "E - All motion tracking.\r\n");
	fprintf(stdout, "F - Xterm 1006 extension.\r\n");
	fprintf(stdout, "I - Focus tracking.\r\n");
	fprintf(stdout, "Q - Quit.\r\n");
	fprintf(stdout, "%s", _VTE_CAP_ESC "8");
	fflush(stdout);
}

static gsize
parse_legacy_mouse_mode(guint8 *data,
                        gsize len)
{
        int button = 0;
        const char *shift = "", *control = "", *alt = "";
        gboolean motion = FALSE;
        int x, y;
        guint8 b;

        if (len < 6)
                return 0;

        b = data[3] - 32;
        switch (b & 3) {
        case 0:
                button = 1;
                if (b & 64) {
                        button += 3;
                }
                break;
        case 1:
                button = 2;
                if (b & 64) {
                        button += 3;
                }
                break;
        case 2:
                button = 3;
                if (b & 64) {
                        button += 3;
                }
                break;
        case 3:
                button = 0;
                break;
        }
        shift = b & 4 ?
                "[shift]" :
                "";
        alt = b & 8 ?
                "[alt]" :
                "";
        control = b & 16 ?
                "[control]" :
                "";
        motion = (b & 32) != 0;
        x = data[4] - 32;
        y = data[5] - 32;
        fprintf(stdout, "%d %s%s%s(%s%s%s) at %d,%d\r\n",
                button,
                motion ? "motion " : "",
                (!motion && button) ? "press" : "",
                (!motion && !button) ? "release" : "",
                alt, control, shift,
                x, y);

        return 6;
}

static gsize
parse_esc(guint8 *data,
          gsize len)
{
        if (len < 3)
                return 0;

        if (!(data[0] == '\033' && data[1] == '[')) /* CSI ? */
                return 0;

        if (data[2] == 'M')
                return parse_legacy_mouse_mode(data, len);

        /* FIXME: add support for xterm extended mode (1006) */

        if (data[2] == 'I' || data[2] == 'O') {
                fprintf(stdout, "focus %s\r\n", data[2] == 'I' ? "in" : "out");
                return 3;
        }

        return 0;
}

static gsize
print_data(guint8 *data,
           gsize len)
{
        static const char codes[][6] = {
                "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
                "BS", "HT", "LF", "VT", "FF", "CR", "SO", "SI",
                "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
                "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US",
                "SPACE"
        };
        gsize i;

        if (len == 0)
                return 0;

        for (i = 0; i < len; i++) {
                guint8 c = (guint8)data[i];

                if (c == '\033' /* ESC */) {
                        switch (data[++i]) {
                        case '_': fprintf(stdout, "APC "); break;
                        case '[': fprintf(stdout, "CSI "); break;
                        case 'P': fprintf(stdout, "DCS "); break;
                        case ']': fprintf(stdout, "OSC "); break;
                        case '^': fprintf(stdout, "PM "); break;
                        case '\\': fprintf(stdout, "ST "); break;
                        default: fprintf(stdout, "ESC "); i--; break;
                        }
                }
                else if (c <= 0x20)
                        fprintf(stdout, "%s ", codes[c]);
                else if (c == 0x7f)
                        fprintf(stdout, "DEL ");
                else if (c >= 0x80)
                        fprintf(stdout, "\\%02x ", c);
                else
                        fprintf(stdout, "%c ", c);
        }

        return len;
}

static gboolean
parse(void)
{
	GByteArray *bytes;
	guchar buffer[64];
	gsize i, length;
	gboolean ret = FALSE;

	bytes = g_byte_array_new();
	if ((length = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
		g_byte_array_append(bytes, buffer, length);
	}

	i = 0;
	while (i < bytes->len) {
		switch (bytes->data[i]) {
		case 'A':
		case 'a':
			tracking_mode = (tracking_mode == tracking_x10) ?
					0 : tracking_x10;
			i++;
			break;
		case 'B':
		case 'b':
			tracking_mode = (tracking_mode == tracking_mouse) ?
					0 : tracking_mouse;
			i++;
			break;
		case 'C':
		case 'c':
			tracking_mode = (tracking_mode == tracking_hilite) ?
					0 : tracking_hilite;
			i++;
			break;
		case 'D':
		case 'd':
			tracking_mode = (tracking_mode == tracking_cell_motion) ?
					0 : tracking_cell_motion;
			i++;
			break;
		case 'E':
		case 'e':
			tracking_mode = (tracking_mode == tracking_all_motion) ?
					0 : tracking_all_motion;
			i++;
			break;
		case 'F':
		case 'f':
			tracking_mode = (tracking_mode == tracking_xterm_ext) ?
					0 : tracking_xterm_ext;
			i++;
			break;
		case 'I':
		case 'i':
			tracking_focus_mode = !tracking_focus_mode;
                        decset(tracking_focus, tracking_focus_mode);
			i++;
			break;
		case 'Q':
		case 'q':
			ret = TRUE;
			i++;
			break;
		case '\033': {
                        gsize consumed = parse_esc(&bytes->data[i], bytes->len - i);
                        if (consumed > 0) {
                                i += consumed;
                                break;
                        }
                }
                        /* fallthrough */
		default:
                        i += print_data(&bytes->data[i], bytes->len - i);
			fprintf(stdout, "\r\n");
			break;
		}
	}
	fflush(stdout);
	g_byte_array_free(bytes, TRUE);

	return ret;
}

static struct termios tcattr, original;

G_GNUC_NORETURN
static void
sigint_handler(int signum)
{
	if (tcsetattr(STDIN_FILENO, TCSANOW, &original) != 0) {
		perror("tcsetattr");
	}
	reset();
	_exit(1);
}

int
main(int argc, char **argv)
{
	int flags;
	gboolean stop;
	fd_set in_fds;

	if (tcgetattr(STDIN_FILENO, &tcattr) != 0) {
		perror("tcgetattr");
		return 1;
	}

	original = tcattr;
	signal(SIGINT, sigint_handler);
	cfmakeraw(&tcattr);
	if (tcsetattr(STDIN_FILENO, TCSANOW, &tcattr) != 0) {
		perror("tcsetattr");
		return 1;
	}

	flags = fcntl(STDIN_FILENO, F_GETFL);
	fcntl(STDIN_FILENO, F_SETFL, flags & ~(O_NONBLOCK));
	fprintf(stdout, "%s",
		_VTE_CAP_CSI "12;1H"
		_VTE_CAP_CSI "2K"
		_VTE_CAP_CSI "2J");
	do {
		clear();
		FD_ZERO(&in_fds);
		FD_SET(STDIN_FILENO, &in_fds);
		stop = TRUE;
		switch (select(STDIN_FILENO + 1, &in_fds, NULL, NULL, NULL)) {
		case 1:
			stop = parse();
			break;
		default:
			stop = TRUE;
			break;
		}
	} while (!stop);
	reset();
	fcntl(STDIN_FILENO, F_SETFL, flags);

	if (tcsetattr(STDIN_FILENO, TCSANOW, &original) != 0) {
		perror("tcsetattr");
		return 1;
	}

	return 0;
}
