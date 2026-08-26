 #include <drivers/pci.h>
#include <arch/x86_64/io.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define PCI_VENDOR_INVALID 0xFFFF
#define PCI_HEADER_MULTI_FUNCTION 0x80
#define PCI_BAR_IO 0x1
#define PCI_BAR_64 0x4
#define PCI_COMMAND_OFFSET 0x04
#define PCI_COMMAND_IO 0x1
#define PCI_COMMAND_MEMORY 0x2
#define PCI_COMMAND_BUS_MASTER 0x4

static struct pci_device devices[PCI_MAX_DEVICES];
static uint32_t device_count;

uint32_t pci_config_read32(uint8_t bus, uint8_t slot,
						   uint8_t function, uint8_t offset)
{
	uint32_t address;
	if (slot >= 32 || function >= 8 || offset & 3)
		return 0xFFFFFFFFu;
	address = 0x80000000u | ((uint32_t)bus << 16) |
			  ((uint32_t)slot << 11) | ((uint32_t)function << 8) |
			  (offset & 0xFC);
	io_out32(PCI_CONFIG_ADDRESS, address);
	return io_in32(PCI_CONFIG_DATA);
}

void pci_config_write32(uint8_t bus, uint8_t slot,
						uint8_t function, uint8_t offset, uint32_t value)
{
	uint32_t address;
	if (slot >= 32 || function >= 8 || offset & 3)
		return;
	address = 0x80000000u | ((uint32_t)bus << 16) |
			  ((uint32_t)slot << 11) | ((uint32_t)function << 8) |
			  (offset & 0xFC);
	io_out32(PCI_CONFIG_ADDRESS, address);
	io_out32(PCI_CONFIG_DATA, value);
}

static void pci_read_bars(struct pci_device *device)
{
	for (uint8_t index = 0; index < PCI_BAR_COUNT; index++) {
		uint8_t offset = (uint8_t)(0x10 + index * 4);
		uint32_t low = pci_config_read32(device->bus, device->slot,
										 device->function, offset);
		uint64_t address;
		uint32_t original_low = low;
		uint32_t mask_low;
		uint32_t mask_high = 0;
		if (low == 0 || low == 0xFFFFFFFFu)
			continue;
		device->bars[index].present = 1;
		device->bars[index].io_space = (uint8_t)(low & PCI_BAR_IO);
		address = low & (device->bars[index].io_space ? ~3ULL : ~0xFULL);
		if (!device->bars[index].io_space && (low & PCI_BAR_64) == PCI_BAR_64 &&
			index + 1 < PCI_BAR_COUNT) {
			uint32_t high = pci_config_read32(device->bus, device->slot,
												   device->function,
												   (uint8_t)(offset + 4));
			address |= (uint64_t)high << 32;
			mask_high = 0;
		}
		device->bars[index].address = address;
		pci_config_write32(device->bus, device->slot, device->function,
						   offset, 0xFFFFFFFFu);
		mask_low = pci_config_read32(device->bus, device->slot,
									 device->function, offset);
		pci_config_write32(device->bus, device->slot, device->function,
						   offset, original_low);
		if (!device->bars[index].io_space && (low & PCI_BAR_64) == PCI_BAR_64 &&
			index + 1 < PCI_BAR_COUNT) {
			uint32_t high = pci_config_read32(device->bus, device->slot,
											   device->function,
											   (uint8_t)(offset + 4));
			pci_config_write32(device->bus, device->slot, device->function,
							   (uint8_t)(offset + 4), 0xFFFFFFFFu);
			mask_high = pci_config_read32(device->bus, device->slot,
										  device->function,
										  (uint8_t)(offset + 4));
			pci_config_write32(device->bus, device->slot, device->function,
							   (uint8_t)(offset + 4), high);
		}
		mask_low &= device->bars[index].io_space ? ~3u : ~0xFu;
		device->bars[index].size = mask_low ? (uint32_t)(~mask_low + 1) : 0;
		(void)mask_high;
	}
}

int pci_enable_bus_master(const struct pci_device *device)
{
	uint32_t command;
	if (!device)
		return -1;
	command = pci_config_read32(device->bus, device->slot,
								device->function, PCI_COMMAND_OFFSET);
	command |= PCI_COMMAND_BUS_MASTER;
	for (uint8_t index = 0; index < PCI_BAR_COUNT; index++)
		if (device->bars[index].present)
			command |= device->bars[index].io_space ? PCI_COMMAND_IO :
													   PCI_COMMAND_MEMORY;
	pci_config_write32(device->bus, device->slot, device->function,
					   PCI_COMMAND_OFFSET, command);
	return 0;
}

void pci_scan(void)
{
	device_count = 0;
	for (uint32_t bus = 0; bus < 256 && device_count < PCI_MAX_DEVICES; bus++)
		for (uint8_t slot = 0; slot < 32 && device_count < PCI_MAX_DEVICES; slot++)
			for (uint8_t function = 0; function < 8 && device_count < PCI_MAX_DEVICES; function++) {
				uint32_t identity = pci_config_read32((uint8_t)bus, slot, function, 0);
				struct pci_device *device;
				uint32_t class_info;
				uint32_t header;
				if ((identity & 0xFFFFu) == PCI_VENDOR_INVALID)
					continue;
				device = &devices[device_count++];
				device->bus = (uint8_t)bus;
				device->slot = slot;
				device->function = function;
				device->vendor_id = (uint16_t)identity;
				device->device_id = (uint16_t)(identity >> 16);
				class_info = pci_config_read32((uint8_t)bus, slot, function, 8);
				device->class_code = (uint8_t)(class_info >> 24);
				device->subclass = (uint8_t)(class_info >> 16);
				device->prog_if = (uint8_t)(class_info >> 8);
				device->irq_line = (uint8_t)pci_config_read32(
					(uint8_t)bus, slot, function, 0x3C);
				header = pci_config_read32((uint8_t)bus, slot, function, 0xC);
				pci_read_bars(device);
				if (function == 0 && !(header & (PCI_HEADER_MULTI_FUNCTION << 16)))
					break;
			}
}

const struct pci_device *pci_device_at(uint32_t index)
{
	return index < device_count ? &devices[index] : 0;
}

uint32_t pci_device_count(void)
{
	return device_count;
}

const struct pci_device *pci_find_device(uint16_t vendor_id,
										uint16_t device_id)
{
	for (uint32_t index = 0; index < device_count; index++)
		if (devices[index].vendor_id == vendor_id &&
			devices[index].device_id == device_id)
			return &devices[index];
	return 0;
}
