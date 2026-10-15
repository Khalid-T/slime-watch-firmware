/* THE FACE AS AN IMAGE on the laptop: the same chunked writer net.cpp
 * streams for `GET /screen.bmp` (src/bmp.h), into a file:
 *   c++ -std=gnu++17 -I src -o /tmp/bmp_test tools/bmp_test.cpp && /tmp/bmp_test /tmp/screen_test.bmp
 * A 240x240 test pattern -- pure red at the top-left, green top-right,
 * blue bottom-left, white bottom-right, a mid grey at the centre, black
 * elsewhere, in the PANEL's swapped RGB565 -- goes through the sink in
 * chunks (each at most BMP_CHUNK, the header alone first); the file
 * must be exactly BMP_SIZE bytes. Then, with Python/PIL:
 *   python3 -c "from PIL import Image; im = Image.open('/tmp/screen_test.bmp'); print(im.size, im.mode, im.getpixel((0,0)), im.getpixel((239,0)), im.getpixel((0,239)), im.getpixel((239,239)), im.getpixel((120,120)))"
 * expecting (240, 240) RGB (255,0,0) (0,255,0) (0,0,255) (255,255,255)
 * (132,130,132) -- the corners right way up (BMP rows are stored
 * bottom-up; the writer walks them so), the grey the 565 expansion of
 * 0x8410. */
#include "bmp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 240
#define H 240

static uint16_t fb[W * H];

static uint16_t swapped(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }

static size_t chunks = 0, biggest = 0, total = 0, first = 0;
static void sink(const uint8_t *b, size_t n, void *ctx)
{
    FILE *f = (FILE *)ctx;
    if (!chunks) first = n;
    chunks++;
    if (n > biggest) biggest = n;
    assert(n <= BMP_CHUNK);
    total += n;
    fwrite(b, 1, n, f);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/tmp/screen_test.bmp";
    memset(fb, 0, sizeof fb);
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
        fb[y * W + x] = swapped(0xF800);                          /* red, top-left */
        fb[y * W + (W - 1 - x)] = swapped(0x07E0);                /* green, top-right */
        fb[(H - 1 - y) * W + x] = swapped(0x001F);                /* blue, bottom-left */
        fb[(H - 1 - y) * W + (W - 1 - x)] = swapped(0xFFFF);      /* white, bottom-right */
    }
    for (int y = 116; y < 124; y++) for (int x = 116; x < 124; x++) fb[y * W + x] = swapped(0x8410);   /* a mid grey */
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    bmp_write(fb, W, H, sink, f);
    fclose(f);
    assert(total == BMP_SIZE(W, H));
    assert(total == 172854);
    assert(first == BMP_HEADER_BYTES);            /* the header goes alone, first */
    assert(biggest <= BMP_CHUNK);
    /* the header, read back */
    uint8_t h[BMP_HEADER_BYTES];
    f = fopen(path, "rb"); assert(f);
    assert(fread(h, 1, sizeof h, f) == sizeof h);
    assert(h[0] == 'B' && h[1] == 'M');
    assert((h[2] | (h[3] << 8) | (h[4] << 16)) == (int)BMP_SIZE(W, H));
    assert(h[10] == BMP_HEADER_BYTES && h[14] == 40 && h[18] == W && h[22] == H && h[26] == 1 && h[28] == 24 && h[30] == 0);
    /* the first stored row is the BOTTOM row: blue then, at its end, white; BGR order */
    uint8_t row[W * 3];
    assert(fread(row, 1, sizeof row, f) == sizeof row);
    assert(row[0] == 255 && row[1] == 0 && row[2] == 0);                              /* blue = B,G,R 255,0,0 */
    assert(row[(W - 1) * 3] == 255 && row[(W - 1) * 3 + 1] == 255 && row[(W - 1) * 3 + 2] == 255);
    fclose(f);
    printf("bmp_test: %s written, %zu B in %zu chunks (first %zu, biggest %zu); the header and the bottom row check\n",
           path, total, chunks, first, biggest);
    return 0;
}
