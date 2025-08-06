#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
static __attribute__((noinline)) void
test0(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    zmemmove_union(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test1(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    zmemmove_union(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test2(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    src = opaque(src);
    dst = opaque(dst);
    zmemmove_union(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test3(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    src = opaque(src);
    dst = opaque(dst);
    zmemmove_union(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test4(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    zmemmove_union(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test5(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    zmemmove_union(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test6(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    src = opaque(src);
    dst = opaque(dst);
    zmemmove_union(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test7(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    src = opaque(src);
    dst = opaque(dst);
    zmemmove_union(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test8(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test9(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test10(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test11(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test12(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test13(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test14(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test15(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test16(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test17(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test18(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test19(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test20(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test21(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test22(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test23(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(!zhasvalidcap(dst[1]));
    ZASSERT(!opaque(dst[1]));
    ZASSERT(!zhasvalidcap(dst[2]));
    ZASSERT(!opaque(dst[2]));
    ZASSERT(!zhasvalidcap(dst[3]));
    ZASSERT(!opaque(dst[3]));
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test24(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test25(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test26(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test27(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test28(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test29(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test30(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test31(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test32(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test33(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test34(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test35(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test36(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test37(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test38(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
static __attribute__((noinline)) void
test39(void)
{
    int** src = zgc_alloc(39);
    int** dst = zgc_alloc(39);
    int** origDst = zgc_alloc(39);
    src = opaque(src);
    dst = opaque(dst);
    src[0] = zgc_alloc(sizeof(int));
    *src[0] = 666;
    dst[0] = zgc_alloc(sizeof(int));
    *dst[0] = 1410;
    origDst[0] = dst[0];
    src[1] = zgc_alloc(sizeof(int));
    *src[1] = 667;
    dst[1] = zgc_alloc(sizeof(int));
    *dst[1] = 1411;
    origDst[1] = dst[1];
    src[2] = zgc_alloc(sizeof(int));
    *src[2] = 668;
    dst[2] = zgc_alloc(sizeof(int));
    *dst[2] = 1412;
    origDst[2] = dst[2];
    src[3] = zgc_alloc(sizeof(int));
    *src[3] = 669;
    dst[3] = zgc_alloc(sizeof(int));
    *dst[3] = 1413;
    origDst[3] = dst[3];
    src[4] = zgc_alloc(sizeof(int));
    *src[4] = 670;
    dst[4] = zgc_alloc(sizeof(int));
    *dst[4] = 1414;
    origDst[4] = dst[4];
    src = opaque(src);
    dst = opaque(dst);
    zmemmove(
        (char*)dst + 7, (char*)src + 7,
        32);
    src = opaque(src);
    dst = opaque(dst);
    ZASSERT(!zhasvalidcap(dst[0]));
    ZASSERT(zhasvalidcap(dst[1]));
    ZASSERT(opaque(dst[1]) ==
            opaque(src[1]));
    ZASSERT(*dst[1] == *src[1]);
    ZASSERT(zhasvalidcap(dst[2]));
    ZASSERT(opaque(dst[2]) ==
            opaque(src[2]));
    ZASSERT(*dst[2] == *src[2]);
    ZASSERT(zhasvalidcap(dst[3]));
    ZASSERT(opaque(dst[3]) ==
            opaque(src[3]));
    ZASSERT(*dst[3] == *src[3]);
    ZASSERT(!zhasvalidcap(dst[4]));
    size_t index;
    for (index = 40; index--;) {
        int** expected =
            (index >= 7 &&
             index < 39)
            ? src : origDst;
        ZASSERT(((char*)dst)[index] ==
                ((char*)expected)[index]);
    }
}
int main()
{
    test0();
    test1();
    test2();
    test3();
    test4();
    test5();
    test6();
    test7();
    test8();
    test9();
    test10();
    test11();
    test12();
    test13();
    test14();
    test15();
    test16();
    test17();
    test18();
    test19();
    test20();
    test21();
    test22();
    test23();
    test24();
    test25();
    test26();
    test27();
    test28();
    test29();
    test30();
    test31();
    test32();
    test33();
    test34();
    test35();
    test36();
    test37();
    test38();
    test39();
    return 0;
}
