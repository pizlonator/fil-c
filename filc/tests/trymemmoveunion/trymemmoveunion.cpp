#include <filc_test_support.h>

int main()
{
    int x;
    int y = 666;
    try {
        zmemmove_union(&x, &y, sizeof(int));
    } catch (int& e) {
        x = 1410;
    }
    ZASSERT(x == 666);
    return 0;
}

