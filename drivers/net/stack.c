#include <drivers/net_stack.h>
static uint16_t read_be16(uint16_t value)
{
    return (uint16_t)((value >> 8) | (value << 8));
}

static uint32_t read_be32(uint32_t value)
{
    return ((value & 0x000000ffU) << 24) |
           ((value & 0x0000ff00U) << 8) |
           ((value & 0x00ff0000U) >> 8) |
           ((value & 0xff000000U) >> 24);
}

void net_device_init(net_device *device, struct net_mac mac,
                     uint16_t mtu, net_transmit_fn transmit,
                     void *private_data)
{
    if (!device)
        return;
    device->mac = mac;
    device->mtu = mtu ? mtu : NET_ETHERNET_MTU;
    device->link_up = 0;
    device->transmit = transmit;
    device->private_data = private_data;
}

int net_mac_is_broadcast(const struct net_mac *mac)
{
    if (!mac)
        return 0;
    for (size_t index = 0; index < NET_MAC_LENGTH; index++)
        if (mac->bytes[index] != 0xff)
            return 0;
    return 1;
}

int net_ipv4_is_valid(const struct net_ipv4 *address)
{
    return address != NULL;
}

int net_ipv6_is_valid(const struct net_ipv6 *address)
{
    return address != NULL;
}

uint16_t net_checksum(const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t sum = 0;
    if (!data)
        return 0;
    while (length > 1) {
        sum += ((uint16_t)bytes[0] << 8) | bytes[1];
        bytes += 2;
        length -= 2;
    }
    if (length)
        sum += (uint16_t)bytes[0] << 8;
    while (sum >> 16)
        sum = (sum & 0xffffu) + (sum >> 16);
    return (uint16_t)~sum;
}

int net_ipv4_header_valid(const struct net_ipv4_header *header, size_t length)
{
    size_t header_length;
    if (!header || length < sizeof(*header) ||
        (header->version_ihl >> 4) != 4)
        return 0;
    header_length = (size_t)(header->version_ihl & 0x0f) * 4;
    return header_length >= sizeof(*header) && header_length <= length &&
           read_be16(header->total_length) >= header_length &&
           read_be16(header->total_length) <= length;
}

int net_ipv6_header_valid(const struct net_ipv6_header *header, size_t length)
{
    uint32_t version;
    if (!header || length < sizeof(*header))
        return 0;
    version = read_be32(header->version_traffic_flow) >> 28;
    return version == 6 &&
           (size_t)read_be16(header->payload_length) <=
               length - sizeof(*header);
}

int net_tcp_header_valid(const struct net_tcp_header *header, size_t length)
{
    size_t header_length;
    if (!header || length < sizeof(*header))
        return 0;
    header_length = (size_t)(header->data_offset_reserved >> 4) * 4;
    return header_length >= sizeof(*header) && header_length <= length;
}

int net_udp_header_valid(const struct net_udp_header *header, size_t length)
{
    if (!header || length < sizeof(*header))
        return 0;
    return read_be16(header->length) >= sizeof(*header) &&
           read_be16(header->length) <= length;
}
