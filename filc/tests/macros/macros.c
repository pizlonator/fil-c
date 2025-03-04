#include <stdfil.h>

int main()
{
#ifndef __PIZLONATOR_WAS_HERE__
    zerror("__PIZLONATOR_WAS_HERE__ not defined");
#endif
#ifndef __FILC__
    zerror("__FILC__ not defined");
#endif
    return 0;
}

