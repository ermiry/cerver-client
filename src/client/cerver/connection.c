#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/cerver/network.h"
#include "client/cerver/cerver.h"
#include "client/cerver/client.h"
#include "client/cerver/connection.h"
#include "client/cerver/handler.h"
#include "client/cerver/packets.h"

#include "client/threads/thread.h"

#include "client/utils/utils.h"
#include "client/utils/log.h"

void connection_remove_auth_data (Connection *connection);

#pragma region stats

static inline ConnectionStats *connection_stats_new (void) {

    ConnectionStats *stats = (ConnectionStats *) malloc (sizeof (ConnectionStats));
    if (stats) {
        memset (stats, 0, sizeof (ConnectionStats));
        stats->received_packets = NULL;
        stats->sent_packets = NULL;
    } 

    return stats;

}

static inline void connection_stats_delete (ConnectionStats *stats) { 
    
    if (stats) {
        packets_per_type_delete (stats->received_packets);
        packets_per_type_delete (stats->sent_packets);

        free (stats); 
    } 
    
}

static ConnectionStats *connection_stats_create (void) {

    ConnectionStats *stats = connection_stats_new ();
    if (stats) {
        stats->received_packets = packets_per_type_new ();
        stats->sent_packets = packets_per_type_new ();
    }

    return stats;

}

#pragma endregion

Connection *connection_new (void) {

    Connection *connection = (Connection *) malloc (sizeof (Connection));
    if (connection) {
        memset (connection, 0, sizeof (Connection));

        connection->name = NULL;

        connection->use_ipv6 = false;
        connection->protocol = DEFAULT_CONNECTION_PROTOCOL;

        connection->ip = NULL;
        memset (&connection->address, 0, sizeof (struct sockaddr_storage));

        connection->max_sleep = DEFAULT_CONNECTION_MAX_SLEEP;
        connection->connected = false;

        connection->cerver = NULL;

        connection->receive_packet_buffer_size = RECEIVE_PACKET_BUFFER_SIZE;
        connection->sock_receive = NULL;

        connection->full_packet = false;

        connection->received_data = NULL;
        connection->received_data_delete = NULL;

        connection->receive_packets = true;
        connection->custom_receive = NULL;
        connection->custom_receive_args = NULL;

        connection->auth_data = NULL;
        connection->delete_auth_data = NULL;
        connection->auth_packet = NULL;

        connection->stats = NULL;
    }

    return connection;

}

void connection_delete (void *connection_ptr) {

    if (connection_ptr) {
        Connection *connection = (Connection *) connection_ptr;

        str_delete (connection->name);

        str_delete (connection->ip);

        cerver_delete (connection->cerver);
        
        sock_receive_delete (connection->sock_receive);

        if (connection->received_data && connection->received_data_delete)
            connection->received_data_delete (connection->received_data);

        connection_remove_auth_data (connection);

        connection_stats_delete (connection->stats);

        free (connection);
    }

}

Connection *connection_create_empty (void) {

    Connection *connection = connection_new ();
    if (connection) {
        connection->sock_receive = sock_receive_new ();
        connection->stats = connection_stats_create ();
    }

    return connection;

}

// compares two connections by their names
int connection_comparator_by_name (void *one, void *two) {

    return str_compare (((Connection *) one)->name, ((Connection *) two)->name);

}

// compare two connections by their socket fds
int connection_comparator_by_sock_fd (const void *a, const void *b) {

    if (a && b) {
        Connection *con_a = (Connection *) a;
        Connection *con_b = (Connection *) b;

        if (con_a->sock_fd < con_b->sock_fd) return -1;
        else if (con_a->sock_fd == con_b->sock_fd) return 0;
        else return 1; 
    }

    return 0;

}

// sets the connection's name, if it had a name before, it will be replaced
void connection_set_name (Connection *connection, const char *name) {

    if (connection) {
        if (connection->name) str_delete (connection->name);
        connection->name = name ? str_new (name) : NULL;
    }

}

// sets the connection max sleep (wait time) to try to connect to the cerver
void connection_set_max_sleep (Connection *connection, u32 max_sleep) {

    if (connection) connection->max_sleep = max_sleep;

}

// read packets into a buffer of this size in client_receive ()
// by default the value RECEIVE_PACKET_BUFFER_SIZE is used
void connection_set_receive_buffer_size (Connection *connection, u32 size) {

    if (connection) connection->receive_packet_buffer_size = size;

}

// sets the connection received data
// 01/01/2020 - a place to safely store the request response, like when using client_connection_request_to_cerver ()
void connection_set_received_data (Connection *connection, void *data, size_t data_size, Action data_delete) {

    if (connection) {
        connection->received_data = data;
        connection->received_data_size = data_size;
        connection->received_data_delete = data_delete;
    }

}

// sets a custom receive method to handle incomming packets in the connection
// a reference to the client and connection will be passed to the action as ClientConnection structure
void connection_set_custom_receive (Connection *connection, Action custom_receive, void *args) {

    if (connection) {
        connection->custom_receive = custom_receive;
        connection->custom_receive_args = args;
        if (connection->custom_receive) connection->receive_packets = true;
    }

}

// sets the connection auth data and a method to destroy it once the connection has ended
void connection_set_auth_data (Connection *connection, void *auth_data, size_t auth_data_size, Action delete_auth_data) {

    if (connection && auth_data) {
        connection_remove_auth_data (connection);

        connection->auth_data = auth_data;
        connection->delete_auth_data = delete_auth_data;
    } 

}

// removes the connection auth data and its destroy method
void connection_remove_auth_data (Connection *connection) {

    if (connection) {
        if (connection->delete_auth_data) 
            connection->delete_auth_data (connection->auth_data);
        else free (connection->auth_data);

        connection->delete_auth_data = NULL;
        connection->auth_data = NULL;

        if (connection->auth_packet) {
            packet_delete (connection->auth_packet);
            connection->auth_packet = NULL;
        }
    }

}

// generates the connection auth packet to be send to the server
// this is also generated automatically whenever the cerver ask for authentication
void connection_generate_auth_packet (Connection *connection) {

    if (connection) {
        if (connection->auth_data) {
            connection->auth_packet = packet_generate_request (AUTH_PACKET, CLIENT_AUTH_DATA, 
                connection->auth_data, connection->auth_data_size);
        }
    }

}

// sets up the new connection values
static u8 connection_init (Connection *connection) {

    u8 retval = 1;

    if (connection) {
        switch (connection->protocol) {
            case IPPROTO_TCP: 
                connection->sock_fd = socket ((connection->use_ipv6 == 1 ? AF_INET6 : AF_INET), SOCK_STREAM, 0);
                break;
            case IPPROTO_UDP:
                connection->sock_fd = socket ((connection->use_ipv6 == 1 ? AF_INET6 : AF_INET), SOCK_DGRAM, 0);
                break;

            default: 
                client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Unkonw protocol type!"); 
                return 1;
        }

        if (connection->sock_fd > 0) {
            if (connection->use_ipv6) {
                struct sockaddr_in6 *addr = (struct sockaddr_in6 *) &connection->address;
                addr->sin6_family = AF_INET6;
                addr->sin6_addr = in6addr_any;
                addr->sin6_port = htons (connection->port);
            } 

            else {
                struct sockaddr_in *addr = (struct sockaddr_in *) &connection->address;
                addr->sin_family = AF_INET;
                addr->sin_addr.s_addr = inet_addr (connection->ip->str);
                addr->sin_port = htons (connection->port);
            }

            retval = 0;     // connection setup was successfull
        }

        else {
            client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to create new socket!");
        }
    }

    return retval;

}

// creates a new connection that is ready to be started
// returns a newly allocated connection on success, NULL if any initial setup has failed
Connection *connection_create (const char *ip_address, u16 port, Protocol protocol, bool use_ipv6) {

    Connection *connection = NULL;

    if (ip_address) {
        connection = connection_create_empty ();
        if (connection) {
            connection->ip = str_new (ip_address);

            connection->port = port;
            connection->protocol = protocol;
            connection->use_ipv6 = use_ipv6;

            connection->connected = false;

            // set up the new connection to be ready to be started
            if (connection_init (connection)) {
                #ifdef CLIENT_DEBUG
                client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to init the new connection!");
                #endif
                connection_delete (connection);
                connection = NULL;
            }
        }
    }

    return connection;

}

// try to connect a client to an address (server) with exponential backoff
static u8 connection_try (Connection *connection, const struct sockaddr_storage address) {

    i32 numsec;
    for (numsec = 2; numsec <= connection->max_sleep; numsec <<= 1) {
        if (!connect (connection->sock_fd, 
            (const struct sockaddr *) &address, 
            sizeof (struct sockaddr))) 
            return 0;

        if (numsec <= connection->max_sleep / 2) sleep (numsec);
    } 

    return 1;

}

// starts a connection -> connects to the specified ip and port
// returns 0 on success, 1 on error
int connection_start (Connection *connection) {

    return (connection ? connection_try (connection, connection->address) : 1);

}

static ConnectionCustomReceiveData *connection_custom_receive_data_new (Client *client, Connection *connection, void *args) {

    ConnectionCustomReceiveData *custom_data = (ConnectionCustomReceiveData *) malloc (sizeof (ConnectionCustomReceiveData));
    if (custom_data) {
        custom_data->client = client;
        custom_data->connection = connection;
        custom_data->args = args;
    }

    return custom_data;

}

static inline void connection_custom_receive_data_delete (void *custom_data_ptr) {

    if (custom_data_ptr) free (custom_data_ptr);

}

// starts listening and receiving data in the connection sock
void connection_update (void *ptr) {

    if (ptr) {
        ClientConnection *cc = (ClientConnection *) ptr;
        
        if (cc->connection->name) {
            char *s = c_string_create ("connection-%s", cc->connection->name->str);
            if (s) {
                thread_set_name (s);
                free (s);
            }
        }

        ConnectionCustomReceiveData *custom_data = connection_custom_receive_data_new (cc->client, cc->connection, 
            cc->connection->custom_receive_args);

        cc->connection->sock_receive = sock_receive_new ();

        while (cc->client->running && cc->connection->connected) {
            if (cc->connection->custom_receive) {
                // if a custom receive method is set, use that one directly
                cc->connection->custom_receive (custom_data);
            } 

            else {
                // use the default receive method that expects cerver type packages
                client_receive (cc->client, cc->connection);
            }
        }

        connection_custom_receive_data_delete (custom_data);
        client_connection_aux_delete (cc);
    }

}

// ends a connection
void connection_end (Connection *connection) {

    if (connection) {
        if (connection->connected) {
            // if we are connected to a cerver, send a disconnect packet
            if (connection->cerver) {
                Packet *packet = packet_generate_request (REQUEST_PACKET, CLIENT_CLOSE_CONNECTION, NULL, 0);
                if (packet) {
                    packet_set_network_values (packet, NULL, connection);
                    packet_send (packet, 0, NULL, false);
                    packet_delete (packet);
                }
            }

            close (connection->sock_fd);
            // connection->sock_fd = -1;
            connection->connected = false;
        }
    }

}

// closes a connection directly
void connection_close (Connection *connection) {

    if (connection) {
        if (connection->connected) {
            close (connection->sock_fd);
            // connection->sock_fd = -1;
            connection->connected = false;
        }
    }

}