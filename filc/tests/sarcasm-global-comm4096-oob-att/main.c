#include <stdio.h>

void sink_oob_store(long off, long v);

int main(void)
{
    printf("expect trap:\n");
    sink_oob_store(4096, 0x41); /* one past the end */
    printf("SHOULD NOT PRINT\n");
    return 0;
}
