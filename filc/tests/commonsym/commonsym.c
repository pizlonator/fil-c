/* Common symbols are merged across translation units, keep their bounds and
   are scanned by the GC. */
#include <stdfil.h>
#include <stdio.h>
#include <string.h>

__attribute__((common)) int counter;
__attribute__((common)) char name[32];
__attribute__((common)) int* ptr;

void bump(void);
int* make(void);
void check_from_other_unit(void);

int main()
{
    ZASSERT(!counter);
    ZASSERT(!name[0]);
    ZASSERT(!ptr);
    bump();
    bump();
    ZASSERT(counter == 2);
    strcpy(name, "common");
    ptr = make();
    zgc_request_and_wait();
    ZASSERT(ptr[99] == 42);
    check_from_other_unit();
    printf("common ok %d %s\n", counter, name);
    return 0;
}
