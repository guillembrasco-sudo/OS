#include <kernel/window_system.h>

#include <string.h>
#include <limits.h>

#define DEFAULT_TITLEBAR_H   35u
#define DEFAULT_FOOTER_H    25u
#define DEFAULT_BORDER_W     1u
#define DEFAULT_PADDING     25u
#define DEFAULT_MIN_W       160
#define DEFAULT_MIN_H       90

#define BUTTON_W             90
#define BUTTON_H             24
#define BUTTON_GAP           15
#define BUTTON_Y              5
#define CLOSE_W              36
#define CLOSE_H              18

#define ANIMATION_OPEN_MS   600u
#define ANIMATION_CLOSE_MS  500u

static void *default_alloc(size_t size, void *ctx) {
    (void)ctx;
    /* Replace with kmalloc/pool allocator in the OS. */
    return __builtin_calloc(1, size);
}

static void default_free(void *ptr, void *ctx) {
    (void)ctx;
    __builtin_free(ptr);
}

static WindowAllocator normalize_allocator(WindowAllocator a) {
    if (!a.alloc) a.alloc = default_alloc;
    if (!a.free)  a.free = default_free;
    return a;
}

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int rect_contains(Rect r, int32_t x, int32_t y) {
    if (r.width <= 0 || r.height <= 0) return 0;
    return asm_window_hit_test(x, y, r.x, r.y, r.width, r.height);
}

static Rect rect_intersect(Rect a, Rect b) {
    const int32_t left = a.x > b.x ? a.x : b.x;
    const int32_t top = a.y > b.y ? a.y : b.y;
    const int32_t ar = a.x + a.width;
    const int32_t br = b.x + b.width;
    const int32_t ab = a.y + a.height;
    const int32_t bb = b.y + b.height;
    const int32_t right = ar < br ? ar : br;
    const int32_t bottom = ab < bb ? ab : bb;

    Rect out = { left, top, right - left, bottom - top };
    if (out.width < 0) out.width = 0;
    if (out.height < 0) out.height = 0;
    return out;
}

static void mark_damage_internal(WindowManager *wm, Window *window, const Rect *r) {
    if (!wm || !window || !r || r->width <= 0 || r->height <= 0) return;
    window->damage.rect = *r;
    window->damage.valid = 1;

    if (wm->mark_damage)
        wm->mark_damage(window, r, wm->damage_ctx);
}

static void notify_taskbar_internal(WindowManager *wm, Window *window) {
    if (wm && wm->notify_taskbar)
        wm->notify_taskbar(window, window->state, wm->taskbar_ctx);
}

static void link_window(WindowManager *wm, Window *window) {
    window->prev = wm->tail;
    window->next = NULL;

    if (wm->tail)
        wm->tail->next = window;
    else
        wm->head = window;

    wm->tail = window;
    window->z_index = 0;

    for (Window *it = wm->head; it; it = it->next)
        if (it != window)
            ++it->z_index;
}

static void unlink_window(WindowManager *wm, Window *window) {
    if (window->prev)
        window->prev->next = window->next;
    else
        wm->head = window->next;

    if (window->next)
        window->next->prev = window->prev;
    else
        wm->tail = window->prev;

    if (wm->focused == window)
        wm->focused = NULL;

    window->prev = NULL;
    window->next = NULL;
}

static void raise_window(WindowManager *wm, Window *window) {
    if (!wm || !window || wm->tail == window) {
        if (wm) wm->focused = window;
        return;
    }

    unlink_window(wm, window);
    link_window(wm, window);
    wm->focused = window;
    window->flags |= WINDOW_FLAG_FOCUSED | WINDOW_FLAG_ACTIVE;
}

void window_manager_init(
    WindowManager *wm,
    uint32_t display_width,
    uint32_t display_height,
    WindowAllocator allocator
) {
    if (!wm) return;

    memset(wm, 0, sizeof(*wm));
    wm->display.width = display_width;
    wm->display.height = display_height;
    wm->allocator = normalize_allocator(allocator);
    wm->next_id = 1;
}

void window_manager_set_damage_callback(
    WindowManager *wm,
    void (*mark_damage)(Window *window, const Rect *rect, void *ctx),
    void *ctx
) {
    if (!wm) return;
    wm->mark_damage = mark_damage;
    wm->damage_ctx = ctx;
}

void window_manager_set_taskbar_callback(
    WindowManager *wm,
    void (*notify_taskbar)(Window *window, WindowState state, void *ctx),
    void *ctx
) {
    if (!wm) return;
    wm->notify_taskbar = notify_taskbar;
    wm->taskbar_ctx = ctx;
}

void window_manager_set_standby_callback(
    WindowManager *wm,
    WindowStandbyFn callback,
    void *ctx
)
{
    if (!wm) return;
    wm->standby_callback = callback;
    wm->standby_ctx = ctx;
}

void window_manager_enter_standby(WindowManager *wm)
{
    if (!wm || wm->standby) return;

    wm->standby = 1;
    if (wm->standby_callback)
        wm->standby_callback(1, wm->standby_ctx);

    for (Window *it = wm->head; it; it = it->next)
        mark_damage_internal(wm, it, &it->clip);
}

void window_manager_wake(WindowManager *wm)
{
    if (!wm || !wm->standby) return;

    wm->standby = 0;
    if (wm->standby_callback)
        wm->standby_callback(0, wm->standby_ctx);

    for (Window *it = wm->head; it; it = it->next)
        mark_damage_internal(wm, it, &it->clip);
}

int window_manager_is_standby(const WindowManager *wm)
{
    return wm ? wm->standby != 0 : 0;
}

static int allocate_backbuffer(Window *window) {
    const size_t pixels = (size_t)window->width * (size_t)window->height;
    const size_t bytes = pixels * sizeof(uint32_t);

    if (pixels == 0 || pixels > SIZE_MAX / sizeof(uint32_t))
        return 0;

    window->backbuffer.pixels =
        (uint32_t *)window->allocator.alloc(bytes, window->allocator.ctx);

    if (!window->backbuffer.pixels)
        return 0;

    window->backbuffer.width = (uint32_t)window->width;
    window->backbuffer.height = (uint32_t)window->height;
    window->backbuffer.stride_pixels = (uint32_t)window->width;
    window->backbuffer.bytes = bytes;
    return 1;
}

static int resize_backbuffer(Window *window, int32_t new_width, int32_t new_height) {
    if (new_width < 1 || new_height < 1)
        return 0;

    if (new_width == window->width && new_height == window->height)
        return 1;

    const size_t pixels = (size_t)new_width * (size_t)new_height;
    if (pixels == 0 || pixels > SIZE_MAX / sizeof(uint32_t))
        return 0;

    const size_t bytes = pixels * sizeof(uint32_t);
    uint32_t *new_buffer =
        (uint32_t *)window->allocator.alloc(bytes, window->allocator.ctx);

    if (!new_buffer)
        return 0;

    const int32_t old_width = window->width;
    const int32_t old_height = window->height;
    uint32_t *old_buffer = window->backbuffer.pixels;

    if (old_buffer) {
        const int32_t copy_w = old_width < new_width ? old_width : new_width;
        const int32_t copy_h = old_height < new_height ? old_height : new_height;

        for (int32_t y = 0; y < copy_h; ++y) {
            memcpy(
                new_buffer + (size_t)y * (size_t)new_width,
                old_buffer + (size_t)y * (size_t)old_width,
                (size_t)copy_w * sizeof(uint32_t)
            );
        }

        window->allocator.free(old_buffer, window->allocator.ctx);
    }

    window->backbuffer.pixels = new_buffer;
    window->backbuffer.width = (uint32_t)new_width;
    window->backbuffer.height = (uint32_t)new_height;
    window->backbuffer.stride_pixels = (uint32_t)new_width;
    window->backbuffer.bytes = bytes;
    return 1;
}

Window *window_create(
    WindowManager *wm,
    const char *title,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint32_t clear_color,
    const WindowCallbacks *callbacks,
    void *owner
) {
    if (!wm || width < DEFAULT_MIN_W || height < DEFAULT_MIN_H)
        return NULL;

    Window *window =
        (Window *)wm->allocator.alloc(sizeof(Window), wm->allocator.ctx);

    if (!window)
        return NULL;

    memset(window, 0, sizeof(*window));
    window->allocator = wm->allocator;
    window->display = wm->display;
    window->id = wm->next_id++;
    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->saved_x = x;
    window->saved_y = y;
    window->saved_width = width;
    window->saved_height = height;
    window->min_width = DEFAULT_MIN_W;
    window->min_height = DEFAULT_MIN_H;
    window->clear_color = clear_color;
    window->titlebar_height = DEFAULT_TITLEBAR_H;
    window->footer_height = DEFAULT_FOOTER_H;
    window->border_width = DEFAULT_BORDER_W;
    window->collapsed_height =
        window->titlebar_height + window->footer_height;
    window->content_padding = DEFAULT_PADDING;
    window->state = WINDOW_STATE_NORMAL;
    window->flags =
        WINDOW_FLAG_VISIBLE |
        WINDOW_FLAG_HAS_BORDER |
        WINDOW_FLAG_HAS_CLOSE |
        WINDOW_FLAG_HAS_MINIMIZE |
        WINDOW_FLAG_HAS_MAXIMIZE |
        WINDOW_FLAG_RESIZABLE |
        WINDOW_FLAG_DRAGGABLE |
        WINDOW_FLAG_DIRTY |
        WINDOW_FLAG_ACTIVE;
    window->callbacks = callbacks ? *callbacks : (WindowCallbacks){0};
    window->owner = owner;

    if (title) {
        size_t len = strlen(title);

        if (len > sizeof(window->title) - 1)
            len = sizeof(window->title) - 1;

        memcpy(window->title, title, len);
        window->title[len] = '\0';
        window->title_length = (uint32_t)len;
    }

    if (!allocate_backbuffer(window)) {
        wm->allocator.free(window, wm->allocator.ctx);
        return NULL;
    }

    window_recalculate_layout(window);
    link_window(wm, window);
    raise_window(wm, window);
    window_clear(wm, window);

    return window;
}

void window_destroy(WindowManager *wm, Window *window) {
    if (!wm || !window || window->state == WINDOW_STATE_CLOSED)
        return;

    Rect old_bounds = window_get_bounds(window);
    mark_damage_internal(wm, window, &old_bounds);

    window->state = WINDOW_STATE_CLOSED;
    window->flags &= ~(WINDOW_FLAG_VISIBLE | WINDOW_FLAG_ACTIVE |
                       WINDOW_FLAG_FOCUSED | WINDOW_FLAG_DIRTY |
                       WINDOW_FLAG_ANIMATING | WINDOW_FLAG_DRAGGABLE);

    unlink_window(wm, window);

    if (window->backbuffer.pixels)
        window->allocator.free(
            window->backbuffer.pixels,
            window->allocator.ctx
        );

    window->backbuffer.pixels = NULL;
    window->backbuffer.bytes = 0;
    window->allocator.free(window, window->allocator.ctx);
}

static void start_centered_height_animation(
    Window *window,
    int32_t target_height,
    uint32_t duration_ms
) {
    const int32_t collapsed = (int32_t)window->collapsed_height;
    const int32_t current_h = window->height;
    const int32_t initial_top =
        window->y + ((current_h - collapsed) / 2);

    const int32_t target_top =
        window->y + ((current_h - target_height) / 2);

    window_begin_animation(
        window,
        initial_top,
        target_top,
        collapsed,
        target_height,
        duration_ms
    );
}

int window_minimize(WindowManager *wm, Window *window) {
    if (!wm || !window || window->state == WINDOW_STATE_CLOSED)
        return 0;

    if (window->state == WINDOW_STATE_MINIMIZED)
        return 1;

    if (window->state == WINDOW_STATE_MAXIMIZED) {
        window->saved_x = window->x;
        window->saved_y = window->y;
        window->saved_width = window->width;
        window->saved_height = window->height;
    }

    const Rect old = window_get_bounds(window);

    window->state = WINDOW_STATE_MINIMIZED;
    window->flags &= ~WINDOW_FLAG_VISIBLE;

    mark_damage_internal(wm, window, &old);
    notify_taskbar_internal(wm, window);

    return 1;
}

int window_maximize(WindowManager *wm, Window *window) {
    if (!wm || !window || window->state == WINDOW_STATE_CLOSED)
        return 0;

    if (window->state == WINDOW_STATE_MAXIMIZED)
        return 1;

    window->saved_x = window->x;
    window->saved_y = window->y;
    window->saved_width = window->width;
    window->saved_height = window->height;

    const Rect old = window_get_bounds(window);

    window->x = 0;
    window->y = 0;
    window->width = (int32_t)wm->display.width;
    window->height = (int32_t)wm->display.height;
    window->state = WINDOW_STATE_MAXIMIZED;
    window->flags |= WINDOW_FLAG_VISIBLE | WINDOW_FLAG_DIRTY;

    if (!resize_backbuffer(window, window->width, window->height)) {
        window->x = window->saved_x;
        window->y = window->saved_y;
        window->width = window->saved_width;
        window->height = window->saved_height;
        return 0;
    }

    window_recalculate_layout(window);

    mark_damage_internal(wm, window, &old);
    mark_damage_internal(wm, window, &window->clip);
    notify_taskbar_internal(wm, window);

    return 1;
}

int window_restore(WindowManager *wm, Window *window) {
    if (!wm || !window || window->state == WINDOW_STATE_CLOSED)
        return 0;

    if (window->state == WINDOW_STATE_MAXIMIZED) {
        const Rect old = window_get_bounds(window);

        window->x = window->saved_x;
        window->y = window->saved_y;
        window->width = window->saved_width;
        window->height = window->saved_height;
        window->state = WINDOW_STATE_NORMAL;
        window->flags |= WINDOW_FLAG_VISIBLE | WINDOW_FLAG_DIRTY;

        if (!resize_backbuffer(window, window->width, window->height))
            return 0;

        window_recalculate_layout(window);

        mark_damage_internal(wm, window, &old);
        mark_damage_internal(wm, window, &window->clip);
        notify_taskbar_internal(wm, window);
        return 1;
    }

    if (window->state == WINDOW_STATE_MINIMIZED) {
        window->state = WINDOW_STATE_NORMAL;
        window->flags |= WINDOW_FLAG_VISIBLE | WINDOW_FLAG_DIRTY;
        window_recalculate_layout(window);
        mark_damage_internal(wm, window, &window->clip);
        notify_taskbar_internal(wm, window);
        return 1;
    }

    return 1;
}

int window_close(WindowManager *wm, Window *window) {
    if (!wm || !window || window->state == WINDOW_STATE_CLOSED)
        return 0;

    if (window == wm->master_console) {
        window_manager_enter_standby(wm);
        return 1;
    }

    if (window->close_latched)
        return 1;

    window->close_latched = 1;

    /* Matches the HTML's 150 ms visual delay semantically.
       The native server should implement the timing in its event loop. */
    if (window->callbacks.mouse_click)
        window->callbacks.mouse_click(window, -1, -1, 0);

    const Rect old = window_get_bounds(window);
    const int32_t collapsed = (int32_t)window->collapsed_height;
    const int32_t target_top =
        window->y + ((window->height - collapsed) / 2);

    window->flags |= WINDOW_FLAG_ANIMATING;
    window_begin_animation(
        window,
        window->y,
        target_top,
        window->height,
        collapsed,
        ANIMATION_CLOSE_MS
    );

    mark_damage_internal(wm, window, &old);
    return 1;
}

void window_recalculate_layout(Window *window) {
    if (!window) return;

    const int32_t border = (int32_t)window->border_width;
    const int32_t titlebar = (int32_t)window->titlebar_height;
    const int32_t footer = (int32_t)window->footer_height;
    const int32_t body_h =
        window->height - titlebar - footer - (2 * border);

    window->titlebar = (Rect){
        border,
        border,
        window->width - (2 * border),
        titlebar
    };

    window->footer = (Rect){
        border,
        window->height - border - footer,
        window->width - (2 * border),
        footer
    };

    const int32_t client_y = border + titlebar;
    const int32_t client_h = body_h > 0 ? body_h : 0;

    window->left_border = (Rect){
        border,
        client_y,
        5,
        client_h
    };

    window->right_border = (Rect){
        window->width - border - 5,
        client_y,
        5,
        client_h
    };

    window->client = (Rect){
        border + 5,
        client_y,
        window->width - (2 * border) - 10,
        client_h
    };

    window->clip = (Rect){
        0, 0, window->width, window->height
    };

    /* HTML has two 90 px text buttons at 15 px separation on the left. */
    window->maximize_button = (Rect){
        (int32_t)15,
        BUTTON_Y,
        BUTTON_W,
        BUTTON_H
    };

    window->minimize_button = (Rect){
        (int32_t)15 + BUTTON_W + BUTTON_GAP,
        BUTTON_Y,
        BUTTON_W,
        BUTTON_H
    };
    
    window->close_button = (Rect){
        window->width - 15 - CLOSE_W,
        (int32_t)((titlebar - CLOSE_H) / 2),
        CLOSE_W,
        CLOSE_H
    };
    
    /* Zona reservada para el título */
    const int32_t title_left =
        window->minimize_button.x +
        window->minimize_button.width + 12;
    
    const int32_t title_right =
        window->close_button.x - 12;
    
    window->title_rect = (Rect){
        title_left,
        0,
        title_right - title_left,
        (int32_t)window->titlebar_height
    };
}

void window_mark_dirty(WindowManager *wm, Window *window, const Rect *local_rect) {
    if (!wm || !window) return;

    window->flags |= WINDOW_FLAG_DIRTY;

    Rect local = local_rect ? *local_rect : window->clip;
    Rect clipped = rect_intersect(local, window->clip);
    if (clipped.width <= 0 || clipped.height <= 0)
        return;

    clipped.x += window->x;
    clipped.y += window->y;
    mark_damage_internal(wm, window, &clipped);
}

void window_begin_animation(
    Window *window,
    int32_t from_top,
    int32_t to_top,
    int32_t from_height,
    int32_t to_height,
    uint32_t duration_ms
) {
    if (!window || duration_ms == 0)
        return;

    window->animation_active = 1;
    window->animation_elapsed_ms = 0;
    window->animation_ms = duration_ms;
    window->animation_from_top = from_top;
    window->animation_to_top = to_top;
    window->animation_from_height = from_height;
    window->animation_to_height = to_height;
    window->flags |= WINDOW_FLAG_ANIMATING;
}

static uint32_t smooth_back_out(uint32_t p) {
    /*
     * Approximates the HTML cubic-bezier(0.16,1,0.3,1)
     * with a rationally cheap smoothstep-like curve.
     * p = [0,65535].
     */
    uint64_t t = p;
    uint64_t t2 = (t * t) >> 16;
    uint64_t inv = 65535u - t;
    uint64_t inv2 = (inv * inv) >> 16;
    uint64_t curve = 65535u - ((inv2 * inv) >> 16);
    (void)t2;
    return (uint32_t)curve;
}

void window_tick(WindowManager *wm, Window *window, uint32_t delta_ms) {
    if (!wm || !window || !window->animation_active)
        return;

    const Rect old = window_get_bounds(window);

    uint64_t elapsed = (uint64_t)window->animation_elapsed_ms + delta_ms;
    if (elapsed > window->animation_ms)
        elapsed = window->animation_ms;

    window->animation_elapsed_ms = (uint32_t)elapsed;

    uint32_t p = (uint32_t)(
        ((uint64_t)window->animation_elapsed_ms * 65535u) /
        window->animation_ms
    );
    uint32_t curve = smooth_back_out(p);

    const int32_t top_delta =
        window->animation_to_top - window->animation_from_top;
    const int32_t height_delta =
        window->animation_to_height - window->animation_from_height;

    window->y =
        window->animation_from_top +
        (int32_t)(((int64_t)top_delta * curve) >> 16);

    const int32_t new_height =
        window->animation_from_height +
        (int32_t)(((int64_t)height_delta * curve) >> 16);

    if (new_height != window->height) {
        window->height = clamp_i32(
            new_height,
            (int32_t)window->collapsed_height,
            INT_MAX
        );
        resize_backbuffer(window, window->width, window->height);
    }

    window_recalculate_layout(window);
    window_mark_dirty(wm, window, NULL);

    if (window->animation_elapsed_ms >= window->animation_ms) {
        window->animation_active = 0;
        window->flags &= ~WINDOW_FLAG_ANIMATING;

        if (window->close_latched) {
            window->state = WINDOW_STATE_CLOSED;
            window->flags &= ~WINDOW_FLAG_VISIBLE;
            notify_taskbar_internal(wm, window);

            /* Removal is deferred until compositor acknowledges no more refs.
               In a real OS, use refcount/RCU rather than freeing in IRQ context. */
        }
    }

    mark_damage_internal(wm, window, &old);
    Rect now = window_get_bounds(window);
    mark_damage_internal(wm, window, &now);
}

void window_fill(Window *window, uint32_t rgba) {
    if (!window || !window->backbuffer.pixels)
        return;

    const size_t pixels =
        (size_t)window->backbuffer.width *
        (size_t)window->backbuffer.height;

    if (pixels > UINT32_MAX)
        return;

    asm_fast_clear_buffer(
        window->backbuffer.pixels,
        (uint32_t)pixels,
        rgba
    );

    window->clear_color = rgba;
    window->flags |= WINDOW_FLAG_DIRTY;
}

void window_clear(WindowManager *wm, Window *window) {
    if (!window) return;
    window_fill(window, window->clear_color);
    window_mark_dirty(wm, window, NULL);
}

Rect window_get_bounds(const Window *window) {
    if (!window)
        return (Rect){0,0,0,0};

    return (Rect){
        window->x,
        window->y,
        window->width,
        window->height
    };
}

WindowHit window_hit_test(
    const Window *window,
    int32_t mouse_x,
    int32_t mouse_y
) {
    if (!window || !(window->flags & WINDOW_FLAG_VISIBLE))
        return WINDOW_HIT_NONE;

    const int32_t lx = mouse_x - window->x;
    const int32_t ly = mouse_y - window->y;

    if (rect_contains(window->close_button, lx, ly))
        return WINDOW_HIT_CLOSE;

    if (rect_contains(window->minimize_button, lx, ly))
        return WINDOW_HIT_MINIMIZE;

    if (rect_contains(window->maximize_button, lx, ly))
        return WINDOW_HIT_MAXIMIZE;

    if (rect_contains(window->titlebar, lx, ly))
        return WINDOW_HIT_TITLEBAR;

    if (rect_contains(window->left_border, lx, ly))
        return WINDOW_HIT_LEFT_BORDER;

    if (rect_contains(window->right_border, lx, ly))
        return WINDOW_HIT_RIGHT_BORDER;

    if (mouse_x < window->x ||
        mouse_y < window->y ||
        mouse_x >= window->x + window->width ||
        mouse_y >= window->y + window->height)
        return WINDOW_HIT_NONE;

    return WINDOW_HIT_CLIENT;
}

Window *window_manager_pick_window(
    WindowManager *wm,
    int32_t mouse_x,
    int32_t mouse_y
) {
    if (!wm) return NULL;

    Window *picked = NULL;

    for (Window *it = wm->head; it; it = it->next) {
        if (!(it->flags & WINDOW_FLAG_VISIBLE))
            continue;

        if (window_hit_test(it, mouse_x, mouse_y) != WINDOW_HIT_NONE)
            picked = it;
    }

    if (picked)
        raise_window(wm, picked);

    return picked;
}

int window_begin_drag(
    Window *window,
    int32_t mouse_x,
    int32_t mouse_y
) {
    if (!window ||
        !(window->flags & WINDOW_FLAG_DRAGGABLE) ||
        window->state == WINDOW_STATE_MAXIMIZED ||
        window->state == WINDOW_STATE_CLOSED)
        return 0;

    if (window_hit_test(window, mouse_x, mouse_y) != WINDOW_HIT_TITLEBAR)
        return 0;

    window->dragging = 1;
    window->drag_start_mouse_x = mouse_x;
    window->drag_start_mouse_y = mouse_y;
    window->drag_start_window_x = window->x;
    window->drag_start_window_y = window->y;
    return 1;
}

void window_drag_to(
    WindowManager *wm,
    Window *window,
    int32_t mouse_x,
    int32_t mouse_y
) {
    if (!wm || !window || !window->dragging)
        return;

    const Rect old = window_get_bounds(window);

    int32_t new_x =
        window->drag_start_window_x +
        (mouse_x - window->drag_start_mouse_x);

    int32_t new_y =
        window->drag_start_window_y +
        (mouse_y - window->drag_start_mouse_y);

    /* Matches HTML: top is clamped to >= 0, horizontal is unrestricted. */
    if (new_y < 0)
        new_y = 0;

    window->x = new_x;
    window->y = new_y;

    window_recalculate_layout(window);
    window_mark_dirty(wm, window, NULL);

    mark_damage_internal(wm, window, &old);
    Rect now = window_get_bounds(window);
    mark_damage_internal(wm, window, &now);
}

void window_end_drag(Window *window) {
    if (!window) return;
    window->dragging = 0;
}

void window_dispatch_mouse_click(
    WindowManager *wm,
    int32_t mouse_x,
    int32_t mouse_y,
    uint8_t button
) {
    if (!wm) return;

    window_manager_wake(wm);

    Window *window = window_manager_pick_window(wm, mouse_x, mouse_y);
    if (!window) return;

    WindowHit hit = window_hit_test(window, mouse_x, mouse_y);

    switch (hit) {
        case WINDOW_HIT_CLOSE:
            window_close(wm, window);
            break;

        case WINDOW_HIT_MINIMIZE:
            window_minimize(wm, window);
            break;

        case WINDOW_HIT_MAXIMIZE:
            if (window->state == WINDOW_STATE_MAXIMIZED)
                window_restore(wm, window);
            else
                window_maximize(wm, window);
            break;

        case WINDOW_HIT_TITLEBAR:
            window_begin_drag(window, mouse_x, mouse_y);
            break;

        case WINDOW_HIT_CLIENT: {
            const int32_t local_x = mouse_x - window->x;
            const int32_t local_y = mouse_y - window->y;
            if (window->callbacks.mouse_click)
                window->callbacks.mouse_click(
                    window, local_x, local_y, button
                );
            break;
        }

        default:
            break;
    }
}

void window_dispatch_mouse_move(
    WindowManager *wm,
    int32_t mouse_x,
    int32_t mouse_y
) {
    if (!wm) return;

    window_manager_wake(wm);

    Window *window = wm->focused;

    if (window && window->dragging) {
        window_drag_to(wm, window, mouse_x, mouse_y);
        return;
    }

    window = window_manager_pick_window(wm, mouse_x, mouse_y);
    if (!window) return;

    if (window->callbacks.mouse_move) {
        window->callbacks.mouse_move(
            window,
            mouse_x - window->x,
            mouse_y - window->y
        );
    }
}

void window_dispatch_key_down(
    WindowManager *wm,
    uint16_t scancode,
    uint8_t modifiers
)
{
    if (!wm) return;

    window_manager_wake(wm);
    if (wm->focused && wm->focused->callbacks.key_down)
        wm->focused->callbacks.key_down(
            wm->focused, scancode, modifiers
        );
}
