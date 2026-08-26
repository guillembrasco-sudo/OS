#include <kernel/elf_loader.h>
#include <arch/paging.h>
#include <mm/pmm.h>
#include <lib/string.h>

#define ELF64_CLASS 2
#define ELF64_DATA_LSB 1
#define ELF64_MACHINE_X86_64 62
#define ELF_PT_LOAD 1
#define ELF_PF_W 2
#define ELF_USER_LIMIT 0x0000800000000000ULL
#define ELF_PAGE_SIZE 0x1000ULL
#define USER_STACK_TOP 0x00007fffffffe000ULL

struct elf64_header {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct elf64_program_header {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

static int range_inside(uint64_t offset, uint64_t length, size_t size)
{
    return offset <= size && length <= (uint64_t)size - offset;
}

int elf64_load_user(struct process *process, const void *image, size_t image_size)
{
    const struct elf64_header *header = image;
    const uint8_t *bytes = image;
    uint64_t stack_top = USER_STACK_TOP;

    if (!process || !image || image_size < sizeof(*header))
        return -1;
    if (header->ident[0] != 0x7f || header->ident[1] != 'E' ||
        header->ident[2] != 'L' || header->ident[3] != 'F' ||
        header->ident[4] != ELF64_CLASS || header->ident[5] != ELF64_DATA_LSB ||
        header->machine != ELF64_MACHINE_X86_64 || header->type != 2 ||
        header->phentsize < sizeof(struct elf64_program_header) ||
        !range_inside(header->phoff,
                      (uint64_t)header->phnum * header->phentsize,
                      image_size))
        return -1;
    if (header->entry >= ELF_USER_LIMIT)
        return -1;
    if (process_create_user(process, (uintptr_t)header->entry,
                            (uintptr_t)stack_top) != 0)
        return -1;

    for (uint16_t index = 0; index < header->phnum; ++index) {
        const struct elf64_program_header *program =
            (const struct elf64_program_header *)(bytes + header->phoff +
                                                  (uint64_t)index * header->phentsize);
        uint64_t first_page;
        uint64_t last_page;
        if (program->type != ELF_PT_LOAD)
            continue;
        if (program->filesz > program->memsz ||
            !range_inside(program->offset, program->filesz, image_size) ||
            program->vaddr >= ELF_USER_LIMIT ||
            program->memsz > ELF_USER_LIMIT - program->vaddr)
            return -1;
        first_page = program->vaddr & ~(ELF_PAGE_SIZE - 1);
        last_page = (program->vaddr + program->memsz + ELF_PAGE_SIZE - 1) &
                    ~(ELF_PAGE_SIZE - 1);
        for (uint64_t page = first_page; page < last_page; page += ELF_PAGE_SIZE) {
            uint64_t physical = pmm_alloc_page();
            uint64_t copy_start = page > program->vaddr ? page : program->vaddr;
            uint64_t copy_end = program->vaddr + program->filesz;
            if (!physical || paging_map_page_in_space(
                    process->pml4_physical, page, physical,
                    (program->flags & ELF_PF_W) ? PAGING_WRITE : 0) != 0)
                return -1;
            memset((void *)(uintptr_t)paging_phys_to_virt(physical), 0,
                   ELF_PAGE_SIZE);
            if (copy_start < copy_end) {
                uint64_t copy_length = copy_end - copy_start;
                if (copy_length > ELF_PAGE_SIZE - (copy_start - page))
                    copy_length = ELF_PAGE_SIZE - (copy_start - page);
                memcpy((void *)(uintptr_t)(paging_phys_to_virt(physical) +
                                           copy_start - page),
                       bytes + program->offset + copy_start - program->vaddr,
                       (size_t)copy_length);
            }
        }
    }
    return 0;
}
