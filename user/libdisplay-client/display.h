#ifndef LIBDISPLAY_CLIENT_H
#define LIBDISPLAY_CLIENT_H

#include <stdint.h>
#include <user/display/protocol.h>

struct display_client {
    struct display_channel *to_compositor;
    struct display_channel *from_compositor;
    uint32_t surface_id;
    uint32_t buffer_handle;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t *pixels;
};

int display_client_open(struct display_client *client,
                        const char *render_node,
                        struct display_channel *to_compositor,
                        struct display_channel *from_compositor);
uint32_t *display_client_map(struct display_client *client);
int display_client_present(struct display_client *client,
                           const struct display_rect *damage);
int display_client_next_event(struct display_client *client,
                              struct display_message *event);

#endif