/* We copy gsd-wacom-device from gnome-settings-daemon.
 * It include "gsd-enums.h" because the include directory
 * is known. As gnome-settings-daemon's pkg-config file
 * prefixes this, we need a little help to avoid this
 * one line difference */

#include <gnome-settings-daemon/gsd-enums.h>
