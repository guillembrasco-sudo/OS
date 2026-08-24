#ifndef USER_DISPLAY_PROTOCOL_H
#define USER_DISPLAY_PROTOCOL_H

#include <stdint.h>

#define DISPLAY_PROTOCOL_VERSION 1
#define DISPLAY_MAX_DAMAGE 8
#define DISPLAY_MAX_SURFACES 32
#define DISPLAY_MESSAGE_QUEUE 64

enum display_message_type {
    DISPLAY_SURFACE_ATTACH = 1,
    DISPLAY_SURFACE_DAMAGE,
    DISPLAY_SURFACE_COMMIT,
    DISPLAY_SURFACE_RELEASE,
    DISPLAY_INPUT_EVENT
};

struct display_rect {
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
};

struct surface_attach {
    uint32_t surface_id;
    uint32_t buffer_handle;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t pitch;
    uint64_t acquire_fence;
};

struct surface_damage {
    uint32_t surface_id;
    struct display_rect rectangle;
};

struct surface_commit {
    uint32_t surface_id;
    uint32_t damage_count;
    uint64_t serial;
};

struct surface_release {
    uint32_t surface_id;
    uint32_t buffer_handle;
    uint64_t release_fence;
};

struct display_input_event {
    uint32_t type;
    uint32_t target_surface;
    int32_t x;
    int32_t y;
    uint32_t code;
    uint32_t state;
};

struct display_message {
    uint32_t type;
    uint32_t size;
    union {
        struct surface_attach attach;
        struct surface_damage damage;
        struct surface_commit commit;
        struct surface_release release;
        struct display_input_event input;
    } body;
};

struct display_channel {
    volatile uint32_t head;
    volatile uint32_t tail;
    struct display_message messages[DISPLAY_MESSAGE_QUEUE];
};

int display_channel_send(struct display_channel *channel,
                         const struct display_message *message);
int display_channel_receive(struct display_channel *channel,
                            struct display_message *message);

#endif