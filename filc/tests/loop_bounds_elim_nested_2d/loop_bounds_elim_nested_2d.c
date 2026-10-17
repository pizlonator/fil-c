#include <stdfil.h>

__attribute__((noinline))
static int mat_sum(int *m, int rows, int cols) {
    int sum = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int idx = r * cols + c;
            m[idx] = 1;
            sum += m[idx];
        }
    }
    return sum;
}

int main(void) {
    int mat[64 * 64];
    int s = mat_sum(mat, 64, 64);
    zprintf("OK: nested 2d = %d\n", s);
    return (s == 4096) ? 0 : 1;
}
