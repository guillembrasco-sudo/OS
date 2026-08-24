#include <user/libdisplay-client/display.h>

#define CLIENT_WIDTH 1024
#define CLIENT_HEIGHT 768
static uint32_t client_pixels[CLIENT_WIDTH * CLIENT_HEIGHT];
static uint32_t next_surface = 1;

static int string_equal(const char *left, const char *right)
{
    while (*left != 0 && *left == *right) {
        ++left;
        ++right;
    }
    return *left == 0 && *right == 0;
}

int display_client_open(struct display_client *client,
                        const char *render_node,
                        struct display_channel *to_compositor,
                        struct display_channel *from_compositor)
{
    if (client == 0 || !string_equal(render_node, "/dev/dri/renderD128") ||
        to_compositor == 0 || from_compositor == 0)
        return -1;
    client->to_compositor = to_compositor;
    client->from_compositor = from_compositor;
    client->surface_id = next_surface++;
    client->buffer_handle = client->surface_id;
    client->width = CLIENT_WIDTH;
    client->height = CLIENT_HEIGHT;
    client->pitch = CLIENT_WIDTH * sizeof(uint32_t);
    client->pixels = client_pixels;
    return 0;
}

uint32_t *display_client_map(struct display_client *client)
{
    return client == 0 ? 0 : client->pixels;
}

int display_client_present(struct display_client *client,
                           const struct display_rect *damage)
{
    struct display_message message;
    if (client == 0 || damage == 0)
        return -1;
    message.type = DISPLAY_SURFACE_ATTACH;
    message.size = sizeof(message.body.attach);
    message.body.attach = (struct surface_attach){
        client->surface_id, client->buffer_handle, client->width,
        client->height, 2, client->pitch, 0
    };
    if (display_channel_send(client->to_compositor, &message) != 0)
        return -1;
    message.type = DISPLAY_SURFACE_DAMAGE;
    message.size = sizeof(message.body.damage);
    message.body.damage = (struct surface_damage){ client->surface_id, *damage };
    if (display_channel_send(client->to_compositor, &message) != 0)
        return -1;
    message.type = DISPLAY_SURFACE_COMMIT;
    message.size = sizeof(message.body.commit);
    message.body.commit = (struct surface_commit){ client->surface_id, 1, 1 };
    return display_channel_send(client->to_compositor, &message);
}

int display_client_next_event(struct display_client *client,
                              struct display_message *event)
{
    if (client == 0 || event == 0)
        return -1;
    return display_channel_receive(client->from_compositor, event);
}