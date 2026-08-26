#ifndef DRIVERS_NET_STACK_H
#define DRIVERS_NET_STACK_H

#include <stdint.h>
#include <stddef.h>

#define NET_MAC_LENGTH 6
#define NET_IPV6_LENGTH 16
#define NET_ETHERNET_MTU 1500

struct net_mac {
    uint8_t bytes[NET_MAC_LENGTH];
};

struct net_ipv4 {
    uint8_t bytes[4];
};

struct net_ipv6 {
    uint8_t bytes[NET_IPV6_LENGTH];
};

struct net_device;
typedef int (*net_transmit_fn)(struct net_device *device,
                               const void *frame, size_t length);

typedef struct net_device {
    struct net_mac mac;
    uint16_t mtu;
    uint8_t link_up;
    net_transmit_fn transmit;
    void *private_data;
} net_device;

#pragma pack(push, 1)

struct net_ethernet_header {
    struct net_mac destination;
    struct net_mac source;
    uint16_t ethertype;
};

struct net_arp_ipv4 {
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t hardware_length;
    uint8_t protocol_length;
    uint16_t operation;
    struct net_mac sender_mac;
    struct net_ipv4 sender_ip;
    struct net_mac target_mac;
    struct net_ipv4 target_ip;
};

struct net_ipv4_header {
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment_offset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    struct net_ipv4 source;
    struct net_ipv4 destination;
};

struct net_ipv6_header {
    uint32_t version_traffic_flow;
    uint16_t payload_length;
    uint8_t next_header;
    uint8_t hop_limit;
    struct net_ipv6 source;
    struct net_ipv6 destination;
};

struct net_tcp_header {
    uint16_t source_port;
    uint16_t destination_port;
    uint32_t sequence;
    uint32_t acknowledgement;
    uint8_t data_offset_reserved;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_pointer;
};

struct net_udp_header {
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t length;
    uint16_t checksum;
};

#pragma pack(pop)

void net_device_init(net_device *device, struct net_mac mac,
                     uint16_t mtu, net_transmit_fn transmit,
                     void *private_data);
int net_mac_is_broadcast(const struct net_mac *mac);
int net_ipv4_is_valid(const struct net_ipv4 *address);
int net_ipv6_is_valid(const struct net_ipv6 *address);
uint16_t net_checksum(const void *data, size_t length);
int net_ipv4_header_valid(const struct net_ipv4_header *header, size_t length);
int net_ipv6_header_valid(const struct net_ipv6_header *header, size_t length);
int net_tcp_header_valid(const struct net_tcp_header *header, size_t length);
int net_udp_header_valid(const struct net_udp_header *header, size_t length);

#endif
