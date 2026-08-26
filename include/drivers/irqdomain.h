#ifndef DRIVERS_IRQDOMAIN_H
#define DRIVERS_IRQDOMAIN_H

#include <stdint.h>

typedef void (*irq_handler_fn)(uint8_t irq, void *context);

int irq_register_handler(uint8_t irq, irq_handler_fn handler, void *context);
void irq_unregister_handler(uint8_t irq);
void irq_dispatch(uint8_t irq);

#endif
