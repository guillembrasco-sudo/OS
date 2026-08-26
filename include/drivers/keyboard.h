#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

#include <kernel/window_system.h>

int keyboard_init(WindowManager *window_manager);
void keyboard_irq_handler(uint8_t irq, void *context);

#endif
