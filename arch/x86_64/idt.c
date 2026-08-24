#include "idt.h"

#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

// Arreglo externo con los 32 stubs de excepciones definidos en ensamblador
extern void* isr_stub_table[32];

static void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low  = (uint16_t)(base & 0xFFFF);
    idt[num].selector    = sel;
    idt[num].ist         = 0;
    idt[num].type_attr   = flags;
    idt[num].offset_mid  = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    idt[num].zero        = 0;
}

static void pic_disable(void) {
    // Enviar comandos de inicialización (ICW1)
    __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)0x11), "Nd"((uint16_t)0x20));
    __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)0x11), "Nd"((uint16_t)0xA0));

    // Remapear offset de IRQs a partir del vector 32 (0x20) (ICW2)
    __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x21));
    __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)0x28), "Nd"((uint16_t)0xA1));

    // Configurar cascada PIC maestro/esclavo (ICW3 y ICW4)
    __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)0x04), "Nd"((uint16_t)0x21));
    __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)0x02), "Nd"((uint16_t)0xA1));
    __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)0x01), "Nd"((uint16_t)0x21));
    __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)0x01), "Nd"((uint16_t)0xA1));

    // Enmascarar todas las interrupciones (desactivar PIC)
    __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)0xFF), "Nd"((uint16_t)0x21));
    __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)0xFF), "Nd"((uint16_t)0xA1));
}

void idt_init(void) {
    pic_disable();
    
    idt_ptr.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idt_ptr.base  = (uint64_t)&idt;

    // Inicializar la tabla limpia
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // Configurar las 32 excepciones del procesador (0 a 31)
    for (uint8_t i = 0; i < 32; i++) {
        idt_set_gate(i, (uint64_t)isr_stub_table[i], 0x08, 0x8E);
    }

    // Cargar la IDT usando lidt
    __asm__ __volatile__("lidt %0" : : "m"(idt_ptr));
}

// Manejador en C invocado desde el stub en Ensamblador
void exception_handler(uint64_t vector, uint64_t error_code) {
    // Evita el Triple Fault congelando la CPU de forma segura
    __asm__ __volatile__("cli");
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}