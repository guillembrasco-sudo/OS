#include <kernel/window_system.h>

static int url_is_exact(const char *url)
{
    size_t index = 0;
    size_t host_start;
    if (!url)
        return 0;
    if (url[0] == 'h' && url[1] == 't' && url[2] == 't' &&
        url[3] == 'p' && url[4] == 's' && url[5] == ':' &&
        url[6] == '/' && url[7] == '/')
        host_start = 8;
    else if (url[0] == 'h' && url[1] == 't' && url[2] == 't' &&
             url[3] == 'p' && url[4] == ':' && url[5] == '/' &&
             url[6] == '/')
        host_start = 7;
    else
        return 0;
    if (!url[host_start])
        return 0;
    for (index = host_start; url[index]; index++)
        if (url[index] <= ' ' || url[index] == '<' || url[index] == '>')
            return 0;
    return index > host_start;
}

static int url_is_https(const char *url)
{
    return url && url[0] == 'h' && url[1] == 't' && url[2] == 't' &&
           url[3] == 'p' && url[4] == 's';
}

static void web_key_down(Window *window, uint16_t scancode, uint8_t modifiers)
{
    static char last_url[64];
    WindowManager *wm = window ? (WindowManager *)window->owner : NULL;
    char character = 0;
    size_t index;
    (void)modifiers;
    if (!window || !wm)
        return;
    if (scancode == 0x0e) {
        if (window->input_length)
            window->input_text[--window->input_length] = '\0';
    } else if (scancode == 0x0f) {
        for (index = 0; index + 1 < sizeof(window->input_text) && last_url[index]; index++)
            window->input_text[index] = last_url[index];
        window->input_text[index] = '\0';
        window->input_length = (uint32_t)index;
    } else if (scancode == 0x1c) {
        for (index = 0; index + 1 < sizeof(last_url) && window->input_text[index]; index++)
            last_url[index] = window->input_text[index];
        last_url[index] = '\0';
        if (url_is_https(window->input_text))
            window_set_status(window, "HTTPS requiere TLS");
        else
            window_set_status(window, url_is_exact(window->input_text) ?
                              "URL HTTP exacta aceptada" : "URL no valida");
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
    window_mark_dirty(wm, window, NULL);
    window_manager_present(wm);
}

int window_web_open(WindowManager *wm)
{
    WindowCallbacks callbacks = {0};
    Window *window;
    if (!wm)
        return -1;
    callbacks.key_down = web_key_down;
    window = window_create(wm, "Web", 120, 120, 700, 420,
                           0x172A3AFFu, &callbacks, wm);
    if (!window)
        return -1;
    window_set_status(window, "Escribe una URL exacta y pulsa Enter");
    wm->focused = window;
    window->flags |= WINDOW_FLAG_FOCUSED | WINDOW_FLAG_ACTIVE;
    window_manager_present(wm);
    return 0;
}
