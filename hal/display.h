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

int display_early_console_init(void);
int display_set_mode(const struct display_mode *mode);

#endif
