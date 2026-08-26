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
#include <mm/kheap.h>
#include <arch/paging.h>
#include <kernel/panic.h>
#include <lib/printf.h>

// Layout de multiboot_info_t (Multiboot 1) para localizar el mapa de
// memoria. boot/boot.asm arranca con la cabecera clasica Multiboot1
// (magic 0x1BADB002), asi que este es el formato que realmente llega aqui
// (no Multiboot2). Solo se listan los campos que necesitamos.
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
};
#pragma pack(pop)

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

	// kheap_init() nunca se llamaba: kmalloc() habria hecho panic() en el
	// primer uso porque heap_start seguia siendo NULL. Reservamos unas
	// paginas fisicas y le damos a kheap su alias en el mapa directo
	// (ya disponible tras vmm_init/paging_init).
#define KHEAP_INITIAL_PAGES 256 // 1 MiB de arena inicial para kmalloc
	uint64_t kheap_phys = pmm_alloc_pages(KHEAP_INITIAL_PAGES);
	if (kheap_phys == 0)
		panic("kmain: sin memoria fisica para el heap inicial del kernel");
	kheap_init((uintptr_t)paging_phys_to_virt(kheap_phys),
	           KHEAP_INITIAL_PAGES * PMM_PAGE_SIZE);
	kaslr_arch_init();
	if (display_early_console_init(multiboot_info_addr) != 0)
		kprintf("[DISPLAY] framebuffer no disponible; usando salida serie/VGA\n");
	else
		kprintf("[DISPLAY] framebuffer console ready\n");
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
