/* Flags values for the third argument of NEWTOY()
 *
 * Included from both main.c (runs in toys.h context) and scripts/install.c
 * (which may build on crazy things like macosx when cross compiling).
 */

// Flags describing command behavior.

// Where to install (toybox --long outputs absolute paths to commands)
// If no location bits set, command not listed in "toybox" command's output.
#define TOYFLAG_USR      (1<<0)
#define TOYFLAG_BIN      (1<<1)
#define TOYFLAG_SBIN     (1<<2)
#define TOYMASK_LOCATION ((1<<4)-1)

// This is a shell built-in function, running in the same process context.
#define TOYFLAG_NOFORK   (1<<4)
#define TOYFLAG_MAYFORK  (1<<5)

// Start command with a umask of 0 (saves old umask in this.old_umask)
#define TOYFLAG_UMASK    (1<<6)

// This command runs as root.
#define TOYFLAG_STAYROOT (1<<7) // Don't drop suid root before running cmd_main
#define TOYFLAG_NEEDROOT (1<<8) // Refuse to run if real uid != 0
#define TOYFLAG_ROOTONLY (TOYFLAG_STAYROOT|TOYFLAG_NEEDROOT)

// Suppress default --help processing
#define TOYFLAG_NOHELP   (1<<9)
#define TOYFLAG_AUTOCONF (1<<10)

// Line buffered stdout
#define TOYFLAG_LINEBUF  (1<<11)
#define TOYFLAG_NOBUF    (1<<12)

// Error code to return if argument parsing fails (default 1)
#define TOYFLAG_ARGFAIL(x) (x<<24)
