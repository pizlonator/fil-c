#include <filc_test_support.h>
#include <stdfil.h>

int main()
{
    zptrtable* dst = zptrtable_new();
    char* src = "hello";

    zmemmove_union(dst, &src, 16);

    return 0;
}

