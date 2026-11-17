/*
 * Copyright © 2019 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Red Hat Author(s): Carlos Santos
 */

#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctype.h>

#define SPECIALS "/:_.-!'()~"

int
main(int argc,
     char *argv[])
{
        /* Note that we're not calling setlocale(LC_ALL, "") here on purpose,
         * since we WANT this to run under C locale.
         */

        auto pwd = getenv("PWD");
        if (pwd == nullptr) {
                fprintf(stderr, "PWD environment variable not set");
                return EXIT_FAILURE;
        }

        auto ch = int{};
        while ((ch = *pwd++ & 0xff)) {
                if (isalnum(ch) || strchr(SPECIALS, ch)) {
                        putchar(ch);
                } else {
                        printf("%%%02X", ch);
                }
        }

        return EXIT_SUCCESS;
}
