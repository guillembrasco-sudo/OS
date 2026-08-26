 #include <arch/kaslr.h>
 #include <mm/slab.h>
 #include <kernel/rcu.h>
#include <hal/display.h>
#include <fs/devfs.h>
#include <drivers/virtio_gpu.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/gdt.h>
#include <kernel/tss.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

// Layout de multiboot_info_t (Multiboot 1) para localizar el mapa de
// memoria. boot/boot.asm arranca con la cabecera clasica Multiboot1
// (magic 0x1BADB002), asi que este es el formato que realmente llega aqui
// (no Multiboot2). Solo se listan los campos que necesitamos.
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
} __attribute__((packed));

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u
#define MULTIBOOT_INFO_MEM_MAP     0x00000040u

int hal_init(void);
void pci_scan(void);
void sched_init(void);
void user_init(void);
void idt_init(void);
void tss_init(uint64_t kernel_stack_top);

static struct virtio_gpu boot_gpu;
extern uint8_t stack_top[];
// Exportado por linker.ld: fin fisico del kernel (codigo+datos+bss), justo
// donde termina la region que pmm_init() debe marcar como reservada.
extern uint8_t _kernel_phys_bss_end[];

// boot.asm preserva el "Multiboot magic" en rdi y el puntero fisico a
// multiboot_info en rsi antes de "call kmain" (ver comentario junto a esa
// llamada). kmain debe declarar esos dos parametros para recibirlos.
void kmain(uint64_t multiboot_magic, uint64_t multiboot_info_addr)
{
	hal_init();
	init_gdt();
	tss_init((uint64_t)stack_top);
	idt_init();

	uint64_t mmap_addr = 0;
	uint32_t mmap_len  = 0;
	if (multiboot_magic == MULTIBOOT_BOOTLOADER_MAGIC && multiboot_info_addr != 0) {
		struct multiboot_info *mbi = (struct multiboot_info *)(uintptr_t)multiboot_info_addr;
		if (mbi->flags & MULTIBOOT_INFO_MEM_MAP) {
			mmap_addr = mbi->mmap_addr;
			mmap_len  = mbi->mmap_length;
		}
	}
	// Inicio fisico del kernel: fijado por linker.ld ("`. = KERNEL_VMA + 0x00100000`",
	// y load_addr = 0x00100000 en la cabecera Multiboot de boot.asm).
	// Fin fisico: simbolo _kernel_phys_bss_end exportado por linker.ld.
	pmm_init(mmap_addr, mmap_len, 0x00100000ULL, (uint64_t)(uintptr_t)_kernel_phys_bss_end);
	vmm_init();
	kaslr_arch_init();
	display_early_console_init();
	devfs_init();
	slab_init();
	rcu_init();
	pci_scan();
	if (virtio_gpu_init(&boot_gpu, 0) != 0)
		display_set_mode(&(struct display_mode){ 80, 25, 0, 0 });
	sched_init();
	user_init();
	for (;;)
		;
}
