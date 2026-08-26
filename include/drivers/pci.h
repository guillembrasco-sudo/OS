#ifndef DRIVERS_PCI_H
#define DRIVERS_PCI_H

#include <stdint.h>

#define PCI_MAX_DEVICES 64
#define PCI_BAR_COUNT 6

struct pci_bar {
    uint64_t address;
    uint32_t size;
    uint8_t io_space;
    uint8_t present;
};

struct pci_device {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t irq_line;
    struct pci_bar bars[PCI_BAR_COUNT];
};

void pci_scan(void);
uint32_t pci_config_read32(uint8_t bus, uint8_t slot,
                           uint8_t function, uint8_t offset);
void pci_config_write32(uint8_t bus, uint8_t slot,
                        uint8_t function, uint8_t offset, uint32_t value);
int pci_enable_bus_master(const struct pci_device *device);
const struct pci_device *pci_device_at(uint32_t index);
uint32_t pci_device_count(void);
const struct pci_device *pci_find_device(uint16_t vendor_id,
                                        uint16_t device_id);

#endif
