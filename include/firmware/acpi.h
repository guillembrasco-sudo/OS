#ifndef FIRMWARE_ACPI_H
#define FIRMWARE_ACPI_H

#include <stdint.h>

struct acpi_platform_info {
    uint64_t lapic_address;
    uint32_t ioapic_address;
    uint32_t ioapic_gsi_base;
    uint32_t enabled_cpus;
};

int acpi_init(struct acpi_platform_info *info);

#endif
