#ifndef DRIVERS_NET_DISPATCH_H
#define DRIVERS_NET_DISPATCH_H

#include <drivers/net_protocols.h>

struct net_stats {
    uint64_t frames;
    uint64_t dropped;
    uint64_t arp_frames;
    uint64_t ipv4_frames;
    uint64_t ipv6_frames;
    uint64_t tcp_frames;
    uint64_t udp_frames;
    uint64_t icmp_replies_sent;
};

struct net_runtime {
    struct net_config config;
    struct net_arp_entry arp[NET_ARP_CACHE_SIZE];
    struct net_stats stats;
};

void net_runtime_init(struct net_runtime *runtime);
int net_runtime_receive(struct net_runtime *runtime, net_device *device,
                        const void *frame, size_t length);
const struct net_stats *net_runtime_stats(const struct net_runtime *runtime);

#endif
