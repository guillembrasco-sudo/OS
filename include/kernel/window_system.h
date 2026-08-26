#ifndef KERNEL_WINDOW_SYSTEM_H
#define KERNEL_WINDOW_SYSTEM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Geometry ---------- */

typedef struct Rect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} Rect;

typedef struct Point {
    int32_t x;
    int32_t y;
} Point;

/* ---------- Window state ---------- */

typedef enum WindowState {
    WINDOW_STATE_NORMAL = 0,
    WINDOW_STATE_MINIMIZED,
    WINDOW_STATE_MAXIMIZED,
    WINDOW_STATE_CLOSED
} WindowState;

/* ---------- Window flags ---------- */

enum WindowFlags {
    WINDOW_FLAG_VISIBLE       = 1u << 0,
    WINDOW_FLAG_FOCUSED      = 1u << 1,
    WINDOW_FLAG_HAS_BORDER   = 1u << 2,
    WINDOW_FLAG_HAS_CLOSE    = 1u << 3,
    WINDOW_FLAG_HAS_MINIMIZE = 1u << 4,
    WINDOW_FLAG_HAS_MAXIMIZE = 1u << 5,
    WINDOW_FLAG_RESIZABLE    = 1u << 6,
    WINDOW_FLAG_DRAGGABLE    = 1u << 7,
    WINDOW_FLAG_DIRTY        = 1u << 8,
    WINDOW_FLAG_ANIMATING    = 1u << 9,
    WINDOW_FLAG_ACTIVE       = 1u << 10
};

typedef enum WindowHit {
    WINDOW_HIT_NONE = 0,
    WINDOW_HIT_CLIENT,
    WINDOW_HIT_TITLEBAR,
    WINDOW_HIT_CLOSE,
    WINDOW_HIT_MINIMIZE,
    WINDOW_HIT_MAXIMIZE,
    WINDOW_HIT_LEFT_BORDER,
    WINDOW_HIT_RIGHT_BORDER,
    WINDOW_HIT_TOP_BORDER,
    WINDOW_HIT_BOTTOM_BORDER
} WindowHit;

/* ---------- RGBA32 backbuffer ---------- */

typedef struct PixelBuffer {
    uint32_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stride_pixels;
    size_t bytes;
} PixelBuffer;

/* ---------- OS/Compositor abstraction ---------- */

typedef void *(*WindowAllocFn)(size_t size, void *ctx);
typedef void (*WindowFreeFn)(void *ptr, void *ctx);

typedef struct WindowAllocator {
    WindowAllocFn alloc;
    WindowFreeFn free;
    void *ctx;
} WindowAllocator;

typedef struct DisplayInfo {
    uint32_t width;
    uint32_t height;
} DisplayInfo;

typedef struct DamageRegion {
    Rect rect;
    uint8_t valid;
} DamageRegion;

typedef struct Window Window;

typedef void (*WindowMouseClickFn)(Window *window, int32_t x, int32_t y, uint8_t button);
typedef void (*WindowMouseMoveFn)(Window *window, int32_t x, int32_t y);
typedef void (*WindowKeyDownFn)(Window *window, uint16_t scancode, uint8_t modifiers);
typedef void (*WindowStandbyFn)(uint8_t active, void *ctx);

typedef struct WindowCallbacks {
    WindowMouseClickFn mouse_click;
    WindowMouseMoveFn mouse_move;
    WindowKeyDownFn key_down;
} WindowCallbacks;

/* ---------- Native window object ---------- */

struct Window {
    /* Identity / topology */
    uint64_t id;
    Window *prev;
    Window *next;
    uint32_t z_index;

    /* Geometry */
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;

    /* Exact restore geometry before maximize */
    int32_t saved_x;
    int32_t saved_y;
    int32_t saved_width;
    int32_t saved_height;

    /* Geometry constraints */
    int32_t min_width;
    int32_t min_height;

    /* Rendering */
    PixelBuffer backbuffer;
    Rect clip;
    uint32_t clear_color;

    /* Window chrome */
    char title[128];
    uint32_t title_length;
    uint32_t titlebar_height;
    uint32_t footer_height;
    uint32_t border_width;
    uint32_t collapsed_height;
    uint32_t content_padding;

    Rect titlebar;
    Rect title_rect;
    Rect footer;
    Rect client;
    Rect left_border;
    Rect right_border;

    /* Hitboxes */
    Rect close_button;
    Rect minimize_button;
    Rect maximize_button;

    /* State */
    WindowState state;
    uint32_t flags;
    uint8_t close_latched;

    /* Drag state */
    uint8_t dragging;
    int32_t drag_start_mouse_x;
    int32_t drag_start_mouse_y;
    int32_t drag_start_window_x;
    int32_t drag_start_window_y;

    /* Animation state: fixed-point normalized progress 0..1 */
    uint8_t animation_active;
    uint32_t animation_ms;
    uint32_t animation_elapsed_ms;
    int32_t animation_from_top;
    int32_t animation_to_top;
    int32_t animation_from_height;
    int32_t animation_to_height;

    /* Callbacks */
    WindowCallbacks callbacks;

    /* OS integration */
    WindowAllocator allocator;
    DisplayInfo display;
    DamageRegion damage;
    char input_text[64];
    uint32_t input_length;
    char status_text[64];
    void *owner;
};

/* ---------- Global window manager ---------- */

typedef struct WindowManager {
    Window *head;
    Window *tail;
    Window *focused;
    uint64_t next_id;
    DisplayInfo display;
    WindowAllocator allocator;
    void (*mark_damage)(Window *window, const Rect *rect, void *ctx);
    void *damage_ctx;
    void (*notify_taskbar)(Window *window, WindowState state, void *ctx);
    void *taskbar_ctx;
    WindowStandbyFn standby_callback;
    void *standby_ctx;
    Window *master_console;
    uint8_t standby;
} WindowManager;

/* ---------- Manager ---------- */

void window_manager_init(
    WindowManager *wm,
    uint32_t display_width,
    uint32_t display_height,
    WindowAllocator allocator
);

void window_manager_set_damage_callback(
    WindowManager *wm,
    void (*mark_damage)(Window *window, const Rect *rect, void *ctx),
    void *ctx
);

void window_manager_set_taskbar_callback(
    WindowManager *wm,
    void (*notify_taskbar)(Window *window, WindowState state, void *ctx),
    void *ctx
);

/* Requires kheap_init() to have completed before the first window is created. */
void window_manager_init_kernel(
    WindowManager *wm,
    uint32_t display_width,
    uint32_t display_height
);
void window_manager_present(WindowManager *wm);
void window_master_console_render(Window *window);

void window_manager_set_standby_callback(
    WindowManager *wm,
    WindowStandbyFn callback,
    void *ctx
);

void window_manager_enter_standby(WindowManager *wm);
void window_manager_wake(WindowManager *wm);
int window_manager_is_standby(const WindowManager *wm);

/* ---------- Lifecycle ---------- */

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
);

void window_destroy(WindowManager *wm, Window *window);

int window_minimize(WindowManager *wm, Window *window);
int window_maximize(WindowManager *wm, Window *window);
int window_restore(WindowManager *wm, Window *window);
int window_close(WindowManager *wm, Window *window);

/* ---------- Layout / rendering ---------- */

void window_recalculate_layout(Window *window);
void window_mark_dirty(WindowManager *wm, Window *window, const Rect *local_rect);
void window_begin_animation(
    Window *window,
    int32_t from_top,
    int32_t to_top,
    int32_t from_height,
    int32_t to_height,
    uint32_t duration_ms
);
void window_tick(WindowManager *wm, Window *window, uint32_t delta_ms);

void window_fill(Window *window, uint32_t rgba);
void window_clear(WindowManager *wm, Window *window);

/* Equivalent of HTML getBoundingClientRect(). */
Rect window_get_bounds(const Window *window);

/* ---------- Input ---------- */

WindowHit window_hit_test(
    const Window *window,
    int32_t mouse_x,
    int32_t mouse_y
);

Window *window_manager_pick_window(
    WindowManager *wm,
    int32_t mouse_x,
    int32_t mouse_y
);

int window_begin_drag(
    Window *window,
    int32_t mouse_x,
    int32_t mouse_y
);

void window_drag_to(
    WindowManager *wm,
    Window *window,
    int32_t mouse_x,
    int32_t mouse_y
);

void window_end_drag(Window *window);

void window_dispatch_mouse_click(
    WindowManager *wm,
    int32_t mouse_x,
    int32_t mouse_y,
    uint8_t button
);

void window_dispatch_mouse_move(
    WindowManager *wm,
    int32_t mouse_x,
    int32_t mouse_y
);

void window_dispatch_key_down(
    WindowManager *wm,
    uint16_t scancode,
    uint8_t modifiers
);

/* Executes one command entered in the master console. */
int window_master_console_execute(WindowManager *wm, const char *command);

Window *window_default_open(WindowManager *wm, const char *title,
                            int32_t x, int32_t y, uint32_t color);
void window_set_status(Window *window, const char *text);
int window_clock_toggle(WindowManager *wm);
int window_file_explorer_open(WindowManager *wm);
int window_device_status_open(WindowManager *wm);
int window_web_open(WindowManager *wm);

/* Kernel-side bridge used by the console syscall. */
int window_manager_kernel_execute_command(const char *command);

/* ---------- Assembly primitives ---------- */

void asm_fast_clear_buffer(
    void *buffer,
    uint32_t pixel_count,
    uint32_t color
);

/*
 * Returns 1 when (px,py) lies inside rect = {x,y,width,height},
 * using half-open bounds [x,x+width) x [y,y+height).
 */
int asm_window_hit_test(
    int32_t px,
    int32_t py,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height
);

#ifdef __cplusplus
}
#endif

#endif /* KERNEL_WINDOW_SYSTEM_H */
