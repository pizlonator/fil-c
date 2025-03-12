#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdfil.h>

struct list {
    struct list* next;
    int value;
};

__attribute__((noinline)) struct list* insert_sorted(struct list** list_head, int value)
{
    struct list* node = (struct list*)malloc(sizeof(struct list));
    node->value = value;
    struct list** next_ptr;
    for (next_ptr = list_head; ; next_ptr = &(*next_ptr)->next) {
        if (!*next_ptr || (*next_ptr)->value > value) {
            node->next = *next_ptr;
            *next_ptr = node;
            return node;
        }
    }
}

int main()
{
    struct list* list = NULL;
    unsigned i;
    unsigned value = 42;
    unsigned n = 10000;
    for (i = n; i--;) {
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        insert_sorted(&list, value);
    }
    unsigned count = 0;
    bool had_last = false;
    int last_value;
    for (; list; list = list->next) {
        if (had_last)
            ZASSERT(list->value > last_value);
        had_last = true;
        last_value = list->value;
        count++;
    }
    ZASSERT(had_last);
    ZASSERT(last_value > 0);
    ZASSERT(count == n);
    return 0;
}
