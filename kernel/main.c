 #include <arch/kaslr.h>
 #include <mm/slab.h>
 #include <kernel/rcu.h>
#include <hal/display.h>
#include <fs/devfs.h>
#include <drivers/virtio_gpu.h>
#include <arch/x86_64/idt.h>

int hal_init(void);
void pmm_init(void);
void vmm_init(void);
void pci_scan(void);
void sched_init(void);
void user_init(void);
void idt_init(void);

static struct virtio_gpu boot_gpu;

void kmain(void)
{
	hal_init();
	idt_init();
	pmm_init();
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
