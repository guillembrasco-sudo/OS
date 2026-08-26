#include <stdint.h>
#include <stddef.h>
#include <firmware/acpi.h>
#include <arch/paging.h>

#define RSDP_SCAN_START 0x000e0000ULL
#define RSDP_SCAN_END   0x00100000ULL
#define RSDP_SIGNATURE  "RSD PTR "
#define SDT_SIGNATURE_MADT 0x43495041u

struct rsdp_v1 {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
};

struct rsdp_v2 {
    struct rsdp_v1 first;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
};

struct sdt_header {
    uint32_t signature;
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};

struct madt {
    struct sdt_header header;
    uint32_t lapic_address;
    uint32_t flags;
};

static int checksum_valid(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    uint8_t sum = 0;
    for (size_t index = 0; index < length; ++index)
        sum = (uint8_t)(sum + bytes[index]);
    return sum == 0;
}

static int signature_valid(const char *left, const char *right, size_t length)
{
    for (size_t index = 0; index < length; ++index)
        if (left[index] != right[index])
            return 0;
    return 1;
}

static const struct rsdp_v2 *find_rsdp(void)
{
    for (uint64_t address = RSDP_SCAN_START;
         address < RSDP_SCAN_END; address += 16) {
        const struct rsdp_v2 *candidate =
            (const struct rsdp_v2 *)(uintptr_t)address;
        if (!signature_valid(candidate->first.signature, RSDP_SIGNATURE, 8) ||
            !checksum_valid(candidate, sizeof(struct rsdp_v1)))
            continue;
        if (candidate->first.revision < 2)
            return candidate;
        if (candidate->length >= sizeof(*candidate) &&
            checksum_valid(candidate, candidate->length))
            return candidate;
    }
    return 0;
}

static const struct sdt_header *physical_sdt(uint64_t physical)
{
    return (const struct sdt_header *)(uintptr_t)paging_phys_to_virt(physical);
}

int acpi_init(struct acpi_platform_info *info)
{
    const struct rsdp_v2 *rsdp;
    const struct sdt_header *root;
    const uint8_t *entries;
    uint32_t entry_size;

    if (!info)
        return -1;
    *info = (struct acpi_platform_info){0};
    rsdp = find_rsdp();
    if (!rsdp)
        return -1;
    if (rsdp->first.revision >= 2 && rsdp->xsdt_address) {
        root = physical_sdt(rsdp->xsdt_address);
        entry_size = 8;
    } else {
        root = physical_sdt(rsdp->first.rsdt_address);
        entry_size = 4;
    }
    if (!root || root->length < sizeof(*root) ||
        !checksum_valid(root, root->length))
        return -1;
    entries = (const uint8_t *)root + sizeof(*root);
    for (uint32_t offset = 0; offset + entry_size <=
         root->length - sizeof(*root); offset += entry_size) {
        uint64_t table_address = entry_size == 8
            ? *(const uint64_t *)(entries + offset)
            : *(const uint32_t *)(entries + offset);
        const struct sdt_header *header = physical_sdt(table_address);
        if (!header || header->signature != SDT_SIGNATURE_MADT ||
            header->length < sizeof(struct madt) ||
            !checksum_valid(header, header->length))
            continue;
        {
            const struct madt *table = (const struct madt *)header;
            uint32_t cursor = sizeof(*table);
            info->lapic_address = table->lapic_address;
            while (cursor + 2 <= table->header.length) {
                const uint8_t *entry = (const uint8_t *)table + cursor;
                if (entry[1] < 2 || cursor + entry[1] > table->header.length)
                    break;
                if (entry[0] == 0 && entry[1] >= 8 &&
                    (*(const uint32_t *)(entry + 4) & 1))
                    ++info->enabled_cpus;
                if (entry[0] == 1 && entry[1] >= 12 &&
                    !info->ioapic_address) {
                    info->ioapic_address = *(const uint32_t *)(entry + 4);
                    info->ioapic_gsi_base = *(const uint32_t *)(entry + 8);
                }
                cursor += entry[1];
            }
        }
        return info->lapic_address && info->ioapic_address ? 0 : -1;
    }
    return -1;
}
