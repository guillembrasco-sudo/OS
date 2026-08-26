#include <kernel/window_system.h>

Window *window_default_open(WindowManager *wm, const char *title,
                            int32_t x, int32_t y, uint32_t color)
{
    if (!wm)
        return NULL;
    return window_create(wm, title, x, y, 420, 220, color, NULL, NULL);
}

void window_set_status(Window *window, const char *text)
{
    size_t index = 0;
    if (!window || !text)
        return;
    while (text[index] && index + 1 < sizeof(window->status_text)) {
        window->status_text[index] = text[index];
        index++;
    }
    window->status_text[index] = '\0';
}
