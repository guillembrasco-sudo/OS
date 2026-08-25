#include <stdint.h>

#include "arch/x86_64/gdt.h"

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

struct tss_descriptor_64 {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;       // 0x89: Present (1), Ring 0 (00), System (0), Type TSS (1001)
    uint8_t  granularity;  // Limite (bits 16-19)
    uint8_t  base_high;
    uint32_t base_upper32; // Parte alta de 32 bits de la dirección de base
    uint32_t reserved;     // Debe ser 0
} PACKED;

// Puntero de la GDT (GDTR) de 10 bytes para 64 bits (dirección de 64 bits)
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} PACKED;

// Definimos espacio para 5 descriptores iniciales
struct gdt_entry gdt[7];
struct gdt_ptr   gdtr;

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF); 
    gdt[num].base_middle = (base >> 16) & 0xFF; 
    gdt[num].base_high   = (base >> 24) & 0xFF; 

    gdt[num].limit_low   = (limit & 0xFFFF); 
    gdt[num].granularity = (limit >> 16) & 0x0F; 

    gdt[num].granularity |= gran & 0xF0; 
    gdt[num].access      = access; 
}

void gdt_set_tss_descriptor(uint64_t base, uint32_t limit) {
    // El índice 5 (0x28) coincide con el selector cargado en ltr
    struct tss_descriptor_64 *tss_desc = (struct tss_descriptor_64 *)&gdt[5];

    tss_desc->limit_low    = (uint16_t)(limit & 0xFFFF);
    tss_desc->base_low     = (uint16_t)(base & 0xFFFF);
    tss_desc->base_middle  = (uint8_t)((base >> 16) & 0xFF);
    tss_desc->access       = 0x89;
    tss_desc->granularity  = (uint8_t)((limit >> 16) & 0x0F);
    tss_desc->base_high    = (uint8_t)((base >> 24) & 0xFF);
    tss_desc->base_upper32 = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    tss_desc->reserved     = 0;
}

extern void gdt_flush(uint64_t gdtr_address); 

void init_gdt() {
    // Calcula el límite automáticamente abarcando las 7 entradas
    gdtr.limit = sizeof(gdt) - 1; 
    gdtr.base  = (uint64_t)&gdt; 

    gdt_set_gate(0, 0, 0, 0, 0); 
    gdt_set_gate(1, 0, 0, 0x9A, 0x20); 
    gdt_set_gate(2, 0, 0, 0x92, 0x00); 
    gdt_set_gate(3, 0, 0, 0xFA, 0x20); 
    gdt_set_gate(4, 0, 0, 0xF2, 0x00); 

    gdt_flush((uint64_t)&gdtr); 
}