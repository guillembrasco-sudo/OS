#define PAGE_SHIFT      12
#define TABLE_DESCRIPTOR 0x3
#define PAGE_DESCRIPTOR  0x3
#define ACCESS_FLAG      (1 << 10)
#define DEVICE_MEMORY    (1 << 2) // Definido previamente en el registro MAIR

// Estructura de una entrada de tabla (Descriptor de 64 bits)
typedef unsigned long page_table_entry_t;

// Configurar los registros base de traducción
void mmu_init(page_table_entry_t *kernel_l1_table) {
    // 1. Configurar MAIR (Memory Attribute Indirection Register)
    // Define el tipo de memoria: Atributo 0 = Normal con caché, Atributo 1 = Device (MMIO)
    unsigned long mair = (0xFF << 0) | (0x04 << 8); 
    asm volatile("msr mair_el1, %0" :: "r"(mair));

    // 2. Apuntar el registro del Kernel a nuestra tabla raíz
    asm volatile("msr ttbr1_el1, %0" :: "r"(kernel_l1_table));

    // 3. Configurar TCR (Translation Control Register)
    // Define el tamaño de las direcciones (ej. 48 bits) y páginas de 4KB
    unsigned long tcr = (16 << 0) | (16 << 16) | (3 << 8) | (3 << 24); 
    asm volatile("msr tcr_el1, %0" :: "r"(tcr));

    // 4. Habilitar la MMU en el registro del sistema SCTLR_EL1
    unsigned long sctlr;
    asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= 0x1; // Bit 0: Habilitar MMU
    asm volatile("msr sctlr_el1, %0" :: "r"(sctlr));
}

// Mapear una dirección virtual a física
void mmu_map_page(page_table_entry_t *root_table, unsigned long virtual_addr, unsigned long physical_addr, unsigned long flags) {
    // Extraer los índices para los niveles de páginas L1, L2, L3 (ARM64 usa comúnmente 3 o 4 niveles)
    unsigned int l1_index = (virtual_addr >> 30) & 0x1FF;
    unsigned int l2_index = (virtual_addr >> 21) & 0x1FF;
    unsigned int l3_index = (virtual_addr >> 12) & 0x1FF;

    // Recorrer los niveles creando subtablas si no existen (similar a x86_64)
    // ...
    
    // En el último nivel (L3), escribir el descriptor de página física
    root_table[l3_index] = physical_addr | PAGE_DESCRIPTOR | ACCESS_FLAG | flags;
}
