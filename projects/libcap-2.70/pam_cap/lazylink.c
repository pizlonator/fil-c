/*
 * Test if the provided LDFLAGS support lazy linking
 */
#include <stdio.h>
#include <stdlib.h>

#include "../libcap/execable.h"

extern int nothing_sets_this(void);
extern void nothing_uses_this(void);

void nothing_uses_this(void)
{
    nothing_sets_this();
}

SO_MAIN(int argc, char **argv)
{
    exit(0);
}
