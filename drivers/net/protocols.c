#include <drivers/net_protocols.h>
#include <lib/string.h>

static int ipv4_equal(const struct net_ipv4 *left,
                      const struct net_ipv4 *right)
{
        return left && right && left->bytes[0] == right->bytes[0] &&
            left->bytes[1] == right->bytes[1] &&
            left->bytes[2] == right->bytes[2] &&
            left->bytes[3] == right->bytes[3];
}

void net_protocols_init(struct net_config *config)
{
    if (config)
        memset(config, 0, sizeof(*config));
}

// Configura una IPv4 estatica para el runtime. El cliente DHCP (net_dhcp_*)
// ya tiene el estado necesario para negociarla dinamicamente, pero nada en
// dispatch.c dispara todavia una peticion DHCP real al recibir una trama, asi
// que de momento esta es la unica forma de que el stack sepa "cual es mi IP"
// para poder contestar ARP/ICMP.
void net_config_set_ipv4(struct net_config *config, struct net_ipv4 address,
                         struct net_ipv4 netmask, struct net_ipv4 gateway)
{
    if (!config)
        return;
    config->address = address;
    config->netmask = netmask;
    config->gateway = gateway;
    config->ipv4_ready = 1;
}

int net_arp_learn(struct net_arp_entry *cache, size_t capacity,
                  const struct net_ipv4 *address, const struct net_mac *mac)
{
    size_t free_index = capacity;
    if (!cache || !address || !mac)
        return -1;
    for (size_t index = 0; index < capacity; index++) {
        if (cache[index].valid && ipv4_equal(&cache[index].address, address)) {
            cache[index].mac = *mac;
            return 0;
        }
        if (!cache[index].valid && free_index == capacity)
            free_index = index;
    }
    if (free_index == capacity)
        return -1;
    cache[free_index].address = *address;
    cache[free_index].mac = *mac;
    cache[free_index].valid = 1;
    return 0;
}

const struct net_mac *net_arp_lookup(const struct net_arp_entry *cache,
                                     size_t capacity,
                                     const struct net_ipv4 *address)
{
    if (!cache || !address)
        return NULL;
    for (size_t index = 0; index < capacity; index++)
        if (cache[index].valid && ipv4_equal(&cache[index].address, address))
            return &cache[index].mac;
    return NULL;
}

enum net_dhcp_state net_dhcp_start(net_device *device)
{
    if (!device || !device->transmit || !device->link_up)
        return NET_DHCP_IDLE;
    return NET_DHCP_DISCOVER_SENT;
}

int net_dhcp_handle_offer(enum net_dhcp_state *state,
                          struct net_config *config,
                          const struct net_ipv4 *address,
                          const struct net_ipv4 *gateway,
                          const struct net_ipv4 *netmask,
                          const struct net_ipv4 *dns)
{
    if (!state || !config || !address || !gateway || !netmask || !dns ||
        *state != NET_DHCP_DISCOVER_SENT)
        return -1;
    config->address = *address;
    config->gateway = *gateway;
    config->netmask = *netmask;
    config->dns = *dns;
    config->ipv4_ready = 1;
    *state = NET_DHCP_BOUND;
    return 0;
}

int net_dns_name_valid(const char *name)
{
    size_t length = 0;
    if (!name || !name[0])
        return 0;
    for (; name[length]; length++)
        if (name[length] <= ' ' || name[length] == '/' ||
            name[length] == ':')
            return 0;
    return length <= NET_DNS_MAX_NAME && name[0] != '.' &&
           name[length - 1] != '.';
}

size_t net_dns_build_query(uint8_t *packet, size_t capacity,
                           uint16_t transaction_id, const char *name,
                           uint16_t query_type)
{
    size_t offset = 12;
    size_t label_start;
    if (!packet || capacity < 17 || !net_dns_name_valid(name))
        return 0;
    memset(packet, 0, capacity);
    packet[0] = (uint8_t)(transaction_id >> 8);
    packet[1] = (uint8_t)transaction_id;
    packet[5] = 1;
    label_start = 0;
    for (size_t index = 0;; index++) {
        if (name[index] == '.' || name[index] == '\0') {
            size_t label_length = index - label_start;
            if (label_length == 0 || label_length > 63 ||
                offset + label_length + 1 >= capacity)
                return 0;
            packet[offset++] = (uint8_t)label_length;
            memcpy(packet + offset, name + label_start, label_length);
            offset += label_length;
            if (name[index] == '\0')
                break;
            label_start = index + 1;
        }
    }
    if (offset + 5 >= capacity)
        return 0;
    packet[offset++] = 0;
    packet[offset++] = (uint8_t)(query_type >> 8);
    packet[offset++] = (uint8_t)query_type;
    packet[offset++] = 0;
    packet[offset++] = 1;
    return offset;
}

struct net_socket *net_socket_open(struct net_socket *sockets, size_t capacity,
                                   uint16_t local_port)
{
    if (!sockets || local_port == 0)
        return NULL;
    for (size_t index = 0; index < capacity; index++)
        if (!sockets[index].used) {
            memset(&sockets[index], 0, sizeof(sockets[index]));
            sockets[index].used = 1;
            sockets[index].local_port = local_port;
            sockets[index].state = NET_TCP_CLOSED;
            return &sockets[index];
        }
    return NULL;
}

int net_tcp_connect(struct net_socket *socket, const struct net_ipv4 *address,
                    uint16_t remote_port)
{
    if (!socket || !socket->used || !address || remote_port == 0)
        return -1;
    socket->remote_address = *address;
    socket->remote_port = remote_port;
    socket->tcp = NULL;
    socket->connected = 0;
    socket->state = NET_TCP_SYN_SENT;
    return 0;
}

int net_tcp_handle_syn_ack(struct net_socket *socket, uint32_t sequence)
{
    if (!socket || !socket->used)
        return -1;
    socket->connected = 1;
    socket->tcp = NULL;
    socket->state = NET_TCP_ESTABLISHED;
    (void)sequence;
    return 0;
}

int net_http_build_get(char *request, size_t capacity, const char *host,
                       const char *path)
{
    size_t host_length = 0;
    size_t path_length = 0;
    if (!request || !host || !path || path[0] != '/')
        return -1;
    while (host[host_length]) host_length++;
    while (path[path_length]) path_length++;
    if (host_length == 0 || host_length > 255 || path_length > 1024 ||
        capacity < host_length + path_length + 46)
        return -1;
    memcpy(request, "GET ", 4);
    memcpy(request + 4, path, path_length);
    memcpy(request + 4 + path_length, " HTTP/1.1\r\nHost: ", 17);
    memcpy(request + 21 + path_length, host, host_length);
    memcpy(request + 21 + path_length + host_length,
           "\r\nConnection: close\r\n\r\n", 24);
    request[45 + path_length + host_length] = '\0';
    return (int)(45 + path_length + host_length);
}
