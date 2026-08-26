#include <kernel/window_system.h>

static void fill_rect(Window *window, Rect rect, uint32_t color)
{
    if (!window || !window->backbuffer.pixels)
        return;
    for (int32_t y = rect.y; y < rect.y + rect.height; y++) {
        if (y < 0 || y >= (int32_t)window->backbuffer.height)
            continue;
        for (int32_t x = rect.x; x < rect.x + rect.width; x++)
            if (x >= 0 && x < (int32_t)window->backbuffer.width)
                window->backbuffer.pixels[
                    y * window->backbuffer.stride_pixels + x] = color;
    }
}

void window_master_console_render(Window *window)
{
    if (!window)
        return;
    window_fill(window, 0x101820FFu);
    fill_rect(window, window->titlebar, 0x263849FFu);
    fill_rect(window, window->client, 0x0B1118FFu);
    fill_rect(window, window->footer, 0x17232EFFu);
    fill_rect(window, window->maximize_button, 0x3E6B8FFFu);
    fill_rect(window, window->minimize_button, 0x3E6B8FFFu);
    fill_rect(window, window->close_button, 0xB83B4BFFu);
    fill_rect(window, (Rect){
        window->client.x + 16,
        window->client.y + window->client.height - 42,
        window->client.width - 32,
        26
    }, 0x17232EFFu);
}

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

int window_master_console_execute(WindowManager *wm, const char *command)
{
    if (command_is(command, "scstat"))
        return window_device_status_open(wm);

    if (command_is(command, "file") || command_is(command, "files"))
        return window_file_explorer_open(wm);

    if (command_is(command, "time") || command_is(command, "hour"))
        return window_clock_toggle(wm);

    if (command_is(command, "web") || command_is(command, "browser"))
        return window_web_open(wm);

    window_set_status(wm->master_console, "comando no reconocido");
    return -1;
}

static void master_console_key_down(
    Window *window,
    uint16_t scancode,
    uint8_t modifiers
)
{
    WindowManager *wm = window ? (WindowManager *)window->owner : NULL;
    char character = 0;
    static char last_command[64];
    (void)modifiers;

    if (!window || !wm)
        return;
    if (scancode == 0x0e) {
        if (window->input_length)
            window->input_text[--window->input_length] = '\0';
    } else if (scancode == 0x0f) {
        size_t index = 0;
        while (last_command[index] && index + 1 < sizeof(window->input_text)) {
            window->input_text[index] = last_command[index];
            index++;
        }
        window->input_text[index] = '\0';
        window->input_length = (uint32_t)index;
    } else if (scancode == 0x1c) {
        size_t index = 0;
        while (window->input_text[index] && index + 1 < sizeof(last_command)) {
            last_command[index] = window->input_text[index];
            index++;
        }
        last_command[index] = '\0';
        window_master_console_execute(wm, window->input_text);
        window->input_length = 0;
        window->input_text[0] = '\0';
    } else {
        switch (scancode) {
        case 0x10: character = 'q'; break; case 0x11: character = 'w'; break;
        case 0x12: character = 'e'; break; case 0x13: character = 'r'; break;
        case 0x14: character = 't'; break; case 0x15: character = 'y'; break;
        case 0x16: character = 'u'; break; case 0x17: character = 'i'; break;
        case 0x18: character = 'o'; break; case 0x19: character = 'p'; break;
        case 0x1e: character = 'a'; break; case 0x1f: character = 's'; break;
        case 0x20: character = 'd'; break; case 0x21: character = 'f'; break;
        case 0x22: character = 'g'; break; case 0x23: character = 'h'; break;
        case 0x24: character = 'j'; break; case 0x25: character = 'k'; break;
        case 0x26: character = 'l'; break; case 0x2c: character = 'z'; break;
        case 0x2d: character = 'x'; break; case 0x2e: character = 'c'; break;
        case 0x2f: character = 'v'; break; case 0x30: character = 'b'; break;
        case 0x31: character = 'n'; break; case 0x32: character = 'm'; break;
        case 0x35: character = '/'; break; case 0x34: character = '.'; break;
        case 0x0c: character = '-'; break;
        case 0x27: character = (modifiers & 1) ? ':' : ';'; break;
        case 0x0b: character = '0'; break; case 0x02: character = '1'; break;
        case 0x03: character = '2'; break; case 0x04: character = '3'; break;
        case 0x05: character = '4'; break; case 0x06: character = '5'; break;
        case 0x07: character = '6'; break; case 0x08: character = '7'; break;
        case 0x09: character = '8'; break; case 0x0a: character = '9'; break;
        default: break;
        }
        if (character && window->input_length + 1 < sizeof(window->input_text)) {
            window->input_text[window->input_length++] = character;
            window->input_text[window->input_length] = '\0';
        }
    }
    window_master_console_render(window);
    window_mark_dirty(wm, window, NULL);
#ifdef KERNEL_BUILD
    window_manager_present(wm);
#endif
}

WindowCallbacks window_master_console_callbacks(void)
{
    return (WindowCallbacks){
        .key_down = master_console_key_down
    };
}