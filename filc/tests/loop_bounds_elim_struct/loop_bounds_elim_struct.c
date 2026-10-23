#include <stdfil.h>

struct Node {
    int x;
    int y;
    int z;
};

__attribute__((noinline))
static int sum_nodes(struct Node *nodes, int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        nodes[i].x = 10;
        nodes[i].y = nodes[i].x + 20;
        nodes[i].z = nodes[i].x + nodes[i].y;
        total += nodes[i].z;
    }
    return total;
}

int main(void) {
    struct Node arr[160];
    int res = sum_nodes(arr, 160);
    zprintf("OK: struct = %d\n", res);
    return (res == 6400) ? 0 : 1;
}
