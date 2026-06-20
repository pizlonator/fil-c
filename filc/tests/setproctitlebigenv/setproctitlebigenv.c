#include <stdfil.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

static char* read_cmdline(size_t* out_len)
{
    int fd;
    do {
        fd = open("/proc/self/cmdline", O_RDONLY);
    } while (fd == -1 && errno == EINTR);
    ZASSERT(fd >= 0);

    size_t cap = 4096;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    ZASSERT(buf);

    for (;;) {
        ssize_t result;
        do {
            result = read(fd, buf + len, cap - len);
        } while (result == -1 && errno == EINTR);

        if (result == 0)
            break;
        ZASSERT(result > 0);

        len += result;
        if (len == cap) {
            cap *= 2;
            buf = (char*)realloc(buf, cap);
            ZASSERT(buf);
        }
    }

    ZASSERT(!close(fd));
    buf[len] = 0;
    *out_len = len;
    return buf;
}

int test_cmdline_starts_with(const char* expected)
{
    size_t n;
    char* buf = read_cmdline(&n);
    size_t expected_len = strlen(expected);
    size_t first_len = 0;
    while (first_len < n && buf[first_len])
        first_len++;
    int result = first_len == expected_len && !memcmp(buf, expected, expected_len);
    free(buf);
    return result;
}

int test_cmdline_contains(const char* substring)
{
    size_t n;
    char* buf = read_cmdline(&n);
    size_t sub_len = strlen(substring);
    if (!sub_len) {
        free(buf);
        return 1;
    }
    if (sub_len > n) {
        free(buf);
        return 0;
    }
    int result = 0;
    for (size_t i = 0; i <= n - sub_len; i++) {
        if (!memcmp(buf + i, substring, sub_len)) {
            result = 1;
            break;
        }
    }
    free(buf);
    return result;
}

int main()
{
    const char* title = "bigenv title";
    zsetproctitle(title);
    ZASSERT(test_cmdline_starts_with(title));
    ZASSERT(!test_cmdline_contains("MARKER_ENV"));
    zprintf("ok\n");
    return 0;
}
