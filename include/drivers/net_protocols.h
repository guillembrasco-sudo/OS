#ifndef DRIVERS_NET_PROTOCOLS_H
#define DRIVERS_NET_PROTOCOLS_H

#include <drivers/net_stack.h>

#define NET_ARP_CACHE_SIZE 16
#define NET_DNS_MAX_NAME 128
#define NET_TCP_MAX_CONNECTIONS 16
#define NET_SOCKET_MAX 32

struct net_config {
    struct net_ipv4 address;
    struct net_ipv4 netmask;
    struct net_ipv4 gateway;
    struct net_ipv4 dns;
    struct net_ipv6 ipv6_address;
    struct net_ipv6 ipv6_gateway;
    uint8_t ipv4_ready;
    uint8_t ipv6_ready;
};

enum net_dhcp_state {
    NET_DHCP_IDLE,
    NET_DHCP_DISCOVER_SENT,
    NET_DHCP_REQUEST_SENT,
    NET_DHCP_BOUND
};

enum net_tcp_state {
    NET_TCP_CLOSED,
    NET_TCP_SYN_SENT,
    NET_TCP_ESTABLISHED,
    NET_TCP_FIN_WAIT,
    NET_TCP_CLOSE_WAIT
};

struct net_arp_entry {
    struct net_ipv4 address;
    struct net_mac mac;
    uint8_t valid;
};

struct net_tcp_connection {
    enum net_tcp_state state;
    struct net_ipv4 remote_address;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t send_sequence;
    uint32_t receive_sequence;
};

struct net_socket {
    uint8_t used;
    uint8_t connected;
    uint16_t local_port;
    uint16_t remote_port;
    struct net_ipv4 remote_address;
    struct net_tcp_connection *tcp;
    enum net_tcp_state state;
};

void net_protocols_init(struct net_config *config);
void net_config_set_ipv4(struct net_config *config, struct net_ipv4 address,
                         struct net_ipv4 netmask, struct net_ipv4 gateway);
int net_arp_learn(struct net_arp_entry *cache, size_t capacity,
                  const struct net_ipv4 *address, const struct net_mac *mac);
const struct net_mac *net_arp_lookup(const struct net_arp_entry *cache,
                                     size_t capacity,
                                     const struct net_ipv4 *address);
enum net_dhcp_state net_dhcp_start(net_device *device);
int net_dhcp_handle_offer(enum net_dhcp_state *state,
                          struct net_config *config,
                          const struct net_ipv4 *address,
                          const struct net_ipv4 *gateway,
                          const struct net_ipv4 *netmask,
                          const struct net_ipv4 *dns);
int net_dns_name_valid(const char *name);
size_t net_dns_build_query(uint8_t *packet, size_t capacity,
                           uint16_t transaction_id, const char *name,
                           uint16_t query_type);
struct net_socket *net_socket_open(struct net_socket *sockets, size_t capacity,
                                   uint16_t local_port);
int net_tcp_connect(struct net_socket *socket, const struct net_ipv4 *address,
                    uint16_t remote_port);
int net_tcp_handle_syn_ack(struct net_socket *socket, uint32_t sequence);
int net_http_build_get(char *request, size_t capacity, const char *host,
                       const char *path);

#endif
