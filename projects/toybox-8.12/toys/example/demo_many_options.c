/* demo_many_options.c - test more than 32 bits worth of option flags
 *
 * Copyright 2015 Rob Landley <rob@landley.net>

USE_DEMO_MANY_OPTIONS(NEWTOY(demo_many_options, "ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba", TOYFLAG_BIN))

config DEMO_MANY_OPTIONS
  bool "demo_many_options"
  default n
  help
    usage: demo_many_options -[a-zA-Z]

    Print the optflags value of the command arguments, in hex.
*/

#define FOR_demo_many_options
#include "toys.h"

void demo_many_options_main(void)
{
  xprintf("optflags=%llx\n", toys.optflags);
}
