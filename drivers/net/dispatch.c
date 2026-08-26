#include <drivers/net_dispatch.h>
#include <lib/string.h>

#define ETHERTYPE_ARP 0x0806
#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_IPV6 0x86DD
#define IP_PROTOCOL_TCP 6
#define IP_PROTOCOL_UDP 17

static uint16_t read_be16(uint16_t value)
{
    return (uint16_t)((value >> 8) | (value << 8));
}

void net_runtime_init(struct net_runtime *runtime)
{
    if (!runtime)
        return;
    memset(runtime, 0, sizeof(*runtime));
    net_protocols_init(&runtime->config);
}

int net_runtime_receive(struct net_runtime *runtime, net_device *device,
                        const void *frame, size_t length)
{
    const struct net_ethernet_header *ethernet;
    uint16_t ethertype;
    if (!runtime || !device || !frame ||
        length < sizeof(struct net_ethernet_header))
        return -1;
    runtime->stats.frames++;
    ethernet = (const struct net_ethernet_header *)frame;
    ethertype = read_be16(ethernet->ethertype);
    if (ethertype == ETHERTYPE_ARP) {
        const struct net_arp_ipv4 *arp;
        if (length < sizeof(*ethernet) + sizeof(*arp))
            goto dropped;
        arp = (const struct net_arp_ipv4 *)((const uint8_t *)frame + sizeof(*ethernet));
        if (arp->hardware_length != NET_MAC_LENGTH || arp->protocol_length != 4)
            goto dropped;
        if (net_arp_learn(runtime->arp, NET_ARP_CACHE_SIZE,
                          &arp->sender_ip, &arp->sender_mac) != 0)
            goto dropped;
        runtime->stats.arp_frames++;
        return 0;
    }
    if (ethertype == ETHERTYPE_IPV4) {
        const struct net_ipv4_header *ipv4;
        size_t payload_offset;
        if (length < sizeof(*ethernet) + sizeof(*ipv4))
            goto dropped;
        ipv4 = (const struct net_ipv4_header *)((const uint8_t *)frame + sizeof(*ethernet));
        if (!net_ipv4_header_valid(ipv4, length - sizeof(*ethernet)))
            goto dropped;
        payload_offset = (size_t)(ipv4->version_ihl & 0x0f) * 4;
        runtime->stats.ipv4_frames++;
        if (ipv4->protocol == IP_PROTOCOL_TCP) {
            if (!net_tcp_header_valid((const struct net_tcp_header *)((const uint8_t *)ipv4 + payload_offset),
                                      length - sizeof(*ethernet) - payload_offset))
                goto dropped;
            runtime->stats.tcp_frames++;
        } else if (ipv4->protocol == IP_PROTOCOL_UDP) {
            if (!net_udp_header_valid((const struct net_udp_header *)((const uint8_t *)ipv4 + payload_offset),
                                      length - sizeof(*ethernet) - payload_offset))
                goto dropped;
            runtime->stats.udp_frames++;
        }
        return 0;
    }
    if (ethertype == ETHERTYPE_IPV6) {
        if (length < sizeof(*ethernet) + sizeof(struct net_ipv6_header) ||
            !net_ipv6_header_valid((const struct net_ipv6_header *)
                ((const uint8_t *)frame + sizeof(*ethernet)),
                length - sizeof(*ethernet)))
            goto dropped;
        runtime->stats.ipv6_frames++;
        return 0;
    }
    return 0;

dropped:
    runtime->stats.dropped++;
    return -1;
}

const struct net_stats *net_runtime_stats(const struct net_runtime *runtime)
{
    return runtime ? &runtime->stats : 0;
}
