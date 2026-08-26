#include <kernel/window_system.h>

int window_file_explorer_open(WindowManager *wm)
{
    Window *window = window_default_open(wm, "File Explorer", 512, 420,
                                          0x202A35FFu);
    if (!window) {
        window_set_status(wm ? wm->master_console : NULL,
                          "error abriendo explorador");
        return -1;
    }
    window_set_status(window, "explorador listo");
    window_set_status(wm->master_console, "abriendo explorador");
    return 0;
}
