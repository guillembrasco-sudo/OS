#ifndef USER_COMPOSITOR_H
#define USER_COMPOSITOR_H

#include <user/display/protocol.h>

int compositor_init(struct display_channel *client_channel,
                    struct display_channel *client_output,
                    struct display_channel *input_channel);
int compositor_run_once(void);
void compositor_run(void);

#endif