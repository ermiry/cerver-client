#ifndef _CLIENT_HANDLER_H_
#define _CLIENT_HANDLER_H_

#include "client/config.h"
#include "client/packets.h"
#include "client/receive.h"

#include "client/threads/jobs.h"

#define RECEIVE_PACKET_BUFFER_SIZE          8192

#ifdef __cplusplus
extern "C" {
#endif

struct _Client;
struct _Connection;
struct _Packet;

typedef enum HandlerType {

	HANDLER_TYPE_NONE         = 0,

	HANDLER_TYPE_CERVER       = 1,
	HANDLER_TYPE_CLIENT       = 2,
	HANDLER_TYPE_ADMIN        = 3,

} HandlerType;

// the strcuture that will be passed to the handler
typedef struct HandlerData {

	int handler_id;

	void *data;                     // handler's own data
	struct _Packet *packet;         // the packet to handle

} HandlerData;

struct _Handler {

	HandlerType type;

	// added every time a new handler gets created
	int unique_id;

	int id;
	pthread_t thread_id;

	// unique handler data
	// will be passed to jobs alongside any job specific data as the args
	void *data;

	// must return a newly allocated handler unique data
	// will be executed when the handler starts
	void *(*data_create) (void *args);
	void *data_create_args;

	// called at the end of the handler to delete the handler's data
	// if no method is set, it won't be deleted
	Action data_delete;

	// the method that this handler will execute to handle packets
	Action handler;

	// used to avoid pushing job to the queue and instead handle
	// the packet directly in the same thread
	// this option is set to false as default
	// pros - inmediate handle with no delays
	//      - handler method can be called from multiple threads
	// neutral - data create and delete will be executed every time
	// cons - calling thread will be busy until handler method is done
	bool direct_handle;

	// the jobs (packets) that are waiting to be handled
	// passed as args to the handler method
	JobQueue *job_queue;

	struct _Cerver *cerver;     // the cerver this handler belongs to
	struct _Client *client;     // the client this handler belongs to

};

typedef struct _Handler Handler;

CLIENT_PRIVATE void handler_delete (void *handler_ptr);

// creates a new handler
// handler method is your actual app packet handler
CLIENT_EXPORT Handler *handler_create (Action handler_method);

// creates a new handler that will be used for
// cerver's multiple app handlers configuration
// it should be registered to the cerver before it starts
// the user is responsible for setting the unique id,
// which will be used to match
// incoming packets
// handler method is your actual app packet handler
CLIENT_EXPORT Handler *handler_create_with_id (
	int id, Action handler_method
);

// sets the handler's data directly
// this data will be passed to the handler method
// using a HandlerData structure
CLIENT_EXPORT void handler_set_data (
	Handler *handler, void *data
);

// set a method to create the handler's data before it starts handling any packet
// this data will be passed to the handler method using a HandlerData structure
CLIENT_EXPORT void handler_set_data_create (
	Handler *handler,
	void *(*data_create) (void *args), void *data_create_args
);

// set the method to be used to delete the handler's data
CLIENT_EXPORT void handler_set_data_delete (
	Handler *handler, Action data_delete
);

// used to avoid pushing job to the queue and instead handle
// the packet directly in the same thread
// pros     - inmediate handle with no delays
//          - handler method can be called from multiple threads
// neutral  - data create and delete will be executed every time
// cons     - calling thread will be busy until handler method is done
CLIENT_EXPORT void handler_set_direct_handle (
	Handler *handler, bool direct_handle
);

// starts the new handler by creating a dedicated thread for it
// called by internal cerver methods
CLIENT_PRIVATE int handler_start (Handler *handler);

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

CLIENT_PRIVATE u8 client_packet_handler (Packet *packet);

// performs the actual recv () method on the connection's sock fd
// handles if the receive method failed
// the amount of bytes read from the socket is placed in rc
CLIENT_PRIVATE ReceiveError client_receive_actual (
	struct _Client *client, struct _Connection *connection,
	char *buffer, const size_t buffer_size,
	size_t *rc
);

// request to read x amount of bytes from the connection's sock fd
// into the specified buffer
// this method will only return once the requested bytes
// have been received or on any error
CLIENT_PRIVATE ReceiveError client_receive_data (
	struct _Client *client, struct _Connection *connection,
	char *buffer, const size_t buffer_size,
	size_t requested_data
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