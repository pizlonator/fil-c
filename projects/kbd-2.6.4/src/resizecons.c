/*
 * resizecons.c - change console video mode
 *
 * Version 1.00
 *
 * How to use this:
 *
 * 1. Get svgalib, make restoretextmode, put it somewhere in your path.
 * 2. Put vga=ask in /etc/lilo/config, boot your machine a number of times,
 *    each time with a different vga mode, and run the command
 *        "restoretextmode -w COLSxROWS".
 *    For me this resulted in the files 80x25, 80x28, 80x50, 80x60, 100x40,
 *    132x25, 132x28, 132x44. Put these files in /usr/lib/kbd/videomodes
 *    (or in your current dir).
 * 3. Now "resizecons COLSxROWS" will change your video mode. (Assuming you
 *    have an appropriate kernel, and svgalib works for your video card.)
 *
 * Note: this is experimental, but it works for me. Comments are welcome.
 * You may have to be root to get the appropriate ioperm permissions.
 * It is not safe to make this program suid root.
 *
 * aeb@cwi.nl - 940924
 *
 * Harm Hanemaaijer added the -lines option, which reprograms the
 * number of scanlines. He writes:
 *
 * Added -lines option, which reprograms the number of scanlines and
 * the font height of the VGA hardware with register I/O, so that
 * switching is possible between textmodes with different numbers
 * of lines, in a VGA compatible way. It should work for 132 column
 * modes also, except that number of columns cannot be changed.
 *
 * Standard VGA textmode uses a 400 scanline screen which is refreshed
 * at 70 Hz. The following modes are supported that use this vertical
 * resolution (C is the number of columns, usually 80 or 132).
 *
 *	mode		font height
 *	C x 25		16
 *	C x 28		14
 *	C x 36		11	(non-standard height)
 *	C x 44		9	(8-line fonts are a good match)
 *	C x 50		8
 *
 * The following modes are supported with a 480 scanline resolution,
 * refresh at 60 Hz. Some not quite VGA compatible displays may not
 * support this (it uses the same vertical timing as standard VGA
 * 640x480x16 graphics mode).
 *
 *	mode		font height
 *	C x 30		16
 *	C x 34		14
 *	C x 40		12	(non-standard height)
 *	C x 60		8
 *
 * Two 12-line fonts are already in the consolefonts directory,
 * namely lat1-12.psfu.gz and lat2-12.psfu.gz.
 * For the 36 lines mode (11 line font), lat1-10.psfu.gz and lat2-10.psfu.gz
 * can be used.
 * 
 * hhanemaa@cs.ruu.nl - 941028
 * 
 * Notes:
 *
 * In the consolefonts directory there is 'default8x9' font file but
 * no 'default8x8'. Why is this? The standard VGA BIOS has an 8-line
 * font, and they are much more common in SVGA modes (e.g. 50 and 60
 * row modes). It is true that standard VGA textmode uses effectively
 * 9 pixel wide characters, but that has nothing to do with the font
 * data.
 */
#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <sys/io.h>
#include <sys/ioctl.h>
#include <linux/vt.h>

#include <kbdfile.h>

#include "paths.h"

#include "libcommon.h"

#define MODE_RESTORETEXTMODE 0
#define MODE_VGALINES 1

static void usage(void);

/* VGA textmode register tweaking. */
static void vga_init_io(void);
static void vga_400_scanlines(void);
static void vga_480_scanlines(void);
static void vga_set_fontheight(int);
static int vga_get_fontheight(void);
static void vga_set_cursor(int, int);
static void vga_set_verticaldisplayend_lowbyte(int);

const char *const dirpath[]  = {
	"",
	DATADIR "/" VIDEOMODEDIR "/",
	NULL
};
char const *const suffixes[] = {
	"",
	NULL
};

int main(int argc, char **argv)
{
	int rr, cc, fd, i, mode;
	struct vt_sizes vtsizes;
	struct vt_stat vtstat;
	struct winsize winsize;
	char *p;
	char tty[12], cmd[80], infile[1024];
	const char *defaultfont;
	struct kbdfile *fp;

	set_progname(argv[0]);
	setuplocale();

	if (argc < 2)
		usage();

	if (argc == 2 && !strcmp(argv[1], "-V"))
		print_version_and_exit();

	rr   = 0; /* make gcc happy */
	cc   = atoi(argv[1]);
	mode = MODE_RESTORETEXTMODE;
	if (argc == 3 && strcmp(argv[1], "-lines") == 0) {
		mode = MODE_VGALINES;
		rr   = atoi(argv[2]);
	} else if (argc == 2 && (p = strchr(argv[1], 'x')) != 0)
		rr = atoi(p + 1);
	else if (argc == 3)
		rr = atoi(argv[2]);
	else
		usage();

	if (cc <= 0 || cc > USHRT_MAX) {
		kbd_error(EXIT_FAILURE, 0, _("resizecons: invalid columns number %d"), cc);
		usage();
	}

	if (rr <= 0 || rr > USHRT_MAX) {
		kbd_error(EXIT_FAILURE, 0, _("resizecons: invalid rows number %d"), rr);
		usage();
	}

	if (!(fp = kbdfile_new(NULL)))
		kbd_error(EXIT_FAILURE, errno, "Unable to create kbdfile instance");

	if (mode == MODE_RESTORETEXTMODE) {
		/* prepare for: restoretextmode -r 80x25 */
		sprintf(infile, "%dx%d", cc, rr);
		if (kbdfile_find(infile, dirpath, suffixes, fp)) {
			kbd_error(EXIT_FAILURE, 0, _("resizecons: cannot find videomode file %s"), infile);
		}
		kbdfile_close(fp);
	}

	if ((fd = getfd(NULL)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	if (ioctl(fd, TIOCGWINSZ, &winsize)) {
		kbd_error(EXIT_FAILURE, errno, "ioctl TIOCGWINSZ");
	}

	if (mode == MODE_VGALINES) {
		/* Get the number of columns. */
		cc = winsize.ws_col;
		if (rr != 25 && rr != 28 && rr != 30 && rr != 34 && rr != 36 &&
		    rr != 40 && rr != 44 && rr != 50 && rr != 60) {
			kbd_error(EXIT_FAILURE, 0, _("Invalid number of lines"));
		}
	}

	if (ioctl(fd, VT_GETSTATE, &vtstat)) {
		kbd_error(EXIT_FAILURE, errno, "ioctl VT_GETSTATE");
	}

	vtsizes.v_rows       = (unsigned short) rr;
	vtsizes.v_cols       = (unsigned short) cc;
	vtsizes.v_scrollsize = 0;

	vga_init_io(); /* maybe only if (mode == MODE_VGALINES) */

	if (ioctl(fd, VT_RESIZE, &vtsizes)) {
		kbd_error(EXIT_FAILURE, errno, "ioctl VT_RESIZE");
	}

	if (mode == MODE_VGALINES) {
		/* Program the VGA registers. */
		int scanlines_old;
		int scanlines_new;
		int fontheight;
		if (winsize.ws_row == 25 || winsize.ws_row == 28 ||
		    winsize.ws_row == 36 || winsize.ws_row == 44 ||
		    winsize.ws_row == 50)
			scanlines_old = 400;
		else
			scanlines_old = 480;
		if (rr == 25 || rr == 28 || rr == 36 || rr == 44 || rr == 50)
			scanlines_new = 400;
		else
			scanlines_new = 480;
		/* Switch to 400 or 480 scanline vertical timing if required. */
		if (scanlines_old != 400 && scanlines_new == 400)
			vga_400_scanlines();
		if (scanlines_old != 480 && scanlines_new == 480)
			vga_480_scanlines();
		switch (rr) {
			case 25:
				fontheight = 16;
				break;
			case 28:
				fontheight = 14;
				break;
			case 30:
				fontheight = 16;
				break;
			case 34:
				fontheight = 14;
				break;
			case 36:
				fontheight = 12;
				break;
			case 40:
				fontheight = 12;
				break;
			case 44:
				fontheight = 9;
				break;
			case 50:
				fontheight = 8;
				break;
			case 60:
				fontheight = 8;
				break;
			default:
				fontheight = 8;
				break;
		}
		/* Set the VGA character height. */
		vga_set_fontheight(fontheight);
		/* Set the line offsets within a character cell of the cursor. */
		if (fontheight >= 10)
			vga_set_cursor(fontheight - 3, fontheight - 2);
		else
			vga_set_cursor(fontheight - 2, fontheight - 1);
		/*
	 * If there are a few unused scanlines at the bottom of the
	 * screen, make sure they are not displayed (otherwise
	 * there is a annoying changing partial line at the bottom).
	 */
		vga_set_verticaldisplayend_lowbyte((fontheight * rr - 1) & 0xff);
		printf(_("Old mode: %dx%d  New mode: %dx%d\n"), winsize.ws_col,
		       winsize.ws_row, cc, rr);
		printf(_("Old #scanlines: %d  New #scanlines: %d  Character height: %d\n"),
		       scanlines_old, scanlines_new, fontheight);
	}

	if (mode == MODE_RESTORETEXTMODE) {
		/* do: restoretextmode -r 25x80 */
		sprintf(cmd, "restoretextmode -r %s\n", kbdfile_get_pathname(fp));
		errno = 0;
		if (system(cmd)) {
			if (errno)
				perror("restoretextmode");
			fprintf(stderr, _("resizecons: the command `%s' failed\n"), cmd);
			exit(EXIT_FAILURE);
		}
	}

	kbdfile_free(fp);

	/*
     * for i in /dev/tty[0-9] /dev/tty[0-9][0-9]
     * do
     *     stty rows $rr cols $cc < $i
     * done
     * kill -SIGWINCH `cat /tmp/selection.pid`
     */
	winsize.ws_row = (unsigned short) rr;
	winsize.ws_col = (unsigned short) cc;
	for (i = 0; i < 16; i++)
		if (vtstat.v_state & (1 << i)) {
			sprintf(tty, "/dev/tty%d", i);
			fd = open(tty, O_RDONLY);
			if (fd < 0 && errno == ENOENT) {
				sprintf(tty, "/dev/vc/%d", i);
				fd = open(tty, O_RDONLY);
			}
			if (fd >= 0) {
				if (ioctl(fd, TIOCSWINSZ, &winsize))
					kbd_warning(errno, "ioctl TIOCSWINSZ");
				close(fd);
			}
		}

#if 0
    /* Try to tell selection about the change */
    /* [may be a security risk?] */
    if ((fd = open("/tmp/selection.pid", O_RDONLY)) >= 0) {
	char buf[64];
	int n = read(fd, buf, sizeof(buf));
	if (n > 0) {
	    int pid;

	    buf[n-1] = 0;
	    pid = atoi(buf);
	    kill(pid, SIGWINCH);
	}
	close(fd);
    }
#endif

	/* do: setfont default8x16 */
	/* (other people might wish other fonts - this should be settable) */

	/* We read the VGA font height register to be sure. */
	/* There isn't much consistency in this. */
	switch (vga_get_fontheight()) {
		case 8:
		case 9:
			defaultfont = "default8x9";
			break;
		case 10:
			defaultfont = "lat1-10";
			break;
		case 11:
		case 12:
			defaultfont = "lat1-12";
			break;
		case 13:
		case 14:
			defaultfont = "iso01.14";
			break;
		case 15:
		case 16:
		default:
			defaultfont = "default8x16";
			break;
	}

	sprintf(cmd, "setfont %s", defaultfont);
	errno = 0;
	if (system(cmd)) {
		if (errno)
			perror("setfont");
		fprintf(stderr, "resizecons: the command `%s' failed\n", cmd);
		exit(EXIT_FAILURE);
	}

	fprintf(stderr, _("resizecons: don't forget to change TERM "
	                  "(maybe to con%dx%d or linux-%dx%d)\n"),
	        cc, rr, cc, rr);
	if (getenv("LINES") || getenv("COLUMNS"))
		fprintf(stderr,
		        "Also the variables LINES and COLUMNS may need adjusting.\n");

	return EXIT_SUCCESS;
}

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stderr,
	        _("resizecons:\n"
	          "call is:  resizecons COLSxROWS  or:  resizecons COLS ROWS\n"
	          "or: resizecons -lines ROWS, with ROWS one of 25, 28, 30, 34,"
	          " 36, 40, 44, 50, 60\n"));
	exit(EXIT_FAILURE);
}

/*
 * The code below is used only with the option `-lines ROWS', and is
 * very hardware dependent, and requires root privileges.
 */

/* Port I/O macros. Note that these are not compatible with the ones */
/* defined in the kernel header files. */

static inline void my_outb(int port, int value)
{
	__asm__ volatile("outb %0,%1"
	                 :
	                 : "a"((unsigned char)value), "d"((unsigned short)port));
}

static inline int my_inb(int port)
{
	unsigned char value;
	__asm__ volatile("inb %1,%0"
	                 : "=a"(value)
	                 : "d"((unsigned short)port));
	return value;
}

/* VGA textmode register tweaking functions. */

static int crtcport;

static void vga_init_io(void)
{
	if (iopl(3) < 0) {
		fprintf(stderr,
		        _("resizecons: cannot get I/O permissions.\n"));
		exit(EXIT_FAILURE);
	}
	crtcport = 0x3d4;
	if ((my_inb(0x3cc) & 0x01) == 0)
		crtcport = 0x3b4;
}

static void vga_set_fontheight(int h)
{
	my_outb(crtcport, 0x09);
	my_outb(crtcport + 1, (my_inb(crtcport + 1) & 0xe0) | (h - 1));
}

static int vga_get_fontheight(void)
{
	my_outb(crtcport, 0x09);
	return (my_inb(crtcport + 1) & 0x1f) + 1;
}

static void vga_set_cursor(int top, int bottom)
{
	my_outb(crtcport, 0x0a);
	my_outb(crtcport + 1, (my_inb(crtcport + 1) & 0xc0) | top);
	my_outb(crtcport, 0x0b);
	my_outb(crtcport + 1, (my_inb(crtcport + 1) & 0xe0) | bottom);
}

static void vga_set_verticaldisplayend_lowbyte(int byte)
{
	/* CRTC register 0x12 */
	/* vertical display end */
	my_outb(crtcport, 0x12);
	my_outb(crtcport + 1, byte);
}

static void vga_480_scanlines(void)
{
	/* CRTC register 0x11 */
	/* vertical sync end (also unlocks CR0-7) */
	my_outb(crtcport, 0x11);
	my_outb(crtcport + 1, 0x0c);

	/* CRTC register 0x06 */
	/* vertical total */
	my_outb(crtcport, 0x06);
	my_outb(crtcport + 1, 0x0b);

	/* CRTC register 0x07 */
	/* (vertical) overflow */
	my_outb(crtcport, 0x07);
	my_outb(crtcport + 1, 0x3e);

	/* CRTC register 0x10 */
	/* vertical sync start */
	my_outb(crtcport, 0x10);
	my_outb(crtcport + 1, 0xea);

	/* CRTC register 0x12 */
	/* vertical display end */
	my_outb(crtcport, 0x12);
	my_outb(crtcport + 1, 0xdf);

	/* CRTC register 0x15 */
	/* vertical blank start */
	my_outb(crtcport, 0x15);
	my_outb(crtcport + 1, 0xe7);

	/* CRTC register 0x16 */
	/* vertical blank end */
	my_outb(crtcport, 0x16);
	my_outb(crtcport + 1, 0x04);

	/* Misc Output register */
	/* Preserver clock select bits and set correct sync polarity */
	my_outb(0x3c2, (my_inb(0x3cc) & 0x0d) | 0xe2);
}

static void vga_400_scanlines(void)
{
	/* CRTC register 0x11 */
	/* vertical sync end (also unlocks CR0-7) */
	my_outb(crtcport, 0x11);
	my_outb(crtcport + 1, 0x0e);

	/* CRTC register 0x06 */
	/* vertical total */
	my_outb(crtcport, 0x06);
	my_outb(crtcport + 1, 0xbf);

	/* CRTC register 0x07 */
	/* (vertical) overflow */
	my_outb(crtcport, 0x07);
	my_outb(crtcport + 1, 0x1f);

	/* CRTC register 0x10 */
	/* vertical sync start */
	my_outb(crtcport, 0x10);
	my_outb(crtcport + 1, 0x9c);

	/* CRTC register 0x12 */
	/* vertical display end */
	my_outb(crtcport, 0x12);
	my_outb(crtcport + 1, 0x8f);

	/* CRTC register 0x15 */
	/* vertical blank start */
	my_outb(crtcport, 0x15);
	my_outb(crtcport + 1, 0x96);

	/* CRTC register 0x16 */
	/* vertical blank end */
	my_outb(crtcport, 0x16);
	my_outb(crtcport + 1, 0xb9);

	/* Misc Output register */
	/* Preserver clock select bits and set correct sync polarity */
	my_outb(0x3c2, (my_inb(0x3cc) & 0x0d) | 0x62);
}
