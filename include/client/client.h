#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <stdbool.h>

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/collections/dlist.h"

#include "client/config.h"
#include "client/connection.h"
#include "client/errors.h"
#include "client/events.h"
#include "client/network.h"
#include "client/packets.h"

#define CLIENT_FILES_MAX_PATHS           32

struct _Client;
struct _Connection;
struct _Packet;
struct _PacketsPerType;
struct _FileHeader;

#pragma region stats

struct _ClientStats {

	time_t threshold_time;                  // every time we want to reset the client's stats

	u64 n_receives_done;                    // n calls to recv ()

	u64 total_bytes_received;               // total amount of bytes received from this client
	u64 total_bytes_sent;                   // total amount of bytes that have been sent to the client (all of its connections)

	u64 n_packets_received;                 // total number of packets received from this client (packet header + data)
	u64 n_packets_sent;                     // total number of packets sent to this client (all connections)

	struct _PacketsPerType *received_packets;
	struct _PacketsPerType *sent_packets;

};

typedef struct _ClientStats ClientStats;

CLIENT_PUBLIC void client_stats_print (struct _Client *client);

struct _ClientFileStats {

	u64 n_files_requests;
	u64 n_success_files_requests;
	u64 n_bad_files_requests;
	u64 n_bytes_sent;

	u64 n_files_uploaded;
	u64 n_success_files_uploaded;
	u64 n_bad_files_uploaded;
	u64 n_bytes_received;

};

typedef struct _ClientFileStats ClientFileStats;

CLIENT_PUBLIC void client_file_stats_print (struct _Client *client);

#pragma endregion

struct _Client {

	String *name;

	DoubleList *connections;

	bool running;                   // any connection is active

	ClientEvent *events[CLIENT_MAX_EVENTS];
	ClientError *errors[CLIENT_MAX_ERRORS];

	// custom packet handlers
	Action app_packet_handler;
	Action app_error_packet_handler;
	Action custom_packet_handler;

	bool check_packets;              // enable / disbale packet checking

	time_t time_started;
	u64 uptime;

	String *session_id;

	// files
	unsigned int n_paths;
	String *paths[CLIENT_FILES_MAX_PATHS];

	// default path where received files will be placed
	String *uploads_path;

	u8 (*file_upload_handler) (
		struct _Client *, struct _Connection *,
		struct _FileHeader *, char **saved_filename
	);

	void (*file_upload_cb) (
		struct _Client *, struct _Connection *,
		const char *saved_filename
	);

	ClientFileStats *file_stats;

	ClientStats *stats;

};

typedef struct _Client Client;

#pragma region main

// sets the client's name
CLIENT_EXPORT void client_set_name (Client *client, const char *name);

// sets a cutom app packet hanlder and a custom app error packet handler
CLIENT_EXPORT void client_set_app_handlers (
	Client *client, 
	Action app_handler, Action app_error_handler
);

// sets a custom packet handler
CLIENT_EXPORT void client_set_custom_handler (Client *client, Action custom_handler);

// set whether to check or not incoming packets
// check packet's header protocol id & version compatibility
// if packets do not pass the checks, won't be handled and will be inmediately destroyed
// packets size must be cheked in individual methods (handlers)
// by default, this option is turned off
CLIENT_EXPORT void client_set_check_packets (Client *client, bool check_packets);

// sets the client's session id
// returns 0 on succes, 1 on error
CLIENT_PUBLIC u8 client_set_session_id (Client *client, const char *session_id);

// creates a new client, whcih may be used to create connections
CLIENT_EXPORT Client *client_create (void);

// stops any activae connection and destroys a client
CLIENT_EXPORT u8 client_teardown (Client *client);

#pragma endregion

#pragma region connections

// returns a connection assocaited with a socket
CLIENT_PUBLIC struct _Connection *client_connection_get_by_socket (Client *client, i32 sock_fd);

// returns a connection (registered to a client) by its name
CLIENT_PUBLIC struct _Connection *client_connection_get_by_name (Client *client, const char *name);

// creates a new connection and registers it to the specified client
// the connection should be ready to be started
// returns a new connection on success, NULL on error
CLIENT_EXPORT struct _Connection *client_connection_create (
	Client *client,
	const char *ip_address, u16 port, Protocol protocol, bool use_ipv6
);

// registers an existing connection to a client
// retuns 0 on success, 1 on error
CLIENT_EXPORT int client_connection_register (Client *client, struct _Connection *connection);

// unregister an exitsing connection from the client
// returns 0 on success, 1 on error or if the connection does not belong to the client
CLIENT_EXPORT int client_connection_unregister (Client *client, struct _Connection *connection);

// performs a receive in the connection's socket to get a complete packet & handle it
CLIENT_EXPORT void client_connection_get_next_packet (Client *client, struct _Connection *connection);

#pragma endregion

#pragma region connect

// connects a client to the host with the specified values in the connection
// it can be a cerver or not
// this is a blocking method, as it will wait until the connection has been successfull or a timeout
// user must manually handle how he wants to receive / handle incomming packets and also send requests
// returns 0 when the connection has been established, 1 on error or failed to connect
CLIENT_EXPORT unsigned int client_connect (Client *client, struct _Connection *connection);

// connects a client to the host with the specified values in the connection
// performs a first read to get the cerver info packet
// this is a blocking method, and works exactly the same as if only calling client_connect ()
// returns 0 when the connection has been established, 1 on error or failed to connect
CLIENT_EXPORT unsigned int client_connect_to_cerver (Client *client, Connection *connection);

// connects a client to the host with the specified values in the connection
// it can be a cerver or not
// this is NOT a blocking method, a new thread will be created to wait for a connection to be established
// upon a success connection, EVENT_CONNECTED will be triggered, otherwise, EVENT_CONNECTION_FAILED will be triggered
// user must manually handle how he wants to receive / handle incomming packets and also send requests
// returns 0 on success connection thread creation, 1 on error
CLIENT_EXPORT unsigned int client_connect_async (Client *client, struct _Connection *connection);

#pragma endregion

#pragma region start

// after a client connection successfully connects to a server,
// it will start the connection's update thread to enable the connection to
// receive & handle packets in a dedicated thread
// returns 0 on success, 1 on error
CLIENT_EXPORT int client_connection_start (Client *client, struct _Connection *connection);

// connects a client connection to a server
// and after a success connection, it will start the connection (create update thread for receiving messages)
// this is a blocking method, returns only after a success or failed connection
// returns 0 on success, 1 on error
CLIENT_EXPORT int client_connect_and_start (Client *client, struct _Connection *connection);

// connects a client connection to a server in a new thread to avoid blocking the calling thread,
// and after a success connection, it will start the connection (create update thread for receiving messages)
// returns 0 on success creating connection thread, 1 on error
CLIENT_EXPORT u8 client_connect_and_start_async (Client *client, struct _Connection *connection);

#pragma endregion

#pragma region requests

// when a client is already connected to the cerver, a request can be made to the cerver
// the result will be passed to the client's handlers
// this is a blocking method, as it will wait until a complete cerver response has been received
// the response will be handled using the client's packet handler
// this method only works if your response consists only of one packet
// neither client nor the connection will be stopped after the request has ended, the request packet won't be deleted
// retruns 0 when the response has been handled, 1 on error
CLIENT_EXPORT unsigned int client_request_to_cerver (Client *client, struct _Connection *connection, struct _Packet *request);

// when a client is already connected to the cerver, a request can be made to the cerver
// the result will be passed to the client's handlers
// this method will NOT block, instead EVENT_CONNECTION_DATA will be triggered
// this method only works if your response consists only of one packet
// neither client nor the connection will be stopped after the request has ended, the request packet won't be deleted
// returns 0 on success request, 1 on error
CLIENT_EXPORT unsigned int client_request_to_cerver_async (Client *client, struct _Connection *connection, struct _Packet *request);

#pragma endregion

#pragma region files

// adds a new file path to take into account when getting a request for a file
// returns 0 on success, 1 on error
CLIENT_EXPORT u8 client_files_add_path (Client *client, const char *path);

// sets the default uploads path to be used when receiving a file
CLIENT_EXPORT void client_files_set_uploads_path (Client *client, const char *uploads_path);

// sets a custom method to be used to handle a file upload (receive)
// in this method, file contents must be consumed from the sock fd
// and return 0 on success and 1 on error
CLIENT_EXPORT void client_files_set_file_upload_handler (
	Client *client,
	u8 (*file_upload_handler) (
		struct _Client *, struct _Connection *,
		struct _FileHeader *, char **saved_filename
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
CLIENT_PUBLIC String *client_files_search_file (Client *client, const char *filename);

// requests a file from the cerver
// the client's uploads_path should have been configured before calling this method
// returns 0 on success sending request, 1 on failed to send request
CLIENT_EXPORT u8 client_file_get (Client *client, Connection *connection, const char *filename);

// sends a file to the cerver
// returns 0 on success sending request, 1 on failed to send request
CLIENT_EXPORT u8 client_file_send (Client *client, Connection *connection, const char *filename);

#pragma endregion

#pragma region game

// requets the cerver to create a new lobby
// game type: is the type of game to create the lobby, the configuration must exist in the cerver
// returns 0 on success sending request, 1 on failed to send request
CLIENT_EXPORT u8 client_game_create_lobby (
	Client *owner, struct _Connection *connection,
	const char *game_type
);

// requests the cerver to join a lobby
// game type: is the type of game to create the lobby, the configuration must exist in the cerver
// lobby id: if you know the id of the lobby to join to, if not, the cerver witll search one for you
// returns 0 on success sending request, 1 on failed to send request
CLIENT_EXPORT u8 client_game_join_lobby (
	Client *client, struct _Connection *connection,
	const char *game_type, const char *lobby_id
);

// request the cerver to leave the currect lobby
// returns 0 on success sending request, 1 on failed to send request
CLIENT_EXPORT u8 client_game_leave_lobby (
	Client *client, struct _Connection *connection,
	const char *lobby_id
);

// requests the cerver to start the game in the current lobby
// returns 0 on success sending request, 1 on failed to send request
CLIENT_EXPORT u8 client_game_start_lobby (
	Client *client, struct _Connection *connection,
	const char *lobby_id
);

#pragma endregion

#pragma region end

// terminates and destroy a connection registered to a client
// returns 0 on success, 1 on error
CLIENT_EXPORT int client_connection_end (Client *client, struct _Connection *connection);

// terminates all of the client connections and deletes them
// returns 0 on success, 1 on error
CLIENT_EXPORT int client_disconnect (Client *client);

// the client got disconnected from the cerver, so correctly clear our data
CLIENT_EXPORT void client_got_disconnected (Client *client);

#pragma endregion

#pragma region aux

typedef struct ClientConnection {

	Client *client;
	struct _Connection *connection;

} ClientConnection;

CLIENT_PRIVATE ClientConnection *client_connection_aux_new (Client *client, struct _Connection *connection);

CLIENT_PRIVATE void client_connection_aux_delete (void *ptr);

#pragma endregion

#pragma region serialization

#define TOKEN_SIZE         256

// serialized session id - token
struct _SToken {

	char token[TOKEN_SIZE];

};

typedef struct _SToken SToken;

#pragma endregion

#endif