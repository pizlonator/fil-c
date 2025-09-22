#include "fuzz.h"
#include "xalloc.h"
#include "mountP.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
        struct libmnt_table *tb = NULL;
        FILE *f = NULL;

        if (size == 0)
                return 0;

        // 128Kb should be enough to trigger all the issues we're interested in
        if (size > 131072)
                return 0;

        tb = mnt_new_table();
        if (!tb)
		err_oom();

        f = fmemopen((char*) data, size, "re");
        if (!f)
		err(EXIT_FAILURE, "fmemopen() failed");

        mnt_table_enable_comments(tb, TRUE);
        (void) mnt_table_parse_stream(tb, f, "mountinfo");

        mnt_unref_table(tb);
        fclose(f);

        return 0;
}
