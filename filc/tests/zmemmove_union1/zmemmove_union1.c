#include <filc_test_support.h>
#include <stdfil.h>

int main(int argc, char** argv)
{
    int foo;
    zprintf("Yo!\n");
    /* Wacky note: if SROA sees this memcpy, it'll just simply delete it! That's a memory safe
       outcome, but whatever. */
    zmemmove_union(&foo, 0, sizeof(foo));
    zprintf("No!\n");
    return 0;
}


