#include <stdfil.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

static char* read_file_full(const char* path, size_t* out_len)
{
    int fd;
    do {
        fd = open(path, O_RDONLY);
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

static char* read_cmdline(size_t* out_len)
{
    return read_file_full("/proc/self/cmdline", out_len);
}

static char* read_environ(size_t* out_len)
{
    return read_file_full("/proc/self/environ", out_len);
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
    size_t orig_cmdline_size;
    char* orig_cmdline = read_cmdline(&orig_cmdline_size);
    free(orig_cmdline);
    size_t orig_environ_size;
    char* orig_environ = read_environ(&orig_environ_size);
    free(orig_environ);
    size_t orig_size = orig_cmdline_size + orig_environ_size;

    size_t title_len = orig_size + 1000;
    char* title = (char*)malloc(title_len + 1);
    ZASSERT(title);
    memset(title, 'X', title_len);
    title[title_len] = 0;

    zsetproctitle(title);

    size_t n;
    char* buf = read_cmdline(&n);
    size_t first_len = 0;
    while (first_len < n && buf[first_len])
        first_len++;
    ZASSERT(first_len > 0);
    ZASSERT(first_len < orig_size);
    for (size_t i = 0; i < first_len; i++)
        ZASSERT(buf[i] == 'X');
    free(buf);

    ZASSERT(!test_cmdline_contains("MARKER_TRUNC_ARG"));
    zprintf("ok\n");

    free(title);
    return 0;
}
