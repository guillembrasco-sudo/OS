#include <stdint.h>
#include <hal/cpu.h>
#include <arch/tlb.h>

#define SERIAL_PORT 0x3f8
#define VGA_MEMORY ((volatile uint16_t *)0xb8000)

static uint16_t vga_position;

uint32_t x86_cpu_security_enable(void);

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

int arch_cpu_init(void)
{
    outb(SERIAL_PORT + 1, 0x00);
    outb(SERIAL_PORT + 3, 0x80);
    outb(SERIAL_PORT + 0, 0x03);
    outb(SERIAL_PORT + 1, 0x00);
    outb(SERIAL_PORT + 3, 0x03);
    outb(SERIAL_PORT + 2, 0xc7);
    outb(SERIAL_PORT + 4, 0x0b);
    x86_pcid_enable();
    x86_cpu_security_enable();
    return 0;
}

uint32_t arch_cpu_id(void)
{
    uint32_t ebx_value;
    __asm__ volatile ("mov $1, %%eax; cpuid" : "=b"(ebx_value) : : "eax", "ecx", "edx");
    return (ebx_value >> 24) & 0xff;
}

void arch_cpu_halt(void)
{
    __asm__ volatile ("hlt");
}

void arch_cpu_enable_interrupts(void)
{
    __asm__ volatile ("sti");
}

void arch_cpu_disable_interrupts(void)
{
    __asm__ volatile ("cli");
}

void arch_cpu_relax(void)
{
    __asm__ volatile ("pause" ::: "memory");
}

void arch_console_putc(char character)
{
    if (character == '\n')
        outb(SERIAL_PORT, '\r');
    while ((inb(SERIAL_PORT + 5) & 0x20) == 0)
        ;
    outb(SERIAL_PORT, (uint8_t)character);
    if (character == '\n') {
        vga_position = (uint16_t)(((vga_position / 80) + 1) * 80);
        return;
    }
    VGA_MEMORY[vga_position++ % (80 * 25)] = (uint16_t)character | 0x0700;
}

uint64_t x86_cpu_read_cr4(void)
{
    uint64_t value;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(value));
    return value;
}

uint32_t x86_cpu_security_enable(void)
{
    uint32_t eax, ebx, ecx, edx;
    uint32_t max_leaf;
    uint64_t cr4 = x86_cpu_read_cr4();
    __asm__ volatile ("cpuid" : "=a"(max_leaf) : "a"(0) : "ebx", "ecx", "edx");
    if (max_leaf < 7)
        return 0;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                      : "a"(7), "c"(0));
    if ((ebx & (1u << 7)) != 0)
        cr4 |= 1ULL << 20;
    if ((ebx & (1u << 20)) != 0)
        cr4 |= 1ULL << 21;
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4) : "memory");
    return (uint32_t)((cr4 >> 20) & 3);
}

void stac(void)
{
    __asm__ volatile ("stac" ::: "memory");
}

void clac(void)
{
    __asm__ volatile ("clac" ::: "memory");
}