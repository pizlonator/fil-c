#include <stdfil.h>
#include <stdbool.h>
#include <string.h>

extern "C" void __gxx_personality_v0();

extern "C" void* opaque(void*);

static bool callback(zstack_frame_description description,
                     void* arg)
{
    ZASSERT(arg == (void*)666);
    ZASSERT(description.is_inline ||
            description.can_throw ||
            !strcmp(description.function_name, "__libc_start_main"));
    if ((!strcmp(description.filename, "<runtime>") &&
         !strcmp(description.function_name, "start_program")) ||
        !strcmp(description.function_name, "__libc_start_main")) {
        ZASSERT(description.can_catch);
        ZASSERT(!description.personality_function);
        ZASSERT(!description.eh_data);
        ZASSERT(!description.is_inline);
    } else {
        if (!strcmp(description.function_name, "main"))
            ZASSERT(!description.is_inline);
        if (description.is_inline) {
            ZASSERT(!description.can_catch);
            ZASSERT(!description.personality_function);
            ZASSERT(!description.eh_data);
        } else {
            ZASSERT(description.can_catch);
            ZASSERT(description.personality_function == __gxx_personality_v0);
        }
    }
    zprintf("%s,%s,%u,%u;",
            description.function_name, description.filename, description.line, description.column);
    return true;
}

struct RAII {
    RAII() = default;
    ~RAII() { opaque(nullptr); }
};

static void foo(void)
{
    RAII raii;
    zstack_scan(callback, (void*)666);
}

static void bar(void)
{
    RAII raii;
    foo();
}

int main()
{
    RAII raii;
    bar();
    zprintf("\n");
    return 0;
}

