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

static void build_ethernet_header(struct net_ethernet_header *ethernet,
                                  struct net_mac destination,
                                  struct net_mac source, uint16_t ethertype)
{
    ethernet->destination = destination;
    ethernet->source = source;
    ethernet->ethertype = read_be16(ethertype); // read_be16 es su propia inversa
}

// Construye y envia una respuesta ARP cuando 'request' es un ARP request
// dirigido a nuestra IP (runtime->config.address). No hace nada si no
// tenemos IPv4 configurada (net_config_set_ipv4) o si la peticion no es
// para nosotros.
static void handle_arp_request(struct net_runtime *runtime, net_device *device,
                               const struct net_arp_ipv4 *request)
{
    uint8_t reply_frame[sizeof(struct net_ethernet_header) + sizeof(struct net_arp_ipv4)];
    struct net_ethernet_header *ethernet = (struct net_ethernet_header *)reply_frame;
    struct net_arp_ipv4 *reply = (struct net_arp_ipv4 *)(reply_frame + sizeof(*ethernet));

    if (!runtime->config.ipv4_ready || !device->transmit ||
        read_be16(request->operation) != NET_ARP_OPERATION_REQUEST ||
        memcmp(request->target_ip.bytes, runtime->config.address.bytes, 4) != 0)
        return;

    build_ethernet_header(ethernet, request->sender_mac, device->mac, ETHERTYPE_ARP);
    reply->hardware_type = request->hardware_type;
    reply->protocol_type = request->protocol_type;
    reply->hardware_length = NET_MAC_LENGTH;
    reply->protocol_length = 4;
    reply->operation = read_be16(NET_ARP_OPERATION_REPLY);
    reply->sender_mac = device->mac;
    reply->sender_ip = runtime->config.address;
    reply->target_mac = request->sender_mac;
    reply->target_ip = request->sender_ip;

    device->transmit(device, reply_frame, sizeof(reply_frame));
}

// Construye y envia una respuesta ICMP echo (pong) para un echo request
// (ping), copiando el payload tal cual exige el protocolo. length es el
// tamano total de la trama Ethernet recibida.
static void handle_icmp_echo(struct net_runtime *runtime, net_device *device,
                             const struct net_ethernet_header *request_ethernet,
                             const struct net_ipv4_header *request_ip,
                             size_t length)
{
    size_t ip_header_length = (size_t)(request_ip->version_ihl & 0x0f) * 4;
    size_t icmp_offset = sizeof(*request_ethernet) + ip_header_length;
    size_t icmp_length = length - icmp_offset;
    const struct net_icmp_header *request_icmp =
        (const struct net_icmp_header *)((const uint8_t *)request_ethernet + icmp_offset);
    uint8_t reply_frame[1514]; // MTU maximo de Ethernet + cabecera, suficiente para un ping normal
    struct net_ethernet_header *reply_ethernet;
    struct net_ipv4_header *reply_ip;
    struct net_icmp_header *reply_icmp;

    if (!device->transmit || icmp_length < sizeof(*request_icmp) ||
        request_icmp->type != NET_ICMP_TYPE_ECHO_REQUEST ||
        sizeof(reply_frame) < icmp_offset + icmp_length)
        return;

    memcpy(reply_frame, request_ethernet, icmp_offset + icmp_length);
    reply_ethernet = (struct net_ethernet_header *)reply_frame;
    reply_ip = (struct net_ipv4_header *)(reply_frame + sizeof(*reply_ethernet));
    reply_icmp = (struct net_icmp_header *)(reply_frame + icmp_offset);

    build_ethernet_header(reply_ethernet, request_ethernet->source, device->mac, ETHERTYPE_IPV4);
    reply_ip->source = request_ip->destination;
    reply_ip->destination = request_ip->source;
    reply_ip->ttl = 64;
    reply_ip->checksum = 0;
    reply_ip->checksum = read_be16(net_checksum(reply_ip, ip_header_length));

    reply_icmp->type = NET_ICMP_TYPE_ECHO_REPLY;
    reply_icmp->code = 0;
    reply_icmp->checksum = 0;
    reply_icmp->checksum = read_be16(net_checksum(reply_icmp, icmp_length));

    device->transmit(device, reply_frame, icmp_offset + icmp_length);
    runtime->stats.icmp_replies_sent++;
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
        handle_arp_request(runtime, device, arp);
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
        } else if (ipv4->protocol == IP_PROTOCOL_ICMP) {
            handle_icmp_echo(runtime, device, ethernet, ipv4, length);
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
