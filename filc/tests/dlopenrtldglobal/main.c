#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdfil.h>
#include <stdio.h>
#include <string.h>

typedef const char* (*id_fn_t)(void);
typedef int (*tag_fn_t)(int);
typedef int (*vsum_fn_t)(int, ...);

int main(void)
{
    void* ha = dlopen("filc/test-output/dlopenrtldglobal/liba.so", RTLD_LAZY | RTLD_GLOBAL);
    ZASSERT(ha);
    void* hb = dlopen("filc/test-output/dlopenrtldglobal/libb.so", RTLD_LAZY | RTLD_GLOBAL);
    ZASSERT(hb);

    id_fn_t id_a = (id_fn_t)dlsym(ha, "plugin_id");
    id_fn_t id_b = (id_fn_t)dlsym(hb, "plugin_id");
    tag_fn_t tag_a = (tag_fn_t)dlsym(ha, "plugin_tag");
    tag_fn_t tag_b = (tag_fn_t)dlsym(hb, "plugin_tag");
    vsum_fn_t vsum_a = (vsum_fn_t)dlsym(ha, "plugin_vsum");
    vsum_fn_t vsum_b = (vsum_fn_t)dlsym(hb, "plugin_vsum");
    id_fn_t id_def = (id_fn_t)dlsym(RTLD_DEFAULT, "plugin_id");

    ZASSERT(id_a);
    ZASSERT(id_b);
    ZASSERT(tag_a);
    ZASSERT(tag_b);
    ZASSERT(vsum_a);
    ZASSERT(vsum_b);
    ZASSERT(id_def);

    ZASSERT(id_a != id_b);
    ZASSERT(tag_a != tag_b);
    ZASSERT(vsum_a != vsum_b);

    const char* sa = id_a();
    const char* sb = id_b();
    const char* sd = id_def();
    int ta = tag_a(7);
    int tb = tag_b(7);
    int va = vsum_a(2, 10, 20);
    int vb = vsum_b(2, 10, 20);

    ZASSERT(!strcmp(sa, "plugin_a"));
    ZASSERT(!strcmp(sb, "plugin_b"));
    ZASSERT(!strcmp(sd, "plugin_a"));
    ZASSERT(ta == 107);
    ZASSERT(tb == 207);
    ZASSERT(va == 130);
    ZASSERT(vb == 230);

    printf("RTLD_GLOBAL_OK: a=%s(%d,%d) b=%s(%d,%d) default=%s\n", sa, ta, va, sb, tb, vb, sd);

    dlclose(hb);
    dlclose(ha);
    return 0;
}
