#include <uxr/client/transport.h>
#include <rmw_microxrcedds_c/config.h>

#include <stm_networking.h>
#include <microros_transports.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define G_PACKET_POOL0_PACKET_NUM 40
#define G_PACKET_POOL0_PACKET_SIZE 512

NX_UDP_SOCKET socket;
NX_PACKET_POOL g_packet_pool0;

// // Stack memory for nx_ip Packet pool.
uint8_t g_packet_pool0_pool_memory[G_PACKET_POOL0_PACKET_NUM * (G_PACKET_POOL0_PACKET_SIZE + sizeof(NX_PACKET))];

// #define LINK_ENABLE_WAIT_TIME (1000U)
#define SOCKET_FIFO_SIZE G_PACKET_POOL0_PACKET_NUM

// Set TX_TIMER_TICKS_PER_SECOND to 1000 (1 ms tick) in thread conf
#define TX_MS_TO_TICKS(milliseconds) ((ULONG) ((milliseconds / 1000.0) * TX_TIMER_TICKS_PER_SECOND))

bool azure_transport_open(struct uxrCustomTransport * transport){
    (void) transport;

    // // Create the packet pool.
    UINT status = nx_packet_pool_create(&g_packet_pool0,
            "g_packet_pool0 Packet Pool",
            G_PACKET_POOL0_PACKET_SIZE,
            &g_packet_pool0_pool_memory[0],
            G_PACKET_POOL0_PACKET_NUM * (G_PACKET_POOL0_PACKET_SIZE + sizeof(NX_PACKET)));


    // Enable udp.
    status = nx_udp_enable(&nx_ip);

    if(NX_SUCCESS != status)
    {
        return false;
    }

    status = nx_udp_socket_create(&nx_ip, &socket, "Micro socket", NX_IP_NORMAL, NX_DONT_FRAGMENT, NX_IP_TIME_TO_LIVE, SOCKET_FIFO_SIZE);

    if(NX_SUCCESS != status)
    {
        return false;
    }

    // Find first avaliable udp port and bind socket.
    // UINT port;
    // status = nx_udp_free_port_find(&nx_ip, 1, &port);

    if(NX_SUCCESS != status)
    {
        return false;
    }

    status = nx_udp_socket_bind(&socket, 9999, TX_NO_WAIT);

    if(NX_SUCCESS != status)
    {
        return false;
    }

    return true;
}

bool azure_transport_close(struct uxrCustomTransport * transport){
    (void) transport;

    UINT status = nx_udp_socket_unbind(&socket);

    if(NX_SUCCESS != status)
    {
        return false;
    }

    status = nx_udp_socket_delete(&socket);

    if(NX_SUCCESS != status)
    {
        return false;
    }

    status = nx_packet_pool_delete(&g_packet_pool0);

    if(NX_SUCCESS != status)
    {
        return false;
    }

    return true;
}

size_t azure_transport_write(struct uxrCustomTransport* transport, const uint8_t * buf, size_t len, uint8_t * err){
    custom_transport_args * args = (custom_transport_args*) transport->args;

    volatile int result = 1;
    NX_PACKET *data_packet;

    // Get free packet from g_packet_pool0
    result = nx_packet_allocate(&g_packet_pool0, &data_packet, NX_UDP_PACKET, NX_NO_WAIT);

    if (result != NX_SUCCESS)
    {
        *err = (uint8_t) result;
        return 0;
    }

    result = nx_packet_data_append(data_packet, (VOID *)buf, len, &g_packet_pool0, NX_NO_WAIT);

    if (result != NX_SUCCESS)
    {
        *err = (uint8_t) result;
        return 0;
    }

    result = nx_udp_socket_send(&socket, data_packet, args->agent_ip_address, args->agent_port);

    if (result != NX_SUCCESS)
    {
        *err = (uint8_t) result;
        return 0;
    }

    return len;
}

size_t azure_transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err){
    (void) transport;
    (void) len;

    NX_PACKET *data_packet = NULL;
    ULONG bytes_read = 0;

    UINT result = nx_udp_socket_receive(&socket, &data_packet, TX_MS_TO_TICKS(timeout));

    if (result != NX_SUCCESS)
    {
        *err = (uint8_t) result;
        return 0;
    }

    // Retrieve the data from the packet
    result = nx_packet_data_retrieve(data_packet, buf, &bytes_read);

    // Release the packet
    nx_packet_release(data_packet);

    if(NX_SUCCESS != result)
    {
        *err = (uint8_t) result;
        return 0;
    }

    return bytes_read;
}
