#ifndef DRIVERS_MOUSE_H
#define DRIVERS_MOUSE_H

#include <kernel/window_system.h>

int mouse_init(WindowManager *window_manager);
void mouse_irq_handler(uint8_t irq, void *context);

#endif
