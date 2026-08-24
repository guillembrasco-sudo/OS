#include <user/display/compositor.h>

struct compositor_surface {
    uint32_t id;
    uint32_t buffer_handle;
    uint32_t width;
    uint32_t height;
    uint32_t visible;
    uint32_t z_order;
    int32_t x;
    int32_t y;
    uint32_t committed;
    uint64_t serial;
    struct display_rect damage[DISPLAY_MAX_DAMAGE];
    uint32_t damage_count;
};

static struct display_channel *client_channel;
static struct display_channel *client_output;
static struct display_channel *input_channel;
static struct compositor_surface surfaces[DISPLAY_MAX_SURFACES];
static uint32_t surface_count;
static uint32_t focused_surface;
static uint32_t kms_open;

static struct compositor_surface *find_surface(uint32_t id)
{
    for (uint32_t index = 0; index < surface_count; ++index)
        if (surfaces[index].id == id)
            return &surfaces[index];
    return 0;
}

static struct compositor_surface *create_surface(uint32_t id)
{
    if (surface_count == DISPLAY_MAX_SURFACES)
        return 0;
    surfaces[surface_count].id = id;
    surfaces[surface_count].visible = 1;
    surfaces[surface_count].z_order = surface_count;
    return &surfaces[surface_count++];
}

static void mark_full_damage(struct compositor_surface *surface)
{
    surface->damage[0].x = 0;
    surface->damage[0].y = 0;
    surface->damage[0].width = surface->width;
    surface->damage[0].height = surface->height;
    surface->damage_count = 1;
}

static int process_client_message(const struct display_message *message)
{
    struct compositor_surface *surface;
    if (message->type == DISPLAY_SURFACE_ATTACH) {
        surface = find_surface(message->body.attach.surface_id);
        if (surface == 0)
            surface = create_surface(message->body.attach.surface_id);
        if (surface == 0)
            return -1;
        surface->buffer_handle = message->body.attach.buffer_handle;
        surface->width = message->body.attach.width;
        surface->height = message->body.attach.height;
        mark_full_damage(surface);
    } else if (message->type == DISPLAY_SURFACE_DAMAGE) {
        surface = find_surface(message->body.damage.surface_id);
        if (surface != 0 && surface->damage_count < DISPLAY_MAX_DAMAGE)
            surface->damage[surface->damage_count++] = message->body.damage.rectangle;
    } else if (message->type == DISPLAY_SURFACE_COMMIT) {
        surface = find_surface(message->body.commit.surface_id);
        if (surface != 0) {
            surface->committed = 1;
            surface->serial = message->body.commit.serial;
            focused_surface = surface->id;
        }
    }
    return 0;
}

static int compose_damage(void)
{
    uint32_t damage_count = 0;
    for (uint32_t index = 0; index < surface_count; ++index) {
        struct compositor_surface *surface = &surfaces[index];
        if (surface->visible != 0 && surface->committed != 0) {
            damage_count += surface->damage_count;
            struct display_message release = {
                DISPLAY_SURFACE_RELEASE,
                sizeof(struct surface_release),
                { .release = { surface->id, surface->buffer_handle,
                               surface->serial } }
            };
            display_channel_send(client_output, &release);
        }
    }
    if (damage_count == 0)
        return 0;

    /* The DRM/KMS client performs one atomic commit for the accumulated damage. */
    for (uint32_t index = 0; index < surface_count; ++index) {
        surfaces[index].damage_count = 0;
        surfaces[index].committed = 0;
    }
    return 0;
}

int compositor_init(struct display_channel *new_client_channel,
                    struct display_channel *new_client_output,
                    struct display_channel *new_input_channel)
{
    if (new_client_channel == 0 || new_client_output == 0 ||
        new_input_channel == 0)
        return -1;
    client_channel = new_client_channel;
    client_output = new_client_output;
    input_channel = new_input_channel;
    surface_count = 0;
    focused_surface = 0;
    kms_open = 1;
    return 0;
}

int compositor_run_once(void)
{
    struct display_message message;
    int processed = 0;
    if (kms_open == 0)
        return -1;
    while (display_channel_receive(client_channel, &message) == 0) {
        process_client_message(&message);
        ++processed;
    }
    while (display_channel_receive(input_channel, &message) == 0) {
        if (focused_surface != 0) {
            message.body.input.target_surface = focused_surface;
            display_channel_send(client_output, &message);
        }
        ++processed;
    }
    compose_damage();
    return processed;
}

void compositor_run(void)
{
    for (;;)
        compositor_run_once();
}