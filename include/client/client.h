#ifndef _CLIENT_CLIENT_H_
#define _CLIENT_CLIENT_H_

#include <stdbool.h>

#include <time.h>

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/collections/dlist.h"

#include "client/cerver.h"
#include "client/config.h"
#include "client/connection.h"
#include "client/network.h"
#include "client/packets.h"
#include "client/handler.h"

#include "client/utils/log.h"

#define CLIENT_NAME_SIZE						64
#define CLIENT_FILES_MAX_PATHS           		32

#define CLIENT_DEFAULT_NAME						"no-name"

#ifdef __cplusplus
extern "C" {
#endif

struct _Cerver;
struct _Client;
struct _Connection;
struct _Packet;
struct _PacketsPerType;
struct _Handler;

struct _FileHeader;

struct _ClientEvent;
struct _ClientError;

#pragma region stats

struct _ClientStats {

	time_t threshold_time;			// every time we want to reset the client's stats

	u64 n_receives_done;			// n calls to recv ()

	u64 total_bytes_received;		// total amount of bytes received from this client
	u64 total_bytes_sent;			// total amount of bytes that have been sent to the client (all of its connections)

	u64 n_packets_received;			// total number of packets received from this client (packet header + data)
	u64 n_packets_sent;				// total number of packets sent to this client (all connections)

	struct _PacketsPerType *received_packets;
	struct _PacketsPerType *sent_packets;

};

typedef struct _ClientStats ClientStats;

CLIENT_PUBLIC void client_stats_print (
	struct _Client *client
);

struct _ClientFileStats {

	u64 n_files_requests;				// n requests to get a file
	u64 n_success_files_requests;		// fulfilled requests
	u64 n_bad_files_requests;			// bad requests
	u64 n_files_sent;					// n files sent
	u64 n_bad_files_sent;				// n files that failed to send
	u64 n_bytes_sent;					// total bytes sent

	u64 n_files_upload_requests;		// n requests to upload a file
	u64 n_success_files_uploaded;		// n files received
	u64 n_bad_files_upload_requests;	// bad requests to upload files
	u64 n_bad_files_received;			// files that failed to be received
	u64 n_bytes_received;				// total bytes received

};

typedef struct _ClientFileStats ClientFileStats;

CLIENT_PUBLIC void client_file_stats_print (
	struct _Client *client
);

#pragma endregion

#pragma region main

#define CLIENT_MAX_EVENTS				32
#define CLIENT_MAX_ERRORS				32

#define CLIENT_CONNECTIONS_STATUS_MAP(XX)									\
	XX(0,	NONE,		None, 		Undefined)								\
	XX(1,	ERROR,		Error, 		Failed to remove connection)			\
	XX(2,	ONE,		One,		At least one active connection)			\
	XX(3,	DROPPED,	Dropped,	Removed due to no connections left)

typedef enum ClientConnectionsStatus {

	#define XX(num, name, string, description) CLIENT_CONNECTIONS_STATUS_##name = num,
	CLIENT_CONNECTIONS_STATUS_MAP (XX)
	#undef XX

} ClientConnectionsStatus;

CLIENT_PUBLIC const char *client_connections_status_to_string (
	const ClientConnectionsStatus status
);

CLIENT_PUBLIC const char *client_connections_status_description (
	const ClientConnectionsStatus status
);

// anyone that connects to the cerver
struct _Client {

	// generated using connection values
	u64 id;
	time_t connected_timestamp;

	char name[CLIENT_NAME_SIZE];

	DoubleList *connections;

	// multiple connections can be associated with the same client using the same session id
	String *session_id;

	time_t last_activity;	// the last time the client sent / receive data

	bool drop_client;		// client failed to authenticate

	void *data;
	Action delete_data;

	// used when the client connects to another server
	bool running;
	time_t time_started;
	u64 uptime;

	// custom packet handlers
	unsigned int num_handlers_alive;       // handlers currently alive
	unsigned int num_handlers_working;     // handlers currently working
	pthread_mutex_t *handlers_lock;
	struct _Handler *app_packet_handler;
	struct _Handler *app_error_packet_handler;
	struct _Handler *custom_packet_handler;

	bool check_packets;              // enable / disbale packet checking

	// general client lock
	pthread_mutex_t *lock;

	struct _ClientEvent *events[CLIENT_MAX_EVENTS];
	struct _ClientError *errors[CLIENT_MAX_ERRORS];

	// files
	unsigned int n_paths;
	String *paths[CLIENT_FILES_MAX_PATHS];

	// default path where received files will be placed
	String *uploads_path;

	u8 (*file_upload_handler) (
		struct _Client *, struct _Connection *,
		struct _FileHeader *,
		const char *file_data, size_t file_data_len,
		char **saved_filename
	);

	void (*file_upload_cb) (
		struct _Client *, struct _Connection *,
		const char *saved_filename
	);

	ClientFileStats *file_stats;

	ClientStats *stats;

};

typedef struct _Client Client;

CLIENT_PUBLIC Client *client_new (void);

// completely deletes a client and all of its data
CLIENT_PUBLIC void client_delete (void *ptr);

// used in data structures that require a delete function
// but the client needs to stay alive
CLIENT_PUBLIC void client_delete_dummy (void *ptr);

// creates a new client and inits its values
CLIENT_PUBLIC Client *client_create (void);

// creates a new client and registers a new connection
CLIENT_PUBLIC Client *client_create_with_connection (
	struct _Cerver *cerver,
	const i32 sock_fd, const struct sockaddr_storage *address
);

// sets the client's name
CLIENT_EXPORT void client_set_name (
	Client *client, const char *name
);

// sets the client's session id
CLIENT_PUBLIC u8 client_set_session_id (
	Client *client, const char *session_id
);

// returns the client's app data
CLIENT_EXPORT void *client_get_data (Client *client);

// sets client's data and a way to destroy it
// deletes the previous data of the client
CLIENT_EXPORT void client_set_data (
	Client *client, void *data, Action delete_data
);

// sets customs PACKET_TYPE_APP and PACKET_TYPE_APP_ERROR packet types handlers
CLIENT_EXPORT void client_set_app_handlers (
	Client *client,
	struct _Handler *app_handler, struct _Handler *app_error_handler
);

// sets a PACKET_TYPE_CUSTOM packet type handler
CLIENT_EXPORT void client_set_custom_handler (
	Client *client, struct _Handler *custom_handler
);

// set whether to check or not incoming packets
// check packet's header protocol id & version compatibility
// if packets do not pass the checks, won't be handled and will be inmediately destroyed
// packets size must be cheked in individual methods (handlers)
// by default, this option is turned off
CLIENT_EXPORT void client_set_check_packets (
	Client *client, bool check_packets
);

// compare clients based on their client ids
CLIENT_PUBLIC int client_comparator_client_id (
	const void *a, const void *b
);

// compare clients based on their session ids
CLIENT_PUBLIC int client_comparator_session_id (
	const void *a, const void *b
);

// closes all client connections
// returns 0 on success, 1 on error
CLIENT_EXPORT u8 client_disconnect (Client *client);

// the client got disconnected from the cerver, so correctly clear our data
CLIENT_EXPORT void client_got_disconnected (Client *client);

// adds a new connection to the end of the client to the client's connection list
// without adding it to any other structure
// returns 0 on success, 1 on error
CLIENT_EXPORT u8 client_connection_add (
	Client *client, struct _Connection *connection
);

// removes the connection from the client
// returns 0 on success, 1 on error
CLIENT_EXPORT u8 client_connection_remove (
	Client *client, struct _Connection *connection
);

#pragma endregion

#pragma region client

/*** Use these to connect/disconnect a client to/from another server ***/

typedef struct ClientConnection {

	pthread_t connection_thread_id;
	struct _Client *client;
	struct _Connection *connection;

} ClientConnection;

CLIENT_PRIVATE void client_connection_aux_delete (
	ClientConnection *cc
);

// creates a new connection that is ready to connect and registers it to the client
CLIENT_EXPORT struct _Connection *client_connection_create (
	Client *client,
	const char *ip_address, u16 port,
	Protocol protocol, bool use_ipv6
);

// registers an existing connection to a client
// retuns 0 on success, 1 on error
CLIENT_EXPORT int client_connection_register (
	Client *client, struct _Connection *connection
);

// unregister an exitsing connection from the client
// returns 0 on success, 1 on error or if the connection does not belong to the client
CLIENT_EXPORT int client_connection_unregister (
	Client *client, struct _Connection *connection
);

// performs a receive in the connection's socket
// to get a complete packet & handle it
// returns 0 on success, 1 on error
CLIENT_PUBLIC unsigned int client_connection_get_next_packet (
	Client *client, struct _Connection *connection
);

/*** connect ***/

// connects a client to the host with the specified values in the connection
// it can be a cerver or not
// this is a blocking method, as it will wait until the connection has been successfull or a timeout
// user must manually handle how he wants to receive / handle incomming packets and also send requests
// returns 0 when the connection has been established, 1 on error or failed to connect
CLIENT_EXPORT unsigned int client_connect (
	Client *client, struct _Connection *connection
);

// connects a client to the host with the specified values in the connection
// performs a first read to get the cerver info packet
// this is a blocking method, and works exactly the same as if only calling client_connect ()
// returns 0 when the connection has been established, 1 on error or failed to connect
CLIENT_EXPORT unsigned int client_connect_to_cerver (
	Client *client, struct _Connection *connection
);

// connects a client to the host with the specified values in the connection
// it can be a cerver or not
// this is NOT a blocking method, a new thread will be created to wait for a connection to be established
// user must manually handle how he wants to receive / handle incomming packets and also send requests
// returns 0 on success connection thread creation, 1 on error
CLIENT_EXPORT unsigned int client_connect_async (
	Client *client, struct _Connection *connection
);

/*** start ***/

// after a client connection successfully connects to a server,
// it will start the connection's update thread to enable the connection to
// receive & handle packets in a dedicated thread
// returns 0 on success, 1 on error
CLIENT_EXPORT int client_connection_start (
	Client *client, struct _Connection *connection
);

// connects a client connection to a server
// and after a success connection, it will start the connection (create update thread for receiving messages)
// this is a blocking method, returns only after a success or failed connection
// returns 0 on success, 1 on error
CLIENT_EXPORT int client_connect_and_start (
	Client *client, struct _Connection *connection
);

// connects a client connection to a server in a new thread to avoid blocking the calling thread,
// and after a success connection, it will start the connection (create update thread for receiving messages)
// returns 0 on success creating connection thread, 1 on error
CLIENT_EXPORT u8 client_connect_and_start_async (
	Client *client, struct _Connection *connection
);

/*** requests ***/

// when a client is already connected to the cerver, a request can be made to the cerver
// the response will be handled by the client's handlers
// this is a blocking method, as it will wait until a complete cerver response has been received
// the response will be handled using the client's packet handler
// this method only works if your response consists only of one packet
// neither client nor the connection will be stopped after the request has ended, the request packet won't be deleted
// retruns 0 when the response has been handled, 1 on error
CLIENT_EXPORT unsigned int client_request_to_cerver (
	Client *client, struct _Connection *connection,
	struct _Packet *request
);

// when a client is already connected to the cerver, a request can be made to the cerver
// the response will be handled by the client's handlers
// this method will NOT block
// this method only works if your response consists only of one packet
// neither client nor the connection will be stopped after the request has ended, the request packet won't be deleted
// returns 0 on success request, 1 on error
CLIENT_EXPORT unsigned int client_request_to_cerver_async (
	Client *client, struct _Connection *connection,
	struct _Packet *request
);

/*** files ***/

// adds a new file path to take into account when getting a request for a file
// returns 0 on success, 1 on error
CLIENT_EXPORT u8 client_files_add_path (
	Client *client, const char *path
);

// sets the default uploads path to be used when receiving a file
CLIENT_EXPORT void client_files_set_uploads_path (
	Client *client, const char *uploads_path
);

// sets a custom method to be used to handle a file upload (receive)
// in this method, file contents must be consumed from the sock fd
// and return 0 on success and 1 on error
CLIENT_EXPORT void client_files_set_file_upload_handler (
	Client *client,
	u8 (*file_upload_handler) (
		struct _Client *, struct _Connection *,
		struct _FileHeader *,
		const char *file_data, size_t file_data_len,
		char **saved_filename
	)
);

// sets a callback to be executed after a file has been successfully received
CLIENT_EXPORT void client_files_set_file_upload_cb (
	Client *client,
	void (*file_upload_cb) (
		struct _Client *, struct _Connection *,
		const char *saved_filename
	)
);

// search for the requested file in the configured paths
// returns the actual filename (path + directory) where it was found, NULL on error
CLIENT_PUBLIC String *client_files_search_file (
	Client *client, const char *filename
);

// requests a file from the cerver
// the client's uploads_path should have been configured before calling this method
// returns 0 on success sending request, 1 on failed to send request
CLIENT_EXPORT u8 client_file_get (
	Client *client, struct _Connection *connection,
	const char *filename
);

// sends a file to the cerver
// returns 0 on success sending request, 1 on failed to send request
CLIENT_EXPORT u8 client_file_send (
	Client *client, struct _Connection *connection,
	const char *filename
);

/*** end ***/

// closes the connection's socket & set it to be inactive
// does not send a close connection packet to the cerver
// returns 0 on success, 1 on error
CLIENT_PUBLIC int client_connection_stop (
	Client *client, struct _Connection *connection
);

// terminates the connection & closes the socket
// but does NOT destroy the current connection
// returns 0 on success, 1 on error
CLIENT_PUBLIC int client_connection_close (
	Client *client, struct _Connection *connection
);

// terminates and destroys a connection registered to a client
// that is connected to a cerver
// returns 0 on success, 1 on error
CLIENT_PUBLIC int client_connection_end (
	Client *client, struct _Connection *connection
);

// stop any on going connection and process and destroys the client
// returns 0 on success, 1 on error
CLIENT_EXPORT u8 client_teardown (Client *client);

// calls client_teardown () in a new thread as handlers might need time to stop
// that will cause the calling thread to wait at least a second
// returns 0 on success creating thread, 1 on error
CLIENT_EXPORT u8 client_teardown_async (Client *client);

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif