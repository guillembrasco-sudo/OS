#include <hal/display.h>

int display_set_mode(const struct display_mode *mode)
{
	if (mode == 0 || mode->width == 0 || mode->height == 0)
		return -1;
	return 0;
}
