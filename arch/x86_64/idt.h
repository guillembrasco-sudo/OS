#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// Entrada de la IDT en x86-64 (16 bytes)
typedef struct __attribute__((packed)) {
    uint16_t offset_low;  // Bits 0..15 del Handler
    uint16_t selector;    // Descriptor de Código GDT (ej. 0x08)
    uint8_t  ist;         // Interrupt Stack Table (0 si no se usa)
    uint8_t  type_attr;   // Flags (0x8E = Interrupt Gate, Ring 0)
    uint16_t offset_mid;  // Bits 16..31 del Handler
    uint32_t offset_high; // Bits 32..63 del Handler
    uint32_t zero;        // Reservado (debe ser 0)
} idt_entry_t;

// Estructura del Puntero IDTR
typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idt_ptr_t;

void idt_init(void);

#endif