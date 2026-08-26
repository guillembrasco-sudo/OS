#include <stddef.h>
#include <stdint.h>
#include <mm/slab.h>
#include <hal/cpu.h>

#define SLAB_CPU_COUNT 64
#define SLAB_OBJECT_SIZE 256
#define SLAB_OBJECTS_PER_CPU 64

struct slab_object {
    struct slab_object *next;
};

struct per_cpu_slab {
    struct slab_object *free_list;
    unsigned char storage[SLAB_OBJECTS_PER_CPU][SLAB_OBJECT_SIZE];
};

static struct per_cpu_slab per_cpu_slab[SLAB_CPU_COUNT];

void slab_init(void)
{
    for (unsigned cpu = 0; cpu < SLAB_CPU_COUNT; ++cpu) {
        per_cpu_slab[cpu].free_list = 0;
        for (unsigned object = 0; object < SLAB_OBJECTS_PER_CPU; ++object) {
            struct slab_object *entry =
                (struct slab_object *)per_cpu_slab[cpu].storage[object];
            entry->next = per_cpu_slab[cpu].free_list;
            per_cpu_slab[cpu].free_list = entry;
        }
    }
}

void *slab_alloc(size_t size)
{
    unsigned cpu = arch_cpu_id() % SLAB_CPU_COUNT;
    struct slab_object *entry;

    if (size == 0 || size > SLAB_OBJECT_SIZE)
        return 0;
    entry = per_cpu_slab[cpu].free_list;
    if (entry == 0)
        return 0;
    per_cpu_slab[cpu].free_list = entry->next;
    return entry;
}

void slab_free(void *address)
{
    struct slab_object *entry = address;

    if (address == 0)
        return;

    // El objeto debe devolverse a la free-list de la CPU dueña del
    // storage[] donde físicamente vive, NO a la de la CPU que ejecuta
    // slab_free() ahora mismo: si una tarea migra de CPU (normal en SMP,
    // ver arch/x86_64/smp.c) entre el alloc y el free, "cpu actual" y
    // "cpu dueña de esta memoria" pueden ser distintas. Devolverlo a la
    // lista equivocada haría que un futuro slab_alloc() en esa CPU
    // entregara un puntero dentro del storage[] de OTRA CPU.
    for (unsigned cpu = 0; cpu < SLAB_CPU_COUNT; ++cpu) {
        unsigned char *base = &per_cpu_slab[cpu].storage[0][0];
        unsigned char *end  = base + sizeof(per_cpu_slab[cpu].storage);
        if ((unsigned char *)address >= base && (unsigned char *)address < end) {
            entry->next = per_cpu_slab[cpu].free_list;
            per_cpu_slab[cpu].free_list = entry;
            return;
        }
    }
    // Puntero que no pertenece a ningún storage[] de este slab: no hacer
    // nada es más seguro que corromper la free-list de una CPU al azar.
}