#ifndef _CLIENT_HANDLER_H_
#define _CLIENT_HANDLER_H_

#include "client/config.h"
#include "client/client.h"
#include "client/connection.h"
#include "client/packets.h"

#define RECEIVE_PACKET_BUFFER_SIZE          8192

#ifdef __cplusplus
extern "C" {
#endif

struct _Client;
struct _Connection;
struct _Packet;

#define CLIENT_HANDLER_ERROR_MAP(XX)										\
	XX(0,	NONE,		None,				No handler error)				\
	XX(1,	PACKET,		Bad Packet,			Packet check failed)			\
	XX(2,	CLOSED,		Closed Connection, 	The connection has been ended)

typedef enum ClientHandlerError {

	#define XX(num, name, string, description) CLIENT_HANDLER_ERROR_##name = num,
	CLIENT_HANDLER_ERROR_MAP (XX)
	#undef XX

} ClientHandlerError;

CLIENT_PUBLIC const char *client_handler_error_to_string (
	const ClientHandlerError error
);

CLIENT_PUBLIC const char *client_handler_error_description (
	const ClientHandlerError error
);

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
CLIENT_PUBLIC unsigned int client_receive (
	struct _Client *client, struct _Connection *connection
);

#ifdef __cplusplus
}
#endif

#endif