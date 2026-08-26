#include <kernel/window_system.h>
#include <hal/clock.h>

static Window *clock_window;

static void set_clock_status(Window *window)
{
    struct clock_datetime datetime;
    char text[] = "hora 00:00:00";
    if (!window)
        return;
    if (clock_read_rtc(&datetime) != 0) {
        window_set_status(window, "hora no disponible");
        return;
    }
    text[5] = (char)('0' + datetime.hour / 10);
    text[6] = (char)('0' + datetime.hour % 10);
    text[8] = (char)('0' + datetime.minute / 10);
    text[9] = (char)('0' + datetime.minute % 10);
    text[11] = (char)('0' + datetime.second / 10);
    text[12] = (char)('0' + datetime.second % 10);
    window_set_status(window, text);
}

int window_clock_toggle(WindowManager *wm)
{
    if (!wm)
        return -1;
    if (clock_window && clock_window->state != WINDOW_STATE_CLOSED) {
        window_destroy(wm, clock_window);
        clock_window = NULL;
        window_set_status(wm->master_console, "cerrando reloj");
        return 0;
    }
    clock_window = window_default_open(wm, "System Time", 288, 180,
                                       0x263322FFu);
    if (!clock_window) {
        window_set_status(wm->master_console, "error abriendo reloj");
        return -1;
    }
    set_clock_status(clock_window);
    window_set_status(wm->master_console, "abriendo reloj");
    return 0;
}
