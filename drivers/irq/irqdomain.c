#include <drivers/irqdomain.h>

struct irq_slot {
	irq_handler_fn handler;
	void *context;
};

static struct irq_slot irq_slots[256];

int irq_register_handler(uint8_t irq, irq_handler_fn handler, void *context)
{
	if (!handler || irq_slots[irq].handler)
		return -1;
	irq_slots[irq].handler = handler;
	irq_slots[irq].context = context;
	return 0;
}

void irq_unregister_handler(uint8_t irq)
{
	irq_slots[irq].handler = 0;
	irq_slots[irq].context = 0;
}

void irq_dispatch(uint8_t irq)
{
	if (irq_slots[irq].handler)
		irq_slots[irq].handler(irq, irq_slots[irq].context);
}
