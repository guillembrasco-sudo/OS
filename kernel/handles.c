#include <kernel/handles.h>
#include <kernel/spinlock.h>

#define HANDLE_PROCESS_LIMIT 64
#define HANDLE_TABLE_SIZE 128

struct handle_entry {
    void *object;
    uint32_t process_id;
    uint32_t rights;
    uint8_t used;
};

static struct handle_entry table[HANDLE_TABLE_SIZE];
static spinlock_t table_lock = SPINLOCK_INIT;

int handle_install(uint32_t process_id, void *object, uint32_t rights,
                   handle_t *handle)
{
    if (process_id == 0 || object == 0 || rights == 0 || handle == 0)
        return -1;
    spinlock_acquire(&table_lock);
    for (handle_t index = 1; index < HANDLE_TABLE_SIZE; ++index) {
        if (!table[index].used) {
            table[index].object = object;
            table[index].process_id = process_id;
            table[index].rights = rights;
            table[index].used = 1;
            *handle = index;
            spinlock_release(&table_lock);
            return 0;
        }
    }
    spinlock_release(&table_lock);
    return -1;
}

void *handle_lookup(uint32_t process_id, handle_t handle, uint32_t rights)
{
    void *object = 0;
    if (process_id == 0 || handle == 0 || handle >= HANDLE_TABLE_SIZE)
        return 0;
    spinlock_acquire(&table_lock);
    if (table[handle].used && table[handle].process_id == process_id &&
        (table[handle].rights & rights) == rights)
        object = table[handle].object;
    spinlock_release(&table_lock);
    return object;
}

int handle_close(uint32_t process_id, handle_t handle)
{
    if (process_id == 0 || handle == 0 || handle >= HANDLE_TABLE_SIZE)
        return -1;
    spinlock_acquire(&table_lock);
    if (!table[handle].used || table[handle].process_id != process_id) {
        spinlock_release(&table_lock);
        return -1;
    }
    table[handle].used = 0;
    table[handle].object = 0;
    spinlock_release(&table_lock);
    return 0;
}