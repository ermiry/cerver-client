#ifndef _CLIENT_HANDLER_H_
#define _CLIENT_HANDLER_H_

#include "client/config.h"
#include "client/client.h"
#include "client/connection.h"
#include "client/packets.h"

#define RECEIVE_PACKET_BUFFER_SIZE          8192

struct _Client;
struct _Connection;
struct _Packet;

struct _SockReceive {

	struct _Packet *spare_packet;
	size_t missing_packet;

	void *header;
	char *header_end;
	// unsigned int curr_header_pos;
	unsigned int remaining_header;
	bool complete_header;

};

typedef struct _SockReceive SockReceive;

CLIENT_PRIVATE SockReceive *sock_receive_new (void);

CLIENT_PRIVATE void sock_receive_delete (void *sock_receive_ptr);

// receive data from connection's socket
// this method does not perform any checks and expects a valid buffer
// to handle incomming data
// returns 0 on success, 1 on error
CLIENT_PRIVATE unsigned int client_receive_internal (
	struct _Client *client, struct _Connection *connection,
	char *buffer, const size_t buffer_size
);

// allocates a new packet buffer to receive incoming data from the connection's socket
// returns 0 on success handle, 1 if any error ocurred and must likely the connection was ended
CLIENT_PUBLIC unsigned int client_receive (struct _Client *client, struct _Connection *connection);

#endif