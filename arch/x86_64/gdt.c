#include <stdint.h>

// Atributo para evitar que el compilador añada padding (relleno)
#define PACKED __attribute__((packed))

// Descriptor estándar de 8 bytes (Código y Datos)
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} PACKED;

// Puntero de la GDT (GDTR) de 10 bytes para 64 bits (dirección de 64 bits)
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} PACKED;

// Definimos espacio para 5 descriptores iniciales
struct gdt_entry gdt[5];
struct gdt_ptr   gdtr;

// Función auxiliar para rellenar los bits de cada descriptor
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

// Declaración de la función externa en ensamblador que cargará la GDT
extern void gdt_flush(uint64_t gdtr_address);

void init_gdt() {
    // Configura el puntero GDTR
    gdtr.limit = (sizeof(struct gdt_entry) * 5) - 1;
    gdtr.base  = (uint64_t)&gdt;

    // 1. Descriptor Nulo (Obligatorio)
    gdt_set_gate(0, 0, 0, 0, 0);

    // 2. Kernel Code (Anillo 0): Acceso 0x9A, Granularidad 0x20 (Long Mode)
    // Base y límite van en 0 porque el procesador los ignora en 64 bits.
    gdt_set_gate(1, 0, 0, 0x9A, 0x20);

    // 3. Kernel Data (Anillo 0): Acceso 0x92, Granularidad 0x00
    gdt_set_gate(2, 0, 0, 0x92, 0x00);

    // 4. User Code (Anillo 3): Acceso 0xFA, Granularidad 0x20 (Long Mode)
    gdt_set_gate(3, 0, 0, 0xFA, 0x20);

    // 5. User Data (Anillo 3): Acceso 0xF2, Granularidad 0x00
    gdt_set_gate(4, 0, 0, 0xF2, 0x00);

    // Carga la GDT usando la función de ensamblador
    gdt_flush((uint64_t)&gdtr);
}
