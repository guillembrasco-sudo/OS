#ifndef USER_INPUT_SERVER_H
#define USER_INPUT_SERVER_H

#include <user/display/protocol.h>

int input_server_init(struct display_channel *channel);
int input_server_poll(void);
int input_server_inject(const struct display_input_event *event);

#endif