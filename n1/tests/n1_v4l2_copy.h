/* Fast, bounded reads from the N1 decoder's CPU-visible DMA mappings.
 * Retains detached CPU frames so old renderer references cannot pin CAPTURE
 * allocations across seek/reset. No reads extend past the requested plane.
 */
#ifndef AVCODEC_N1_V4L2_COPY_H
#define AVCODEC_N1_V4L2_COPY_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline void n1_v4l2_copy_bytes(uint8_t *dst, const uint8_t *src, size_t size)
{
#if defined(__aarch64__)
    size_t blocks = size / 64;
    if (blocks && !(((uintptr_t)src | (uintptr_t)dst) & 15)) {
        /* LDP/STP outperformed structured LD1/ST1 by about 4x on the
         * actual Mali/S905D DMA mapping. Keep independent load addresses. */
        __asm__ volatile (
            "1: ldp q0, q1, [%[s], #0]\n"
            "ldp q2, q3, [%[s], #32]\n"
            "stp q0, q1, [%[d], #0]\n"
            "stp q2, q3, [%[d], #32]\n"
            "add %[s], %[s], #64\n"
            "add %[d], %[d], #64\n"
            "subs %[n], %[n], #1\n"
            "b.ne 1b\n"
            : [s] "+r"(src), [d] "+r"(dst), [n] "+r"(blocks)
            : : "v0", "v1", "v2", "v3", "cc", "memory");
        size &= 63;
    }
#endif
    if (size) memcpy(dst, src, size);
}

static inline void n1_v4l2_copy_plane(uint8_t *dst, ptrdiff_t dst_stride,
                                    const uint8_t *src, ptrdiff_t src_stride,
                                    size_t width, size_t height)
{
    if (dst_stride == (ptrdiff_t)width && src_stride == (ptrdiff_t)width) {
        n1_v4l2_copy_bytes(dst, src, width * height);
        return;
    }
    while (height--) {
        n1_v4l2_copy_bytes(dst, src, width);
        dst += dst_stride;
        src += src_stride;
    }
}
#endif
