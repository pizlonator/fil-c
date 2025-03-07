#include "raii.h"

void stuff();

extern "C" void baz()
{
    RAII raii;
    stuff();
}

