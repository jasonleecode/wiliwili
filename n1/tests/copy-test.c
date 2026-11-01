#define _GNU_SOURCE
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include "n1_v4l2_copy.h"

int main(void)
{
    const size_t page = sysconf(_SC_PAGESIZE);
    uint8_t *src = mmap(NULL, page * 3, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    uint8_t *dst = mmap(NULL, page * 3, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(src != MAP_FAILED && dst != MAP_FAILED);
    assert(!mprotect(src + page, page, PROT_READ | PROT_WRITE));
    assert(!mprotect(dst + page, page, PROT_READ | PROT_WRITE));
    for (size_t i = 0; i < page; i++) src[page + i] = i * 37 + 11;
    for (size_t n = 0; n <= page; n++) {
        memset(dst + page, 0xa5, page);
        n1_v4l2_copy_bytes(dst + 2 * page - n, src + 2 * page - n, n);
        assert(!memcmp(dst + 2 * page - n, src + 2 * page - n, n));
        if (n < page) assert(dst[2 * page - n - 1] == 0xa5);
    }
    munmap(src, page * 3); munmap(dst, page * 3);
    const int widths[] = {1, 15, 16, 31, 32, 63, 64, 65, 127, 128, 1918, 1920};
    for (unsigned w = 0; w < sizeof(widths) / sizeof(widths[0]); w++) {
        for (int negative = 0; negative < 2; negative++) {
            size_t stride = (widths[w] + 63) & ~63;
            size_t height = 1080, bytes = stride * height;
            uint8_t *a = malloc(bytes), *b = malloc(bytes), *expected = malloc(bytes);
            assert(a && b && expected);
            for (size_t i = 0; i < bytes; i++) a[i] = i * 19 + 7;
            memset(b, 0xa5, bytes); memset(expected, 0xa5, bytes);
            ptrdiff_t step = negative ? -(ptrdiff_t)stride : (ptrdiff_t)stride;
            size_t start = negative ? (height - 1) * stride : 0;
            n1_v4l2_copy_plane(b + start, step, a + start, step, widths[w], height);
            for (size_t y = 0; y < height; y++) memcpy(expected + y * stride, a + y * stride, widths[w]);
            assert(!memcmp(b, expected, bytes));
            free(a); free(b); free(expected);
        }
    }
    return 0;
}
