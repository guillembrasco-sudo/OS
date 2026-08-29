 #include <arch/kaslr.h>
 #include <mm/slab.h>
 #include <kernel/rcu.h>
#include <hal/display.h>
#include <hal/clock.h>
#include <hal/cpu.h>
#include <fs/devfs.h>
#include <fs/vfs.h>
#include <drivers/virtio_gpu.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/pci.h>
#include <drivers/virtio_net.h>
#include <drivers/net_dispatch.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/gdt.h>
#include <kernel/interrupt.h>
#include <kernel/tss.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/kheap.h>
#include <arch/paging.h>
#include <arch/x86_64/lapic.h>
#include <arch/x86_64/ioapic.h>
#include <firmware/acpi.h>
#include <kernel/panic.h>
#include <kernel/window_system.h>
#include <kernel/smp.h>
#include <lib/printf.h>
#include <fs/ramfs.h>
#include <fs/dcache.h>
#include <fs/file.h>

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
void syscall_init(void);
void tss_init(uint64_t kernel_stack_top);

static struct virtio_gpu boot_gpu;
static struct virtio_net boot_net;
static struct net_runtime net_runtime;
static uint8_t net_rx_frame[2048];
static WindowManager window_manager;
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
	isr_install_defaults();
	syscall_init();

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
	pmm_reserve_range(0x8000, 0x1000);
	vmm_init();
	smp_init();
	{
		struct acpi_platform_info platform;
		if (acpi_init(&platform) == 0 &&
		    lapic_init(platform.lapic_address) == 0 &&
		    ioapic_init(platform.ioapic_address, platform.ioapic_gsi_base) == 0)
		{
			ioapic_route_irq(1, 33, 0);
			ioapic_route_irq(12, 44, 0);
			kprintf("[ACPI] LAPIC/IOAPIC ready; CPUs enabled=%u\n",
			        platform.enabled_cpus);
		}
		else
			kprintf("[ACPI] MADT/LAPIC unavailable; using legacy IRQ path\n");
	}
	clock_init();

	// kheap_init() nunca se llamaba: kmalloc() habria hecho panic() en el
	// primer uso porque heap_start seguia siendo NULL. Reservamos unas
	// paginas fisicas y le damos a kheap su alias en el mapa directo
	// (ya disponible tras vmm_init/paging_init).
#define KHEAP_INITIAL_PAGES 4096 // 16 MiB para ventanas y sus backbuffers
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
	vfs_init();
	ramfs_init();
	devfs_init();
	dcache_init();
	file_table_init();
	slab_init();
	rcu_init();
	pci_scan();
	net_runtime_init(&net_runtime);
	// IP estatica temporal (== la que QEMU con red "user" asigna por
	// defecto al invitado) para poder probar ARP/ICMP de verdad ahora
	// mismo. net_dhcp_start()/net_dhcp_handle_offer() (drivers/net/protocols.c)
	// ya implementan el estado de un cliente DHCP real, pero nada en
	// dispatch.c dispara todavia una peticion DHCP al recibir una trama -
	// sigue siendo trabajo pendiente.
	net_config_set_ipv4(&net_runtime.config,
	                     (struct net_ipv4){{10, 0, 2, 15}},
	                     (struct net_ipv4){{255, 255, 255, 0}},
	                     (struct net_ipv4){{10, 0, 2, 2}});
	if (pci_device_count() == 0)
		kprintf("[PCI] no devices found\n");
	else {
		const struct pci_device *net_pci =
			pci_find_device(VIRTIO_NET_PCI_VENDOR, VIRTIO_NET_PCI_DEVICE);
		if (!net_pci)
			net_pci = pci_find_device(VIRTIO_NET_PCI_VENDOR, 0x1000);
		if (net_pci && pci_enable_bus_master(net_pci) == 0) {
			struct net_mac net_mac = {{0, 0, 0, 0, 0, 0}};
			if (virtio_net_init_from_pci(&boot_net, net_pci, net_mac) == 0) {
				kprintf("[NET] VirtIO-net ready for frame polling\n");
			}
		}
	}
	if (virtio_gpu_init(&boot_gpu, 0) != 0) {
		display_set_mode(&(struct display_mode){ 80, 25, 0, 0 });
	} else {
		window_manager_init_kernel(&window_manager,
		                           boot_gpu.drm.mode.width,
		                           boot_gpu.drm.mode.height);
		if (keyboard_init(&window_manager) != 0)
			kprintf("[INPUT] keyboard initialization failed\n");
		if (mouse_init(&window_manager) != 0)
			kprintf("[INPUT] mouse initialization failed\n");
	}
	sched_init();
	user_init();
	arch_cpu_enable_interrupts();
	for (;;) {
		size_t frame_length = 0;
		if (virtio_net_is_ready(&boot_net) &&
		    virtio_net_poll(&boot_net, net_rx_frame, sizeof(net_rx_frame),
		                    &frame_length) > 0)
			net_runtime_receive(&net_runtime, &boot_net.device,
			                    net_rx_frame, frame_length);
		arch_cpu_halt();
	}
}
