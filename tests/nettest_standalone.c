#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <drivers/net_dispatch.h>

static uint8_t last_frame[2048];
static size_t last_frame_len;
static int transmit_calls;

static int fake_transmit(net_device *device, const void *frame, size_t length) {
    (void)device;
    memcpy(last_frame, frame, length);
    last_frame_len = length;
    transmit_calls++;
    return 0;
}

static uint16_t be16(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }

int main(void) {
    struct net_runtime runtime;
    net_device device;
    struct net_mac our_mac = {{0x52, 0x54, 0x00, 0x12, 0x34, 0x56}};

    net_runtime_init(&runtime);
    net_device_init(&device, our_mac, 1500, fake_transmit, 0);
    net_config_set_ipv4(&runtime.config,
                         (struct net_ipv4){{10, 0, 2, 15}},
                         (struct net_ipv4){{255, 255, 255, 0}},
                         (struct net_ipv4){{10, 0, 2, 2}});

    // --- Test 1: ARP request dirigido a nuestra IP ---
    {
        uint8_t frame[sizeof(struct net_ethernet_header) + sizeof(struct net_arp_ipv4)];
        struct net_ethernet_header *eth = (struct net_ethernet_header *)frame;
        struct net_arp_ipv4 *arp = (struct net_arp_ipv4 *)(frame + sizeof(*eth));
        struct net_mac peer_mac = {{0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x01}};

        memset(eth->destination.bytes, 0xff, 6); // broadcast
        eth->source = peer_mac;
        eth->ethertype = be16(0x0806);

        arp->hardware_type = be16(1);
        arp->protocol_type = be16(0x0800);
        arp->hardware_length = 6;
        arp->protocol_length = 4;
        arp->operation = be16(1); // request
        arp->sender_mac = peer_mac;
        arp->sender_ip = (struct net_ipv4){{10, 0, 2, 2}};
        memset(arp->target_mac.bytes, 0, 6);
        arp->target_ip = (struct net_ipv4){{10, 0, 2, 15}}; // nuestra IP

        transmit_calls = 0;
        int rc = net_runtime_receive(&runtime, &device, frame, sizeof(frame));
        assert(rc == 0);
        assert(transmit_calls == 1);
        assert(last_frame_len == sizeof(frame));

        struct net_ethernet_header *reply_eth = (struct net_ethernet_header *)last_frame;
        struct net_arp_ipv4 *reply_arp = (struct net_arp_ipv4 *)(last_frame + sizeof(*reply_eth));

        assert(memcmp(reply_eth->destination.bytes, peer_mac.bytes, 6) == 0);
        assert(memcmp(reply_eth->source.bytes, our_mac.bytes, 6) == 0);
        assert(be16(reply_eth->ethertype) == 0x0806);
        assert(be16(reply_arp->operation) == 2); // reply
        assert(memcmp(reply_arp->sender_mac.bytes, our_mac.bytes, 6) == 0);
        assert(memcmp(reply_arp->sender_ip.bytes, (uint8_t[]){10,0,2,15}, 4) == 0);
        assert(memcmp(reply_arp->target_mac.bytes, peer_mac.bytes, 6) == 0);
        assert(memcmp(reply_arp->target_ip.bytes, (uint8_t[]){10,0,2,2}, 4) == 0);

        printf("OK: ARP request -> ARP reply correcta\n");
    }

    // --- Test 2: ARP request para OTRA IP (no debe responder) ---
    {
        uint8_t frame[sizeof(struct net_ethernet_header) + sizeof(struct net_arp_ipv4)];
        struct net_ethernet_header *eth = (struct net_ethernet_header *)frame;
        struct net_arp_ipv4 *arp = (struct net_arp_ipv4 *)(frame + sizeof(*eth));
        struct net_mac peer_mac = {{0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x01}};

        memset(eth->destination.bytes, 0xff, 6);
        eth->source = peer_mac;
        eth->ethertype = be16(0x0806);
        arp->hardware_type = be16(1);
        arp->protocol_type = be16(0x0800);
        arp->hardware_length = 6;
        arp->protocol_length = 4;
        arp->operation = be16(1);
        arp->sender_mac = peer_mac;
        arp->sender_ip = (struct net_ipv4){{10, 0, 2, 2}};
        arp->target_ip = (struct net_ipv4){{10, 0, 2, 99}}; // NO es nuestra IP

        transmit_calls = 0;
        net_runtime_receive(&runtime, &device, frame, sizeof(frame));
        assert(transmit_calls == 0);
        printf("OK: ARP request para otra IP no genera respuesta\n");
    }

    // --- Test 3: ICMP echo request (ping) ---
    {
        uint8_t frame[sizeof(struct net_ethernet_header) + sizeof(struct net_ipv4_header) +
                      sizeof(struct net_icmp_header) + 4]; // 4 bytes de payload de ejemplo
        struct net_ethernet_header *eth = (struct net_ethernet_header *)frame;
        struct net_ipv4_header *ip = (struct net_ipv4_header *)(frame + sizeof(*eth));
        struct net_icmp_header *icmp = (struct net_icmp_header *)((uint8_t *)ip + sizeof(*ip));
        uint8_t *payload = (uint8_t *)icmp + sizeof(*icmp);
        struct net_mac peer_mac = {{0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x01}};

        eth->destination = our_mac;
        eth->source = peer_mac;
        eth->ethertype = be16(0x0800);

        ip->version_ihl = 0x45;
        ip->tos = 0;
        ip->total_length = be16(sizeof(*ip) + sizeof(*icmp) + 4);
        ip->identification = 0;
        ip->flags_fragment_offset = 0;
        ip->ttl = 64;
        ip->protocol = 1; // ICMP
        ip->checksum = 0;
        ip->source = (struct net_ipv4){{10, 0, 2, 2}};
        ip->destination = (struct net_ipv4){{10, 0, 2, 15}};

        icmp->type = 8; // echo request
        icmp->code = 0;
        icmp->checksum = 0;
        icmp->identifier = be16(0x1234);
        icmp->sequence = be16(1);
        payload[0] = 0xde; payload[1] = 0xad; payload[2] = 0xbe; payload[3] = 0xef;

        transmit_calls = 0;
        int rc = net_runtime_receive(&runtime, &device, frame, sizeof(frame));
        assert(rc == 0);
        assert(transmit_calls == 1);
        assert(last_frame_len == sizeof(frame));

        struct net_ethernet_header *reply_eth = (struct net_ethernet_header *)last_frame;
        struct net_ipv4_header *reply_ip = (struct net_ipv4_header *)(last_frame + sizeof(*reply_eth));
        struct net_icmp_header *reply_icmp = (struct net_icmp_header *)((uint8_t *)reply_ip + sizeof(*reply_ip));
        uint8_t *reply_payload = (uint8_t *)reply_icmp + sizeof(*reply_icmp);

        assert(memcmp(reply_eth->destination.bytes, peer_mac.bytes, 6) == 0);
        assert(memcmp(reply_eth->source.bytes, our_mac.bytes, 6) == 0);
        assert(memcmp(reply_ip->source.bytes, (uint8_t[]){10,0,2,15}, 4) == 0);
        assert(memcmp(reply_ip->destination.bytes, (uint8_t[]){10,0,2,2}, 4) == 0);
        assert(reply_icmp->type == 0); // echo reply
        assert(reply_icmp->identifier == icmp->identifier);
        assert(reply_icmp->sequence == icmp->sequence);
        assert(memcmp(reply_payload, payload, 4) == 0); // payload copiado tal cual

        printf("OK: ICMP echo request -> ICMP echo reply correcta (payload preservado)\n");
    }

    printf("\nTodos los tests pasaron.\n");
    return 0;
}
