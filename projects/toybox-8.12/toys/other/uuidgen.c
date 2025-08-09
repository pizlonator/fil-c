/* uuidgen.c - Create a new random UUID
 *
 * Copyright 2018 The Android Open Source Project
 *
 * UUID RFC: https://tools.ietf.org/html/rfc4122

USE_UUIDGEN(NEWTOY(uuidgen, ">0r(random)", TOYFLAG_USR|TOYFLAG_BIN))

config UUIDGEN
  bool "uuidgen"
  default y
  help
    usage: uuidgen

    Create and print a new RFC4122 random UUID.
*/

#define FOR_uuidgen
#include "toys.h"

void uuidgen_main(void)
{
  create_uuid(toybuf);
  puts(show_uuid(toybuf));
}
