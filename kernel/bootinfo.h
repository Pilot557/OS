#pragma once
#include <stdint.h>

typedef struct {
    void   *framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scanline;
    uint32_t pixel_format;   /* 0 = RGB, 1 = BGR */
} BootInfo;
