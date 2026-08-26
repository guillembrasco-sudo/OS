#include <drivers/virtio_net.h>
#include <arch/x86_64/io.h>
#include <arch/paging.h>
#include <mm/pmm.h>
#include <lib/string.h>

#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER 2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_LEGACY_DEVICE_FEATURES 0
#define VIRTIO_LEGACY_GUEST_FEATURES 4
#define VIRTIO_LEGACY_QUEUE_ADDRESS 8
#define VIRTIO_LEGACY_QUEUE_SIZE 12
#define VIRTIO_LEGACY_QUEUE_SELECT 14
#define VIRTIO_LEGACY_QUEUE_NOTIFY 16
#define VIRTIO_LEGACY_STATUS 18
#define VIRTIO_LEGACY_CONFIG 20
#define VIRTIO_QUEUE_PAGES 4
#define VIRTIO_DESC_F_NEXT 1
#define VIRTIO_DESC_F_WRITE 2
#define VIRTIO_NET_HEADER_SIZE 10

#pragma pack(push, 1)

struct virtio_legacy_desc {
    uint64_t address;
    uint32_t length;
    uint16_t flags;
    uint16_t next;
};

struct virtio_legacy_avail {
    uint16_t flags;
    uint16_t index;
    uint16_t ring[VIRTIO_NET_QUEUE_SIZE];
};

struct virtio_legacy_used {
    uint16_t flags;
    uint16_t index;
};

#pragma pack(pop)

static int virtio_net_send(struct net_device *device, const void *frame,
                           size_t length)
{
    struct virtio_net *network;
    if (!device || !frame || length == 0)
        return -1;
    network = (struct virtio_net *)device->private_data;
    return virtio_net_transmit(network, frame, length);
}

static uint32_t legacy_read(struct virtio_net *network, uint16_t offset)
{
    return io_in32((uint16_t)(network->common_config + offset));
}

static uint16_t legacy_read16(struct virtio_net *network, uint16_t offset)
{
    return io_in16((uint16_t)(network->common_config + offset));
}

static void legacy_write16(struct virtio_net *network, uint16_t offset,
                           uint16_t value)
{
    io_out16((uint16_t)(network->common_config + offset), value);
}

static void legacy_write(struct virtio_net *network, uint16_t offset,
                         uint32_t value)
{
    io_out32((uint16_t)(network->common_config + offset), value);
}

static int legacy_setup_queue(struct virtio_net *network,
                              struct virtio_net_queue *queue,
                              uint16_t index)
{
    uint64_t physical;
    if (!network || !queue)
        return -1;
    legacy_write16(network, VIRTIO_LEGACY_QUEUE_SELECT, index);
    if (legacy_read16(network, VIRTIO_LEGACY_QUEUE_SIZE) == 0)
        return -1;
    physical = pmm_alloc_pages(VIRTIO_QUEUE_PAGES);
    if (!physical)
        return -1;
    queue->physical_address = physical;
    queue->descriptors = paging_phys_to_virt(physical);
    memset((void *)queue->descriptors, 0, VIRTIO_QUEUE_PAGES * PMM_PAGE_SIZE);
    queue->size = VIRTIO_NET_QUEUE_SIZE;
    queue->available = 0;
    queue->used = 0;
    legacy_write(network, VIRTIO_LEGACY_QUEUE_ADDRESS,
                 (uint32_t)(physical / PMM_PAGE_SIZE));
    return 0;
}

static int legacy_init(struct virtio_net *network)
{
    uint32_t features;
    uint8_t status;
    if (!network)
        return -1;
    status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    io_out8((uint16_t)(network->common_config + VIRTIO_LEGACY_STATUS), status);
    features = legacy_read(network, VIRTIO_LEGACY_DEVICE_FEATURES);
    legacy_write(network, VIRTIO_LEGACY_GUEST_FEATURES, features & 0);
    status |= VIRTIO_STATUS_FEATURES_OK;
    io_out8((uint16_t)(network->common_config + VIRTIO_LEGACY_STATUS), status);
    if (io_in8((uint16_t)(network->common_config + VIRTIO_LEGACY_STATUS)) != status)
        return -1;
    if (legacy_setup_queue(network, &network->receive_queue, 0) != 0 ||
        legacy_setup_queue(network, &network->transmit_queue, 1) != 0)
        return -1;
    struct virtio_legacy_desc *receive_descriptors =
        (struct virtio_legacy_desc *)network->receive_queue.descriptors;
    struct virtio_legacy_avail *receive_available =
        (struct virtio_legacy_avail *)((uint8_t *)receive_descriptors + 4096);
    for (uint16_t index = 0; index < VIRTIO_NET_RX_BUFFERS; index++) {
        network->rx_buffer_physical[index] = pmm_alloc_page();
        if (!network->rx_buffer_physical[index])
            return -1;
        network->rx_buffers[index] = (uint8_t *)paging_phys_to_virt(
            network->rx_buffer_physical[index]);
        receive_descriptors[index].address = network->rx_buffer_physical[index];
        receive_descriptors[index].length = PMM_PAGE_SIZE;
        receive_descriptors[index].flags = VIRTIO_DESC_F_WRITE;
        receive_available->ring[index] = index;
    }
    receive_available->index = VIRTIO_NET_RX_BUFFERS;
    network->rx_last_used = 0;
    __sync_synchronize();
    io_out16((uint16_t)(network->common_config + VIRTIO_LEGACY_QUEUE_NOTIFY), 0);
    for (uint8_t index = 0; index < NET_MAC_LENGTH; index++)
        network->device.mac.bytes[index] = io_in8(
            (uint16_t)(network->common_config + VIRTIO_LEGACY_CONFIG + index));
    network->configured = 1;
    status |= VIRTIO_STATUS_DRIVER_OK;
    io_out8((uint16_t)(network->common_config + VIRTIO_LEGACY_STATUS), status);
    network->device.link_up = 1;
    return 0;
}

int virtio_net_pci_probe(uint16_t vendor, uint16_t device)
{
    return vendor == VIRTIO_NET_PCI_VENDOR &&
           (device == VIRTIO_NET_PCI_DEVICE || device == 0x1000);
}

int virtio_net_init(struct virtio_net *network, uintptr_t common_config,
                    struct net_mac mac)
{
    if (!network || !virtio_net_pci_probe(VIRTIO_NET_PCI_VENDOR,
                                          0x1000) ||
        common_config == 0)
        return -1;
    network->common_config = common_config;
    network->notify_config = 0;
    network->device_config = 0;
    network->receive_queue.size = VIRTIO_NET_QUEUE_SIZE;
    network->transmit_queue.size = VIRTIO_NET_QUEUE_SIZE;
    network->receive_queue.available = 0;
    network->transmit_queue.available = 0;
    network->receive_queue.used = 0;
    network->transmit_queue.used = 0;
    network->receive_queue.descriptors = 0;
    network->transmit_queue.descriptors = 0;
    network->configured = 0;
    network->legacy_io = 0;
    network->tx_buffer = 0;
    network->tx_buffer_physical = 0;
    net_device_init(&network->device, mac, NET_ETHERNET_MTU,
                    virtio_net_send, network);
    network->legacy_io = 1;
    return legacy_init(network);
}

int virtio_net_init_from_pci(struct virtio_net *network,
                             const struct pci_device *pci,
                             struct net_mac mac)
{
    if (!network || !pci || pci->vendor_id != VIRTIO_NET_PCI_VENDOR ||
        !virtio_net_pci_probe(pci->vendor_id, pci->device_id))
        return -1;
    for (uint32_t index = 0; index < PCI_BAR_COUNT; index++)
        if (pci->bars[index].present && pci->bars[index].io_space &&
            pci->bars[index].address != 0)
            return virtio_net_init(network, (uintptr_t)pci->bars[index].address,
                                   mac);
    return -1;
}

int virtio_net_poll(struct virtio_net *network, void *frame,
                    size_t capacity, size_t *length)
{
    struct virtio_legacy_desc *descriptors;
    volatile struct virtio_legacy_used *used;
    struct virtio_legacy_avail *available;
    struct virtio_legacy_used_elem { uint32_t id; uint32_t length; } *element;
    uint32_t id;
    uint32_t received_length;
    if (!network || !frame || !length || !network->configured ||
        capacity == 0 || !network->receive_queue.descriptors)
        return -1;
    descriptors = (struct virtio_legacy_desc *)network->receive_queue.descriptors;
        available = (struct virtio_legacy_avail *)((uint8_t *)descriptors + 4096);
    used = (volatile struct virtio_legacy_used *)((uint8_t *)descriptors + 4608);
    if (used->index == network->rx_last_used)
        return 0;
    element = (struct virtio_legacy_used_elem *)((uint8_t *)used + 4);
    id = element[network->rx_last_used % VIRTIO_NET_QUEUE_SIZE].id;
    received_length = element[network->rx_last_used % VIRTIO_NET_QUEUE_SIZE].length;
    if (id >= VIRTIO_NET_RX_BUFFERS || received_length < VIRTIO_NET_HEADER_SIZE)
        return -1;
    received_length -= VIRTIO_NET_HEADER_SIZE;
    if (received_length > capacity)
        return -1;
    memcpy(frame, network->rx_buffers[id] + VIRTIO_NET_HEADER_SIZE,
           received_length);
    *length = received_length;
        available->ring[available->index % VIRTIO_NET_QUEUE_SIZE] = (uint16_t)id;
        __sync_synchronize();
        available->index++;
        io_out16((uint16_t)(network->common_config + VIRTIO_LEGACY_QUEUE_NOTIFY), 0);
    network->rx_last_used++;
    return 1;
}

int virtio_net_transmit(struct virtio_net *network, const void *frame,
                        size_t length)
{
    if (!network || !network->configured || !network->device.link_up ||
        !network->transmit_queue.descriptors || !frame ||
        length < sizeof(struct net_ethernet_header) ||
        length > network->device.mtu)
        return -1;
    struct virtio_legacy_desc *descriptors;
    struct virtio_legacy_avail *available;
    volatile struct virtio_legacy_used *used;
    uint16_t old_used;
    uint16_t available_index;
    if (!network->legacy_io || length + VIRTIO_NET_HEADER_SIZE > PMM_PAGE_SIZE)
        return -1;
    if (!network->tx_buffer) {
        network->tx_buffer_physical = pmm_alloc_page();
        if (!network->tx_buffer_physical)
            return -1;
        network->tx_buffer = (uint8_t *)paging_phys_to_virt(
            network->tx_buffer_physical);
    }
    memset(network->tx_buffer, 0, VIRTIO_NET_HEADER_SIZE);
    memcpy(network->tx_buffer + VIRTIO_NET_HEADER_SIZE, frame, length);
    descriptors = (struct virtio_legacy_desc *)network->transmit_queue.descriptors;
    available = (struct virtio_legacy_avail *)((uint8_t *)descriptors + 4096);
    used = (volatile struct virtio_legacy_used *)((uint8_t *)descriptors + 4608);
    descriptors[0].address = network->tx_buffer_physical;
    descriptors[0].length = (uint32_t)(length + VIRTIO_NET_HEADER_SIZE);
    descriptors[0].flags = 0;
    descriptors[0].next = 0;
    old_used = used->index;
    available_index = available->index;
    available->ring[available_index % VIRTIO_NET_QUEUE_SIZE] = 0;
    __sync_synchronize();
    available->index = (uint16_t)(available_index + 1);
    __sync_synchronize();
    io_out16((uint16_t)(network->common_config + VIRTIO_LEGACY_QUEUE_NOTIFY), 1);
    for (uint32_t attempt = 0; attempt < 100000; attempt++)
        if (used->index != old_used)
            return 0;
    return -1;
}

int virtio_net_is_ready(const struct virtio_net *network)
{
    return network != 0 && network->configured && network->device.link_up;
}
