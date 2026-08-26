#ifndef DRIVERS_VIRTIO_NET_H
#define DRIVERS_VIRTIO_NET_H

#include <drivers/net_stack.h>
#include <drivers/pci.h>

#define VIRTIO_NET_PCI_VENDOR 0x1af4
#define VIRTIO_NET_PCI_DEVICE 0x1041
#define VIRTIO_NET_QUEUE_SIZE 256
#define VIRTIO_NET_RX_BUFFERS 8

struct virtio_net_queue {
    uint16_t size;
    volatile uint16_t available;
    volatile uint16_t used;
    uintptr_t descriptors;
    uint64_t physical_address;
};

struct virtio_net {
    net_device device;
    uintptr_t common_config;
    uintptr_t notify_config;
    uintptr_t device_config;
    struct virtio_net_queue receive_queue;
    struct virtio_net_queue transmit_queue;
    uint8_t configured;
    uint8_t legacy_io;
    uint8_t *tx_buffer;
    uint64_t tx_buffer_physical;
    uint8_t *rx_buffers[VIRTIO_NET_RX_BUFFERS];
    uint64_t rx_buffer_physical[VIRTIO_NET_RX_BUFFERS];
    uint16_t rx_last_used;
};

int virtio_net_pci_probe(uint16_t vendor, uint16_t device);
int virtio_net_init(struct virtio_net *network, uintptr_t common_config,
                    struct net_mac mac);
int virtio_net_init_from_pci(struct virtio_net *network,
                             const struct pci_device *pci,
                             struct net_mac mac);
int virtio_net_transmit(struct virtio_net *network, const void *frame,
                        size_t length);
int virtio_net_is_ready(const struct virtio_net *network);
int virtio_net_poll(struct virtio_net *network, void *frame,
                    size_t capacity, size_t *length);

#endif
