#include <filc_test_support.h>
#include <stdfil.h>

int main()
{
    zmemmove_union("hello", "world", 5);

    return 0;
}

