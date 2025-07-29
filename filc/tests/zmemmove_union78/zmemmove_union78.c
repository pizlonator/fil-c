#include <filc_test_support.h>
#include <stdfil.h>

int main()
{
    char src[64];

    zmemmove_union((void*)666, src, 64);

    return 0;
}

