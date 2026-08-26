#include <drivers/irqdomain.h>
#include <drivers/mouse.h>
#include <arch/x86_64/io.h>

#define PS2_STATUS 0x64
#define PS2_DATA 0x60
#define PS2_STATUS_OUTPUT 0x01

struct mouse_state {
	WindowManager *window_manager;
	int32_t x;
	int32_t y;
	uint8_t packet[3];
	uint8_t packet_index;
};

static struct mouse_state state;

int mouse_init(WindowManager *window_manager)
{
	if (!window_manager)
		return -1;
	state.window_manager = window_manager;
	state.x = 0;
	state.y = 0;
	state.packet_index = 0;
	return irq_register_handler(12, mouse_irq_handler, &state);
}

void mouse_irq_handler(uint8_t irq, void *context)
{
	struct mouse_state *mouse = (struct mouse_state *)context;
	uint8_t value;
	int32_t dx;
	int32_t dy;
	(void)irq;
	if (!(io_in8(PS2_STATUS) & PS2_STATUS_OUTPUT))
		return;
	value = io_in8(PS2_DATA);
	if (mouse->packet_index == 0 && !(value & 0x08))
		return;
	mouse->packet[mouse->packet_index++] = value;
	if (mouse->packet_index != 3)
		return;
	mouse->packet_index = 0;
	dx = (int8_t)mouse->packet[1];
	dy = -(int8_t)mouse->packet[2];
	mouse->x += dx;
	mouse->y += dy;
	window_dispatch_mouse_move(mouse->window_manager, mouse->x, mouse->y);
	if (mouse->packet[0] & 0x01)
		window_dispatch_mouse_click(mouse->window_manager,
									mouse->x, mouse->y, 1);
}
