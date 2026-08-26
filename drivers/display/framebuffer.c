#include <stdint.h>
#include <hal/display.h>
#include <arch/paging.h>
#include <lib/printf.h>

#define MULTIBOOT_INFO_FRAMEBUFFER (1u << 12)
#define MULTIBOOT_FRAMEBUFFER_RGB  1u
#define PAGE_SIZE                   0x1000ULL
#define FRAMEBUFFER_VIRTUAL_BASE   0xFFFF900000000000ULL
#define FONT_WIDTH                 8u
#define FONT_HEIGHT                8u

#pragma pack(push, 1)
struct multiboot_info {
	uint32_t flags;
	uint32_t mem_lower;
	uint32_t mem_upper;
	uint32_t boot_device;
	uint32_t cmdline;
	uint32_t mods_count;
	uint32_t mods_addr;
	uint32_t syms[4];
	uint32_t mmap_length;
	uint32_t mmap_addr;
	uint32_t drives_length;
	uint32_t drives_addr;
	uint32_t config_table;
	uint32_t boot_loader_name;
	uint32_t apm_table;
	uint32_t vbe_control_info;
	uint32_t vbe_mode_info;
	uint16_t vbe_mode;
	uint16_t vbe_interface_seg;
	uint16_t vbe_interface_off;
	uint16_t vbe_interface_len;
	uint64_t framebuffer_addr;
	uint32_t framebuffer_pitch;
	uint32_t framebuffer_width;
	uint32_t framebuffer_height;
	uint8_t framebuffer_bpp;
	uint8_t framebuffer_type;
	uint8_t red_position;
	uint8_t red_mask;
	uint8_t green_position;
	uint8_t green_mask;
	uint8_t blue_position;
	uint8_t blue_mask;
};
#pragma pack(pop)

static struct {
	volatile uint8_t *address;
	uint32_t pitch;
	uint32_t width;
	uint32_t height;
	uint8_t red_position;
	uint8_t red_mask;
	uint8_t green_position;
	uint8_t green_mask;
	uint8_t blue_position;
	uint8_t blue_mask;
	uint32_t cursor_x;
	uint32_t cursor_y;
	int ready;
} framebuffer;

static const uint8_t font_digits[10][7] = {
	{0x0e,0x11,0x13,0x15,0x19,0x11,0x0e}, {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e},
	{0x0e,0x11,0x01,0x02,0x04,0x08,0x1f}, {0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e},
	{0x02,0x06,0x0a,0x12,0x1f,0x02,0x02}, {0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e},
	{0x06,0x08,0x10,0x1e,0x11,0x11,0x0e}, {0x1f,0x01,0x02,0x04,0x08,0x08,0x08},
	{0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e}, {0x0e,0x11,0x11,0x0f,0x01,0x02,0x0c}
};

static const uint8_t font_letters[26][7] = {
	{0x0e,0x11,0x11,0x1f,0x11,0x11,0x11}, {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e},
	{0x0e,0x11,0x10,0x10,0x10,0x11,0x0e}, {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e},
	{0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f}, {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10},
	{0x0e,0x11,0x10,0x17,0x11,0x11,0x0e}, {0x11,0x11,0x11,0x1f,0x11,0x11,0x11},
	{0x0e,0x04,0x04,0x04,0x04,0x04,0x0e}, {0x07,0x02,0x02,0x02,0x12,0x12,0x0c},
	{0x11,0x12,0x14,0x18,0x14,0x12,0x11}, {0x10,0x10,0x10,0x10,0x10,0x10,0x1f},
	{0x11,0x1b,0x15,0x15,0x11,0x11,0x11}, {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
	{0x0e,0x11,0x11,0x11,0x11,0x11,0x0e}, {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10},
	{0x0e,0x11,0x11,0x11,0x15,0x12,0x0d}, {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11},
	{0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e}, {0x1f,0x04,0x04,0x04,0x04,0x04,0x04},
	{0x11,0x11,0x11,0x11,0x11,0x11,0x0e}, {0x11,0x11,0x11,0x11,0x11,0x0a,0x04},
	{0x11,0x11,0x11,0x15,0x15,0x1b,0x11}, {0x11,0x0a,0x04,0x04,0x04,0x0a,0x11},
	{0x11,0x11,0x0a,0x04,0x04,0x04,0x04}, {0x1f,0x02,0x04,0x08,0x10,0x10,0x1f}
};

static uint8_t glyph_row(char character, uint32_t row)
{
	if (character >= 'a' && character <= 'z') character = (char)(character - 'a' + 'A');
	if (character >= '0' && character <= '9') return row < 7 ? font_digits[character - '0'][row] : 0;
	if (character >= 'A' && character <= 'Z') return row < 7 ? font_letters[character - 'A'][row] : 0;
	switch (character) {
	case '.': return row == 5 || row == 6 ? 0x0c : 0;
	case ':': return row == 1 || row == 2 || row == 4 || row == 5 ? 0x0c : 0;
	case '-': return row == 3 ? 0x1f : 0;
	case '_': return row == 6 ? 0x1f : 0;
	case '/': return row < 6 ? (uint8_t)(1u << (5 - row)) : 0;
	case '=': return row == 2 || row == 4 ? 0x1f : 0;
	case '+': return row == 3 ? 0x1f : row == 1 || row == 2 || row == 4 || row == 5 ? 0x04 : 0;
	case '(': return row == 0 || row == 6 ? 0x02 : 0x04;
	case ')': return row == 0 || row == 6 ? 0x08 : 0x04;
	case '[': return row == 0 || row == 6 ? 0x0e : 0x08;
	case ']': return row == 0 || row == 6 ? 0x0e : 0x02;
	case ' ': return 0;
	default: return row == 0 || row == 6 ? 0x1f : 0x11;
	}
}

static uint32_t pack_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
	uint32_t pixel = 0;
	if (framebuffer.red_mask != 0)
		pixel |= ((uint32_t)(red >> (8 - framebuffer.red_mask)) << framebuffer.red_position);
	if (framebuffer.green_mask != 0)
		pixel |= ((uint32_t)(green >> (8 - framebuffer.green_mask)) << framebuffer.green_position);
	if (framebuffer.blue_mask != 0)
		pixel |= ((uint32_t)(blue >> (8 - framebuffer.blue_mask)) << framebuffer.blue_position);
	return pixel;
}

static void put_pixel(uint32_t x, uint32_t y, uint32_t pixel)
{
	*(volatile uint32_t *)(framebuffer.address + y * framebuffer.pitch + x * 4) = pixel;
}

static void clear_screen(void)
{
	uint32_t background = pack_rgb(8, 12, 20);
	for (uint32_t y = 0; y < framebuffer.height; y++)
		for (uint32_t x = 0; x < framebuffer.width; x++)
			put_pixel(x, y, background);
}

int display_early_console_init(uint64_t multiboot_info_addr)
{
	struct multiboot_info *info;
	uint64_t length, physical_base, offset, mapped_size;

	if (multiboot_info_addr == 0) return -1;
	info = (struct multiboot_info *)(uintptr_t)multiboot_info_addr;
	if (!(info->flags & MULTIBOOT_INFO_FRAMEBUFFER)) {
		kprintf("[DISPLAY] GRUB no proporciono framebuffer (flags=0x%x)\n", info->flags);
		return -1;
	}
	kprintf("[DISPLAY] framebuffer=0x%lx mode=%ux%u pitch=%u bpp=%u type=%u positions=%u/%u/%u masks=%u/%u/%u\n",
		info->framebuffer_addr, info->framebuffer_width, info->framebuffer_height,
		info->framebuffer_pitch, info->framebuffer_bpp, info->framebuffer_type,
		info->red_position, info->green_position, info->blue_position,
		info->red_mask, info->green_mask, info->blue_mask);
	if (info->framebuffer_type != MULTIBOOT_FRAMEBUFFER_RGB ||
		info->framebuffer_addr == 0 || info->framebuffer_bpp != 32 || info->framebuffer_width == 0 ||
		info->framebuffer_height == 0 || info->framebuffer_pitch < info->framebuffer_width * 4 ||
		info->green_mask == 0 || info->blue_mask == 0 || info->red_mask > 8 ||
		info->green_mask > 8 || info->blue_mask > 8)
		return -1;
	length = (uint64_t)info->framebuffer_pitch * info->framebuffer_height;
	if (length == 0 || info->framebuffer_addr > UINT64_MAX - length) return -1;
	physical_base = info->framebuffer_addr & ~(PAGE_SIZE - 1);
	offset = info->framebuffer_addr - physical_base;
	mapped_size = (length + offset + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	if (paging_map_region(physical_base, FRAMEBUFFER_VIRTUAL_BASE, mapped_size, PAGING_WRITE, 0, 1) != 0)
		return -1;
	framebuffer.address = (volatile uint8_t *)(uintptr_t)(FRAMEBUFFER_VIRTUAL_BASE + offset);
	framebuffer.pitch = info->framebuffer_pitch;
	framebuffer.width = info->framebuffer_width;
	framebuffer.height = info->framebuffer_height;
	framebuffer.red_position = info->red_position;
	framebuffer.red_mask = info->red_mask;
	framebuffer.green_position = info->green_position;
	framebuffer.green_mask = info->green_mask;
	framebuffer.blue_position = info->blue_position;
	framebuffer.blue_mask = info->blue_mask;
	framebuffer.cursor_x = 0;
	framebuffer.cursor_y = 0;
	framebuffer.ready = 1;
	clear_screen();
	return 0;
}

void display_console_putc(char character)
{
	if (!framebuffer.ready) return;
	if (character == '\n') {
		framebuffer.cursor_x = 0;
		framebuffer.cursor_y += FONT_HEIGHT;
	} else if (character == '\r') {
		framebuffer.cursor_x = 0;
	} else {
		if (framebuffer.cursor_x + FONT_WIDTH > framebuffer.width) {
			framebuffer.cursor_x = 0;
			framebuffer.cursor_y += FONT_HEIGHT;
		}
		for (uint32_t row = 0; row < FONT_HEIGHT && framebuffer.cursor_y + row < framebuffer.height; row++)
			for (uint32_t bit = 0; bit < 5; bit++)
				if (glyph_row(character, row) & (1u << (4 - bit)))
					put_pixel(framebuffer.cursor_x + bit, framebuffer.cursor_y + row, pack_rgb(224, 236, 244));
		framebuffer.cursor_x += FONT_WIDTH;
	}
	if (framebuffer.cursor_y + FONT_HEIGHT > framebuffer.height) {
		clear_screen();
		framebuffer.cursor_x = 0;
		framebuffer.cursor_y = 0;
	}
}

int display_set_mode(const struct display_mode *mode)
{
	if (mode == 0 || mode->width == 0 || mode->height == 0) return -1;
	if (!framebuffer.ready || mode->width != framebuffer.width || mode->height != framebuffer.height)
		return -1;
	return mode->format == DISPLAY_XRGB8888 || mode->format == DISPLAY_ARGB8888 ? 0 : -1;
}
