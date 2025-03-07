#include "raii.h"

RAII::RAII()
{
    zprintf("RAII::RAII()\n");
}

RAII::~RAII()
{
    zprintf("RAII::~RAII()\n");
}
