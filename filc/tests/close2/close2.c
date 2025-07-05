#include <unistd.h>
#include "utils.h"

int main()
{
    close(2);
    *(int*)opaque((void*)666) = 1410;
    return 0;
}

