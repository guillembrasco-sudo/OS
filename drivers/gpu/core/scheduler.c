#include <drivers/gpu_ioctl.h>
#include <drivers/gpu_fence.h>
#include <kernel/spinlock.h>

#define GPU_MAX_CONTEXTS 32
#define GPU_QUEUE_DEPTH 128

struct gpu_context {
    uint8_t used;
    uint32_t id;
    uint32_t owner;
    uint64_t gpu_page_table;
    struct gpu_fence fence;
    struct gpu_command queue[GPU_ENGINE_COUNT][GPU_QUEUE_DEPTH];
    uint32_t queue_count[GPU_ENGINE_COUNT];
};

static struct gpu_context contexts[GPU_MAX_CONTEXTS];
static uint32_t next_context_id = 1;
static struct qspinlock context_lock = QSPINLOCK_INITIALIZER;

static struct gpu_context *find_context(uint32_t owner, uint32_t id)
{
    for (unsigned index = 0; index < GPU_MAX_CONTEXTS; ++index)
        if (contexts[index].used && contexts[index].owner == owner &&
            contexts[index].id == id)
            return &contexts[index];
    return 0;
}

static int command_valid(const struct gpu_command *command)
{
    if (command->opcode < GPU_CMD_BIND_PIPELINE ||
        command->opcode > GPU_CMD_DRAW_INDEXED)
        return 0;
    if (command->length > (64u * 1024u) ||
        (command->offset & 3u) != 0)
        return 0;
    return 1;
}

static int create_context(uint32_t owner, struct gpu_ioctl_context *request)
{
    for (unsigned index = 0; index < GPU_MAX_CONTEXTS; ++index) {
        if (contexts[index].used == 0) {
            struct gpu_context *context = &contexts[index];
            context->used = 1;
            context->owner = owner;
            context->id = next_context_id++;
            context->gpu_page_table = (uintptr_t)context;
            gpu_fence_init(&context->fence);
            for (unsigned engine = 0; engine < GPU_ENGINE_COUNT; ++engine)
                context->queue_count[engine] = 0;
            request->context_id = context->id;
            return 0;
        }
    }
    return -1;
}

int gpu_ioctl_dispatch(uint32_t process_id, uint32_t request, void *argument)
{
    int result;
    if (argument == 0)
        return -1;
    qspin_lock(&context_lock);
    if (request == GPU_IOCTL_CONTEXT_CREATE) {
        result = create_context(process_id, argument);
        qspin_unlock(&context_lock);
        return result;
    }
    if (request == GPU_IOCTL_CONTEXT_DESTROY) {
        struct gpu_ioctl_context *destroy = argument;
        struct gpu_context *context = find_context(process_id,
                                                   destroy->context_id);
        if (context == 0) {
            qspin_unlock(&context_lock);
            return -1;
        }
        context->used = 0;
        qspin_unlock(&context_lock);
        return 0;
    }
    if (request == GPU_IOCTL_SUBMIT_3D) {
        struct gpu_ioctl_submit *submit = argument;
        struct gpu_context *context = find_context(process_id,
                                                   submit->context_id);
        struct gpu_command *commands = (struct gpu_command *)submit->commands;
        uintptr_t command_bytes;
        if (context == 0 || commands == 0 || submit->command_count == 0 ||
            submit->engine >= GPU_ENGINE_COUNT ||
            submit->command_count > GPU_MAX_COMMANDS ||
            submit->command_count > GPU_QUEUE_DEPTH -
                context->queue_count[submit->engine] ||
            submit->command_count > ((uintptr_t)-1) / sizeof(*commands) ||
            submit->commands > (uintptr_t)-1 -
                submit->command_count * sizeof(*commands))
            { qspin_unlock(&context_lock); return -1; }
        command_bytes = submit->command_count * sizeof(*commands);
        (void)command_bytes;
        for (uint32_t index = 0; index < submit->command_count; ++index)
            if (!command_valid(&commands[index])) {
                qspin_unlock(&context_lock);
                return -1;
            }
        for (uint32_t index = 0; index < submit->command_count; ++index)
            context->queue[submit->engine][context->queue_count[submit->engine]++] =
                commands[index];
        submit->signal_sequence = gpu_fence_submit(&context->fence);
        /* Hardware completion interrupt will signal this in the real driver. */
        gpu_fence_signal(&context->fence, submit->signal_sequence);
        context->queue_count[submit->engine] = 0;
        qspin_unlock(&context_lock);
        return 0;
    }
    if (request == GPU_IOCTL_WAIT_FENCE) {
        struct gpu_ioctl_wait *wait = argument;
        struct gpu_context *context = find_context(process_id,
                                                   wait->context_id);
        if (context == 0) {
            qspin_unlock(&context_lock);
            return -1;
        }
        qspin_unlock(&context_lock);
        return gpu_fence_wait(&context->fence, wait->sequence);
    }
    qspin_unlock(&context_lock);
    return -1;
}