#include <drivers/irqdomain.h>
#include <drivers/keyboard.h>
#include <arch/x86_64/io.h>

#define PS2_STATUS 0x64
#define PS2_DATA 0x60
#define PS2_STATUS_OUTPUT 0x01
#define PS2_STATUS_INPUT 0x02

static int ps2_wait_input(void)
{
	for (uint32_t attempt = 0; attempt < 100000; ++attempt)
		if (!(io_in8(PS2_STATUS) & PS2_STATUS_INPUT))
			return 0;
	return -1;
}

int keyboard_init(WindowManager *window_manager)
{
	if (!window_manager || ps2_wait_input() != 0)
		return -1;
	io_out8(PS2_STATUS, 0xAE);
	return irq_register_handler(1, keyboard_irq_handler, window_manager);
}

void keyboard_irq_handler(uint8_t irq, void *context)
{
	static uint8_t modifiers;
	uint8_t scancode;
	(void)irq;
	if (!(io_in8(PS2_STATUS) & PS2_STATUS_OUTPUT))
		return;
	scancode = io_in8(PS2_DATA);
	if (scancode == 0x2a || scancode == 0x36) {
		modifiers |= 1;
		return;
	}
	if (scancode == 0xaa || scancode == 0xb6) {
		modifiers &= (uint8_t)~1u;
		return;
	}
	if (scancode & 0x80)
		return;
	window_dispatch_key_down((WindowManager *)context, scancode, modifiers);
}
