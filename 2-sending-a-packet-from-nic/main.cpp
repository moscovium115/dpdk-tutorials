// MIT License
// 
// Copyright (c) 2024 Muhammad Awais Khalid
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <iostream>
#include <thread>
#include <csignal>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

static volatile sig_atomic_t exit_indicator = 0;

void terminate(int signal) 
{
    exit_indicator = 1;
}

void set_eth_hdr(rte_ether_hdr *const eth_hdr){
    eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    const uint8_t src_mac_addr[6] = {0x12, 0x45, 0xAB, 0xCD, 0x78, 0x21};
    memcpy(eth_hdr->src_addr.addr_bytes, src_mac_addr, sizeof(src_mac_addr));

    const uint8_t dst_mac_addr[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xAB, 0x12};
    memcpy(eth_hdr->dst_addr.addr_bytes, dst_mac_addr, sizeof(dst_mac_addr));

}

void set_ipv4_hdr(rte_ipv4_hdr *const ipv4_hdr){

    ipv4_hdr->version = 4;              // Setting IP version as IPv4
    ipv4_hdr->ihl = 5;                  // Setting IP header length = 20 bytes = (5 * 4 Bytes)
    ipv4_hdr->type_of_service = 0;      // Setting DSCP = 0; ECN = 0;
    ipv4_hdr->total_length = rte_cpu_to_be_16(200);       // Setting total IPv4 packet length to 200 bytes. This includes the IPv4 header (20 bytes) as well.
    ipv4_hdr->packet_id = 0;            // Setting identification = 0 as the packet is non-fragmented.
    ipv4_hdr->fragment_offset = 0x0040; // Setting packet as non-fragmented and fragment offset = 0.
    ipv4_hdr->time_to_live = 64;        // Setting Time to live = 64;
    ipv4_hdr->next_proto_id = 17;       // Setting the next protocol as UDP (17).

    const uint8_t src_ip_addr[4] = {1, 2, 3, 4};                
    memcpy(&ipv4_hdr->src_addr, src_ip_addr, sizeof(src_ip_addr));      // Setting source ip address = 1.2.3.4

    const uint8_t dest_ip_addr[4] = {4, 3, 2, 1};
    memcpy(&ipv4_hdr->dst_addr, dest_ip_addr, sizeof(dest_ip_addr));    // Setting destination ip address = 4.3.2.1

    ipv4_hdr->hdr_checksum = 0;
    ipv4_hdr->hdr_checksum = rte_ipv4_cksum(ipv4_hdr);      // Calculating and setting IPv4 checksum in IPv4 header.
}

void send_packet(rte_mbuf *packet, uint16_t port_ids){
    const uint16_t tx_packets = rte_eth_tx_burst(port_ids[0], 0, &packet, 1);
    if (tx_packets == 0) {
        std::cout << "Unable to transmit the packet. " << std::endl;
        rte_pktmbuf_free(packet);   // As the packet is not transmitted, we need to free the memory buffer by our self.
    } else {
        transmitted_packet_count += tx_packets;
        std::cout << "Packet transmitted successfully ... (" << transmitted_packet_count << ")" << std::endl;
    }
}

void set_udp_hdr(rte_udp_hdr *const udp_hdr){
    udp_hdr->dst_port = rte_cpu_to_be_16(5000);     // Setting destination port = 5000;
    udp_hdr->src_port = rte_cpu_to_be_16(10000);    // Setting source port = 10000;
    udp_hdr->dgram_len = rte_cpu_to_be_16(180);     // Setting datagram length = 180;
    udp_hdr->dgram_cksum = 0;                       // Setting checksum = 0;
}

void insert_data_udp(uint8_t *payload){
    memset(payload, 0, 172);
    const char sample_data[] = {"This is a sample data generated by a DPDK application ..."};
    memcpy(payload, sample_data, sizeof(sample_data));

    // Setting the total packet size in our memory buffer.
    // Total packet size = Ethernet header size + IPv4 header size + UDP header size + Payload size.
    packet->data_len = packet->pkt_len = sizeof(rte_ether_hdr) + sizeof(rte_ipv4_hdr) + sizeof(rte_udp_hdr) + 172;
}

int main(int argc, char **argv)
{
    // Setting up signals to catch TERM and INT signal.
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = terminate;
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGINT, &action, nullptr);

    std::cout << "Starting DPDK program ... " << std::endl;

    int32_t return_val = rte_eal_init(argc, argv);
    if (return_val < 0) 
    {
        std::cerr << "Unable to initialize DPDK EAL (Environment Abstraction Layer). Error code: " << rte_errno << std::endl;
        exit(1);
    }

    argc -= return_val;
    argv += return_val;

    uint16_t port_ids[RTE_MAX_ETHPORTS] = {0};
    int16_t id = 0;
    int16_t total_port_count = 0;
    
    // Detecting the available ports (ethernet interfaces) in the system.
    RTE_ETH_FOREACH_DEV(id) {
        port_ids[total_port_count] = id;
        total_port_count++;
        if (total_port_count >= RTE_MAX_ETHPORTS)
        {
            std::cerr << "Total number of detected ports exceeds RTE_MAX_ETHPORTS. " << std::endl;
            rte_eal_cleanup();
            exit(1);
        }
    }

    if (total_port_count == 0) {
        std::cerr << "No ports detected in the system. " << std::endl;
        rte_eal_cleanup();
        return 1;
    }

    std::cout << "Total ports detected: " << total_port_count << std::endl;

    rte_mempool *memory_pool = rte_pktmbuf_pool_create("mempool_1", 1023, 512, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    const uint16_t rx_queues = 0;
    const uint16_t tx_queues = 1;

    rte_eth_conf portConf = {
        .rxmode = {
            .mq_mode = RTE_ETH_MQ_RX_NONE
        },
        .txmode = {
            .mq_mode = RTE_ETH_MQ_TX_NONE
        }
    };

    // Configure the port (ethernet interface).
     rte_eth_dev_configure(port_ids[0], rx_queues, tx_queues, &portConf);


    const int16_t portSocketId = rte_eth_dev_socket_id(port_ids[0]);
    const int16_t coreSocketId = rte_socket_id();

    // Configure the Rx queue(s) of the port.
 
    rte_eth_rx_queue_setup(port_ids[0], i, 256, ((portSocketId >= 0) ? portSocketId : coreSocketId), nullptr, memory_pool);
    rte_eth_tx_queue_setup(port_ids[0], i, 256, ((portSocketId >= 0) ? portSocketId : coreSocketId), nullptr);
        
    // All the configuration is done. Finally starting the port (ethernet interface) so that we can start transmitting the packets.
    rte_eth_dev_start(port_ids[0]);


    std::cout << "Port configuration successful. Port Id: " << port_ids[0] << std::endl;

    std::cout << "Starting packet tranmission on the ethernet port ... " << std::endl;

    uint64_t transmitted_packet_count = 0;

    // Now we go into a loop to continously transmit the packets on the port (ethernet interface).
    while (!exit_indicator) {

        rte_mbuf *packet = nullptr;
        if (rte_mempool_get(memory_pool, reinterpret_cast<void **>(&packet)) != 0) {
            std::cout << "Error: Unable to get memory buffer from memory pool. " << std::endl;
            using namespace std::literals;
            std::this_thread::sleep_for(100ms);
            continue;
        }

        uint8_t *data = rte_pktmbuf_mtod(packet, uint8_t *);
        // Setting Ethernet header information.
        rte_ether_hdr *const eth_hdr = reinterpret_cast<rte_ether_hdr *>(data);
        set_eth_hdr(eth_hdr);

        // Setting IPv4 header information.
        rte_ipv4_hdr *const ipv4_hdr = reinterpret_cast<rte_ipv4_hdr *>(data + sizeof(rte_ether_hdr));
        set_ipv4_hdr(ipv4_hdr);

        // Setting UDP header information.
        rte_udp_hdr *const udp_hdr = reinterpret_cast<rte_udp_hdr *>(data + sizeof(rte_ether_hdr) + sizeof(rte_ipv4_hdr));
        set_udp_hdr(udp_hdr);

        // Setting data in the UDP payload
        uint8_t *payload = data + sizeof(rte_ether_hdr) + sizeof(rte_ipv4_hdr) + sizeof(rte_udp_hdr);
        insert_data_udp(payload);

        // Now our packet is finally prepared. We will now send it using the DPDK API.
        send_packet(packet, port_ids);
        using namespace std::literals;
        std::this_thread::sleep_for(200ms);
    }

    std::cout << "Exiting DPDK program ... " << std::endl;
    rte_eal_cleanup();
    return 0;
}