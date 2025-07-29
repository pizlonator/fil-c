#include <filc_test_support.h>
#include <stdfil.h>

int main()
{
    zptrtable* src = zptrtable_new();
    char* dst = "hello";

    zmemmove_union(&dst, src, 8);

    return 0;
}

