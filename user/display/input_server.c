#include <user/display/input_server.h>

static struct display_channel *input_channel;

int input_server_init(struct display_channel *channel)
{
    input_channel = channel;
    return channel == 0 ? -1 : 0;
}

int input_server_inject(const struct display_input_event *event)
{
    struct display_message message;
    if (input_channel == 0 || event == 0)
        return -1;
    message.type = DISPLAY_INPUT_EVENT;
    message.size = sizeof(message.body.input);
    message.body.input = *event;
    return display_channel_send(input_channel, &message);
}

int input_server_poll(void)
{
    /* Hardware evdev reads are connected here once the input driver is present. */
    return 0;
}