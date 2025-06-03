/* kernel/kernel.c */
#include "bootinfo.h"
#include <stdint.h>

/* Basic put-pixel (32-bit XRGB) */
static inline void put_px(const BootInfo *bi,
                          uint32_t x, uint32_t y,
                          uint8_t r, uint8_t g, uint8_t b)
{
    if (x >= bi->width || y >= bi->height) return;
    uint32_t *fb = (uint32_t*)bi->framebuffer + y * bi->pixels_per_scanline + x;
    uint32_t col = (bi->pixel_format == 0) ?
                   (r << 16 | g << 8 | b) :     /* RGB */
                   (b << 16 | g << 8 | r);      /* BGR */
    *fb = col;
}

/* Draw a filled heart using the implicit equation
   (x² + y² − r)³ − x² y³ ≤ 0, scaled to screen */
static void draw_heart(const BootInfo *bi)
{
    const float r     = 10000.0f;
    const int   scale = 18;        /* tweak size */
    int cx = bi->width  / 2;
    int cy = bi->height / 2 - 20;

    for (int iy = -60; iy <= 60; ++iy) {
        for (int ix = -60; ix <= 60; ++ix) {
            float x = ix;
            float y = iy;
            float f = (x*x + y*y - r/scale) * (x*x + y*y - r/scale) * (x*x + y*y - r/scale)
                      - x*x * y*y*y;
            if (f <= 0.0f) {
                put_px(bi, cx + ix, cy + iy, 255, 50, 80);   /* pink-red */
            }
        }
    }
}

/* Entry from bootloader */
void kmain(BootInfo *bi)
{
    /* Clear screen */
    for (uint32_t y = 0; y < bi->height; ++y)
        for (uint32_t x = 0; x < bi->width; ++x)
            put_px(bi, x, y, 0, 0, 0);

    draw_heart(bi);

    /* Spin forever */
    for (;;) __asm__ __volatile__("hlt");
}
