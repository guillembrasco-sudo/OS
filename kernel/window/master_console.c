#include <kernel/window_system.h>

static int command_is(const char *command, const char *expected)
{
    size_t index = 0;

    if (!command || !expected)
        return 0;
    while (command[index] && expected[index] &&
           command[index] == expected[index])
        ++index;
    return command[index] == '\0' && expected[index] == '\0';
}

static Window *open_command_window(
    WindowManager *wm,
    const char *title,
    int32_t x,
    int32_t y,
    uint32_t color
)
{
    if (!wm)
        return NULL;
    return window_create(
        wm, title, x, y, 420, 220, color, NULL, NULL
    );
}

int window_master_console_execute(WindowManager *wm, const char *command)
{
    if (command_is(command, "scstat"))
        return open_command_window(
            wm, "Device Status", 64, 420, 0x18252FFFu
        ) ? 0 : -1;

    if (command_is(command, "file") || command_is(command, "files"))
        return open_command_window(
            wm, "File Explorer", 512, 420, 0x202A35FFu
        ) ? 0 : -1;

    if (command_is(command, "time") || command_is(command, "hour"))
        return open_command_window(
            wm, "System Time", 288, 180, 0x263322FFu
        ) ? 0 : -1;

    return -1;
}

static void master_console_key_down(
    Window *window,
    uint16_t scancode,
    uint8_t modifiers
)
{
    (void)window;
    (void)scancode;
    (void)modifiers;
}

WindowCallbacks window_master_console_callbacks(void)
{
    return (WindowCallbacks){
        .key_down = master_console_key_down
    };
}