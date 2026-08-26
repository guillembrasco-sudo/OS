#include <drivers/irqdomain.h>
#include <drivers/mouse.h>
#include <arch/x86_64/io.h>
#include <hal/display.h>

#define PS2_STATUS 0x64
#define PS2_DATA 0x60
#define PS2_STATUS_OUTPUT 0x01
#define PS2_CONFIG_IRQ12 0x02

struct mouse_state {
	WindowManager *window_manager;
	int32_t x;
	int32_t y;
	uint8_t packet[3];
	uint8_t packet_index;
	uint8_t buttons;
};

static struct mouse_state state;

static int ps2_wait_input(void)
{
	for (uint32_t attempt = 0; attempt < 100000; attempt++)
		if (!(io_in8(PS2_STATUS) & 0x02))
			return 0;
	return -1;
}

static int ps2_wait_output(void)
{
	for (uint32_t attempt = 0; attempt < 100000; attempt++)
		if (io_in8(PS2_STATUS) & PS2_STATUS_OUTPUT)
			return 0;
	return -1;
}

static int mouse_enable(void)
{
	uint8_t config;

	if (ps2_wait_input() != 0)
		return -1;
	io_out8(PS2_STATUS, 0xA8);
	if (ps2_wait_input() != 0)
		return -1;
	io_out8(PS2_STATUS, 0x20);
	if (ps2_wait_output() != 0)
		return -1;
	config = io_in8(PS2_DATA);
	config |= PS2_CONFIG_IRQ12;
	if (ps2_wait_input() != 0)
		return -1;
	io_out8(PS2_STATUS, 0x60);
	if (ps2_wait_input() != 0)
		return -1;
	io_out8(PS2_DATA, config);
	if (ps2_wait_input() != 0)
		return -1;
	io_out8(PS2_STATUS, 0xD4);
	if (ps2_wait_input() != 0)
		return -1;
	io_out8(PS2_DATA, 0xF4);
	if (ps2_wait_output() != 0)
		return -1;
	return io_in8(PS2_DATA) == 0xFA ? 0 : -1;
}

int mouse_init(WindowManager *window_manager)
{
	if (!window_manager)
		return -1;
	state.window_manager = window_manager;
	state.x = (int32_t)(window_manager->display.width / 2);
	state.y = (int32_t)(window_manager->display.height / 2);
	state.packet_index = 0;
	state.buttons = 0;
	if (mouse_enable() != 0)
		return -1;
	display_framebuffer_draw_cursor(state.x, state.y);
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
	if (mouse->x < 0) mouse->x = 0;
	if (mouse->y < 0) mouse->y = 0;
	if (mouse->x >= (int32_t)mouse->window_manager->display.width)
		mouse->x = (int32_t)mouse->window_manager->display.width - 1;
	if (mouse->y >= (int32_t)mouse->window_manager->display.height)
		mouse->y = (int32_t)mouse->window_manager->display.height - 1;
	window_dispatch_mouse_move(mouse->window_manager, mouse->x, mouse->y);
	display_framebuffer_draw_cursor(mouse->x, mouse->y);
	if ((mouse->packet[0] & 0x01) && !(mouse->buttons & 0x01))
		window_dispatch_mouse_click(mouse->window_manager,
									mouse->x, mouse->y, 1);
	if (!(mouse->packet[0] & 0x01) && (mouse->buttons & 0x01))
		window_dispatch_mouse_click(mouse->window_manager,
									mouse->x, mouse->y, 0);
	mouse->buttons = mouse->packet[0] & 0x07;
	window_manager_present(mouse->window_manager);
}
