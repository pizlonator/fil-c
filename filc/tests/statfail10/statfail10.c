#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>

int main()
{
    struct stat st;
    stat((char*)0x1010101010101010UL, &st);
    return 0;
}