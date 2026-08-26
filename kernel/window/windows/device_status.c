#include <kernel/window_system.h>

int window_device_status_open(WindowManager *wm)
{
    Window *window = window_default_open(wm, "Device Status", 64, 420,
                                          0x18252FFFu);
    if (!window) {
        window_set_status(wm ? wm->master_console : NULL,
                          "error abriendo estado");
        return -1;
    }
    window_set_status(window, "dispositivos activos");
    window_set_status(wm->master_console, "abriendo estado");
    return 0;
}
