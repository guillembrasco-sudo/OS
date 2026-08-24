#include <user/display/protocol.h>

static uint32_t next_index(uint32_t index)
{
    return (index + 1u) % DISPLAY_MESSAGE_QUEUE;
}

int display_channel_send(struct display_channel *channel,
                         const struct display_message *message)
{
    uint32_t head;
    uint32_t next;
    if (channel == 0 || message == 0)
        return -1;
    head = channel->head;
    next = next_index(head);
    if (next == channel->tail)
        return -1;
    channel->messages[head] = *message;
    __asm__ volatile ("" ::: "memory");
    channel->head = next;
    return 0;
}

int display_channel_receive(struct display_channel *channel,
                            struct display_message *message)
{
    uint32_t tail;
    if (channel == 0 || message == 0)
        return -1;
    tail = channel->tail;
    if (tail == channel->head)
        return -1;
    __asm__ volatile ("" ::: "memory");
    *message = channel->messages[tail];
    channel->tail = next_index(tail);
    return 0;
}