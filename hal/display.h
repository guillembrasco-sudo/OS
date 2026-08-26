#ifndef HAL_DISPLAY_H
#define HAL_DISPLAY_H

#include <stdint.h>

enum display_pixel_format {
	DISPLAY_XRGB8888 = 1,
	DISPLAY_ARGB8888 = 2
};

struct display_mode {
	uint32_t width;
	uint32_t height;
	uint32_t refresh_hz;
	uint32_t format;
};

int display_early_console_init(uint64_t multiboot_info_addr);
int display_set_mode(const struct display_mode *mode);
void display_console_putc(char character);
void display_framebuffer_blit(const uint32_t *pixels, uint32_t width,
							  uint32_t height, uint32_t stride_pixels,
							  int32_t x, int32_t y);
void display_framebuffer_draw_text(const char *text, int32_t x, int32_t y,
								   uint32_t color);
void display_framebuffer_draw_cursor(int32_t x, int32_t y);
void display_framebuffer_clear(void);
void display_framebuffer_present_cursor(void);
void display_framebuffer_fill_rect(int32_t x, int32_t y, uint32_t width,
								   uint32_t height, uint32_t color);

#endif
