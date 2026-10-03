#include <stdfil.h>
#include <stdlib.h>
#include <string.h>

int counter;
char name[32];
int* ptr;

void bump(void)
{
    counter++;
}

int* make(void)
{
    int* result = malloc(100 * sizeof(int));
    result[99] = 42;
    return result;
}

void check_from_other_unit(void)
{
    ZASSERT(counter == 2);
    ZASSERT(!strcmp(name, "common"));
    ZASSERT(ptr[99] == 42);
}
