#include <stdio.h>
struct String { unsigned char* bytes; unsigned long size; };
unsigned long hash(struct String* s);
int main() {
    struct String h = { (unsigned char*)"hello", 5 };
    printf("hello/5 = %lu\n", hash(&h));
    struct String e = { (unsigned char*)"", 0 };
    printf("empty/0 = %lu\n", hash(&e));
    return 0;
}
