#include <assert.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>

typedef int (*function)(void);
typedef function (*function_getter)(void);
typedef int *(*address_getter)(void);

static address_getter provider_tls, consumer_tls;
static address_getter provider_protected_tls, consumer_protected_tls;

static void check_function(void *library, const char *name, int expected)
{
    function f = (function)dlsym(library, name);
    assert(f);
    assert(f() == expected);
}

static void check_pointer(void *library, const char *name, int expected)
{
    function_getter get = (function_getter)dlsym(library, name);
    assert(get);
    function volatile f = get();
    assert(f() == expected);
}

static void *check_tls(void *unused)
{
    int *a = provider_tls(), *b = consumer_tls();
    assert(a != b && *a == 111 && *b == 17);
    *b = 29;
    assert(*a == 111 && *consumer_tls() == 29);

    a = provider_protected_tls();
    b = consumer_protected_tls();
    assert(a != b && *a == 115 && *b == 23);
    *b = 31;
    assert(*a == 115 && *consumer_protected_tls() == 31);
    return unused;
}

__attribute__((visibility("default"))) int main(void)
{
    void *provider = dlopen("filc/test-output/hiddenvisibility/libprovider.so",
                           RTLD_NOW | RTLD_GLOBAL);
    assert(provider);
    void *consumer = dlopen("filc/test-output/hiddenvisibility/libconsumer.so",
                           RTLD_NOW | RTLD_LOCAL);
    assert(consumer);

    check_function(consumer, "read_hidden", 37);
    check_pointer(consumer, "hidden_pointer", 37);
    check_function(consumer, "cross_tu", 37);
    check_function(consumer, "public_alias", 37);
    check_function(consumer, "read_protected", 43);
    check_pointer(consumer, "protected_pointer", 43);
    check_function(consumer, "protected_function", 43);
    /* Control: public functions must still interpose. */
    check_function(consumer, "read_default", 97);
    check_pointer(consumer, "default_pointer", 97);
    check_function(consumer, "check_callee", 1);
    assert(!dlsym(consumer, "hidden_function"));
    assert(!dlsym(consumer, "hidden_global"));
    assert(!dlsym(consumer, "hidden_tls"));

    address_getter a = (address_getter)dlsym(provider, "provider_global");
    address_getter b = (address_getter)dlsym(consumer, "consumer_global");
    assert(a && b && a() != b() && *a() == 113 && *b() == 19);
    *b() = 41;
    assert(*a() == 113 && *b() == 41);

    provider_tls = (address_getter)dlsym(provider, "provider_tls");
    consumer_tls = (address_getter)dlsym(consumer, "consumer_tls");
    provider_protected_tls = (address_getter)dlsym(provider, "provider_protected_tls");
    consumer_protected_tls = (address_getter)dlsym(consumer, "consumer_protected_tls");
    assert(provider_tls && consumer_tls && provider_protected_tls && consumer_protected_tls);
    check_tls(NULL);
    pthread_t thread;
    assert(!pthread_create(&thread, NULL, check_tls, NULL));
    assert(!pthread_join(thread, NULL));
    assert(*consumer_tls() == 29 && *consumer_protected_tls() == 31);

    puts("hidden visibility ok");
    return 0;
}
