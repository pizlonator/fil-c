#include <stdfil.h>

__attribute__((noinline))
static int mat_oob(int *m, int rows, int cols) {
    int sum = 0;
    for (int r = 0; r <= rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int idx = r * cols + c;
            m[idx] = 1;
            sum += m[idx];
        }
    }
    return sum;
}

int main(void) {
    int mat[8 * 8];
    int s = mat_oob(mat, 8, 8);
    zprintf("Should not reach: %d\n", s);
    return 0;
}
