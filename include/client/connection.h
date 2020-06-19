#ifndef _CLIENT_CONNECTION_H_
#define _CLIENT_CONNECTION_H_

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/network.h"
#include "client/cerver.h"
#include "client/handler.h"
#include "client/packets.h"

// used for connection with exponential backoff (secs)
#define DEFAULT_CONNECTION_MAX_SLEEP                60 
#define DEFAULT_CONNECTION_PROTOCOL                 PROTOCOL_TCP

struct _Cerver;
struct _Client;
struct _Packet;
struct _PacketsPerType;
struct _SockReceive;

struct _ConnectionStats {
    
    time_t connection_threshold_time;       // every time we want to reset the connection's stats
    u64 n_receives_done;                    // total amount of actual calls to recv ()
    u64 total_bytes_received;               // total amount of bytes received from this connection
    u64 total_bytes_sent;                   // total amount of bytes that have been sent to the connection
    u64 n_packets_received;                 // total number of packets received from this connection (packet header + data)
    u64 n_packets_sent;                     // total number of packets sent to this connection

    struct _PacketsPerType *received_packets;
    struct _PacketsPerType *sent_packets;

};

typedef struct _ConnectionStats ConnectionStats;

struct _Connection {

    String *name;

    i32 sock_fd;
    u16 port; 
    Protocol protocol;
    bool use_ipv6;  

    String *ip;                         // cerver's ip
    struct sockaddr_storage address;    // cerver's address

    time_t connected_timestamp;         // when the connection started

    u32 max_sleep;
    bool connected;                     // is the socket connected?

    // info about the cerver we are connected to
    struct _Cerver *cerver;

    u32 receive_packet_buffer_size;         // 01/01/2020 - read packets into a buffer of this size in client_receive ()
    struct _SockReceive *sock_receive;      // 01/01/2020 - used for inter-cerver communications

    // 10/06/2020 - used for direct requests to cerver
    bool full_packet;

    // 01/01/2020 - a place to safely store the request response, like when using client_connection_request_to_cerver ()
    void *received_data;                    
    size_t received_data_size;
    Action received_data_delete;

    bool receive_packets;                   // set if the connection will receive packets or not (default true)
    Action custom_receive;                  // custom receive method to handle incomming packets in the connection
    void *custom_receive_args;              // arguments to be passed to the custom receive method

    void *auth_data;                        // maybe auth credentials
    size_t auth_data_size;
    Action delete_auth_data;                // destroys the auth data when the connection ends
    struct _Packet *auth_packet;

    ConnectionStats *stats;

};

typedef struct _Connection Connection;

extern Connection *connection_new (void);

extern void connection_delete (void *ptr);

extern Connection *connection_create_empty (void);

// compares two connections by their names
extern int connection_comparator_by_name (const void *a, const void *b);

// compare two connections by their socket fds
extern int connection_comparator_by_sock_fd (const void *a, const void *b);

// sets the connection's name, if it had a name before, it will be replaced
extern void connection_set_name (Connection *connection, const char *name);

// sets the connection max sleep (wait time) to try to connect to the cerver
extern void connection_set_max_sleep (Connection *connection, u32 max_sleep);

// read packets into a buffer of this size in client_receive ()
// by default the value RECEIVE_PACKET_BUFFER_SIZE is used
extern void connection_set_receive_buffer_size (Connection *connection, u32 size);

// sets the connection received data
// 01/01/2020 - a place to safely store the request response, like when using client_connection_request_to_cerver ()
extern void connection_set_received_data (Connection *connection, void *data, size_t data_size, Action data_delete);

// sets a custom receive method to handle incomming packets in the connection
// a reference to the client and connection will be passed to the action as ClientConnection structure
extern void connection_set_custom_receive (Connection *connection, Action custom_receive, void *args);

// sets the connection auth data to send whenever the cerver requires authentication 
// and a method to destroy it once the connection has ended
extern void connection_set_auth_data (Connection *connection, void *auth_data, size_t auth_data_size, Action delete_auth_data);

// removes the connection auth data and its destroy method
extern void connection_remove_auth_data (Connection *connection);

// generates the connection auth packet to be send to the server
// this is also generated automatically whenever the cerver ask for authentication
extern void connection_generate_auth_packet (Connection *connection);

// creates a new connection that is ready to be started
// returns a newly allocated connection on success, NULL if any initial setup has failed
extern Connection *connection_create (const char *ip_address, u16 port, Protocol protocol, bool use_ipv6);

// starts a connection -> connects to the specified ip and port
// returns 0 on success, 1 on error
extern int connection_start (Connection *connection);

typedef struct ConnectionCustomReceiveData {

    struct _Client *client;
    struct _Connection *connection;
    void *args;

} ConnectionCustomReceiveData;

// starts listening and receiving data in the connection sock
extern void connection_update (void *ptr);

// closes a connection directly
extern void connection_close (Connection *connection);

#endif