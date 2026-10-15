/* THE FACE AS AN IMAGE (queue item 10): the framebuffer as a 24-bit BMP,
 * streamed -- a 54-byte header and 240 x 240 x 3 bytes (172,854 in
 * all), rows bottom-up, BGR, no palette, no compression, no row
 * padding (720 B a row is a multiple of 4). The panel's swapped RGB565
 * words are turned into RGB888 on the fly, BMP_CHUNK bytes at a time
 * through a sink: no encoder library, no second buffer. Header-only
 * and Arduino-free so net.cpp (`GET /screen.bmp`) and
 * tools/bmp_test.cpp write the very same bytes. */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define BMP_HEADER_BYTES 54
#define BMP_CHUNK        4096
#define BMP_SIZE(w, h)   (BMP_HEADER_BYTES + (size_t)(w) * (h) * 3)

typedef void (*bmp_sink_t)(const uint8_t *bytes, size_t n, void *ctx);

static inline void bmp_le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static inline void bmp_le16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

/* the 54 bytes: BITMAPFILEHEADER (14) + BITMAPINFOHEADER (40) */
static inline void bmp_header(uint8_t *h, int w, int hgt)
{
    uint32_t px = (uint32_t)w * (uint32_t)hgt * 3;
    h[0] = 'B'; h[1] = 'M';
    bmp_le32(h + 2, BMP_HEADER_BYTES + px);   /* the file's size */
    bmp_le32(h + 6, 0);                       /* reserved */
    bmp_le32(h + 10, BMP_HEADER_BYTES);       /* the pixels begin here */
    bmp_le32(h + 14, 40);                     /* BITMAPINFOHEADER */
    bmp_le32(h + 18, (uint32_t)w);
    bmp_le32(h + 22, (uint32_t)hgt);          /* positive: rows bottom-up */
    bmp_le16(h + 26, 1);                      /* planes */
    bmp_le16(h + 28, 24);                     /* bits a pixel */
    bmp_le32(h + 30, 0);                      /* BI_RGB */
    bmp_le32(h + 34, px);
    bmp_le32(h + 38, 2835);                   /* 72 dpi, in px/m */
    bmp_le32(h + 42, 2835);
    bmp_le32(h + 46, 0);                      /* colours: all */
    bmp_le32(h + 50, 0);
}

/* fb: w x h RGB565 words in the PANEL's byte order (high byte second,
 * as the framebuffer holds them). Rows go out bottom-up; the sink is
 * handed the header, then the pixels in chunks of at most BMP_CHUNK. */
static inline void bmp_write(const uint16_t *fb, int w, int h, bmp_sink_t sink, void *ctx)
{
    uint8_t buf[BMP_CHUNK];
    bmp_header(buf, w, h);
    sink(buf, BMP_HEADER_BYTES, ctx);
    size_t n = 0;
    for (int y = h - 1; y >= 0; y--) {
        const uint16_t *row = fb + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            uint16_t s = row[x];
            uint16_t v = (uint16_t)((s >> 8) | (s << 8));        /* back to RGB565 */
            uint8_t r = (uint8_t)((v >> 11) & 31), g = (uint8_t)((v >> 5) & 63), b = (uint8_t)(v & 31);
            buf[n++] = (uint8_t)((b << 3) | (b >> 2));
            buf[n++] = (uint8_t)((g << 2) | (g >> 4));
            buf[n++] = (uint8_t)((r << 3) | (r >> 2));
            if (n + 3 > BMP_CHUNK) { sink(buf, n, ctx); n = 0; }
        }
    }
    if (n) sink(buf, n, ctx);
}
