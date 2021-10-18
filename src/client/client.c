#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <time.h>
#include <errno.h>

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/collections/dlist.h"

#include "client/auth.h"
#include "client/cerver.h"
#include "client/client.h"
#include "client/connection.h"
#include "client/errors.h"
#include "client/events.h"
#include "client/files.h"
#include "client/handler.h"
#include "client/network.h"
#include "client/packets.h"
#include "client/receive.h"

#include "client/threads/thread.h"

#include "client/utils/log.h"
#include "client/utils/utils.h"

static u8 client_file_receive (
	Client *client, Connection *connection,
	FileHeader *file_header,
	const char *file_data, size_t file_data_len,
	char **saved_filename
);

static u64 next_client_id = 0;

#pragma region global

// initializes global client values
// should be called only once at the start of the program
void client_init (void) {

	client_log_init ();

}

// correctly disposes global values
// should be called only once at the very end of the program
void client_end (void) {

	client_log_end ();

}

#pragma endregion

#pragma region aux

static ClientConnection *client_connection_aux_new (
	Client *client, Connection *connection
) {

	ClientConnection *cc = (ClientConnection *) malloc (sizeof (ClientConnection));
	if (cc) {
		cc->connection_thread_id = 0;
		cc->client = client;
		cc->connection = connection;
	}

	return cc;

}

void client_connection_aux_delete (ClientConnection *cc) { if (cc) free (cc); }

#pragma endregion

#pragma region stats

static ClientStats *client_stats_new (void) {

	ClientStats *client_stats = (ClientStats *) malloc (sizeof (ClientStats));
	if (client_stats) {
		(void) memset (client_stats, 0, sizeof (ClientStats));
		client_stats->received_packets = packets_per_type_new ();
		client_stats->sent_packets = packets_per_type_new ();
	}

	return client_stats;

}

static inline void client_stats_delete (ClientStats *client_stats) {

	if (client_stats) {
		packets_per_type_delete (client_stats->received_packets);
		packets_per_type_delete (client_stats->sent_packets);

		free (client_stats);
	}

}

void client_stats_print (Client *client) {

	if (client) {
		if (client->stats) {
			client_log_msg ("\nClient's stats:\n");
			client_log_msg ("Threshold time:            %ld", client->stats->threshold_time);

			client_log_msg ("N receives done:           %ld", client->stats->n_receives_done);

			client_log_msg ("Total bytes received:      %ld", client->stats->total_bytes_received);
			client_log_msg ("Total bytes sent:          %ld", client->stats->total_bytes_sent);

			client_log_msg ("N packets received:        %ld", client->stats->n_packets_received);
			client_log_msg ("N packets sent:            %ld", client->stats->n_packets_sent);

			client_log_msg ("\nReceived packets:");
			packets_per_type_print (client->stats->received_packets);

			client_log_msg ("\nSent packets:");
			packets_per_type_print (client->stats->sent_packets);
		}

		else {
			client_log (
				LOG_TYPE_ERROR, LOG_TYPE_CLIENT,
				"Client does not have a reference to a client stats!"
			);
		}
	}

	else {
		client_log (
			LOG_TYPE_WARNING, LOG_TYPE_CLIENT,
			"Can't get stats of a NULL client!"
		);
	}

}

static ClientFileStats *client_file_stats_new (void) {

	ClientFileStats *file_stats = (ClientFileStats *) malloc (sizeof (ClientFileStats));
	if (file_stats) {
		(void) memset (file_stats, 0, sizeof (ClientFileStats));
	}

	return file_stats;

}

static void client_file_stats_delete (ClientFileStats *file_stats) {

	if (file_stats) free (file_stats);

}

void client_file_stats_print (Client *client) {

	if (client) {
		if (client->file_stats) {
			client_log_msg ("Files requests:                %ld", client->file_stats->n_files_requests);
			client_log_msg ("Success requests:              %ld", client->file_stats->n_success_files_requests);
			client_log_msg ("Bad requests:                  %ld", client->file_stats->n_bad_files_requests);
			client_log_msg ("Files sent:                    %ld", client->file_stats->n_files_sent);
			client_log_msg ("Failed files sent:             %ld", client->file_stats->n_bad_files_sent);
			client_log_msg ("Files bytes sent:              %ld\n", client->file_stats->n_bytes_sent);

			client_log_msg ("Files upload requests:         %ld", client->file_stats->n_files_upload_requests);
			client_log_msg ("Success uploads:               %ld", client->file_stats->n_success_files_uploaded);
			client_log_msg ("Bad uploads:                   %ld", client->file_stats->n_bad_files_upload_requests);
			client_log_msg ("Bad files received:            %ld", client->file_stats->n_bad_files_received);
			client_log_msg ("Files bytes received:          %ld\n", client->file_stats->n_bytes_received);
		}
	}

}

#pragma endregion

#pragma region main

const char *client_connections_status_to_string (
	const ClientConnectionsStatus status
) {

	switch (status) {
		#define XX(num, name, string, description) case CLIENT_CONNECTIONS_STATUS_##name: return #string;
		CLIENT_CONNECTIONS_STATUS_MAP(XX)
		#undef XX
	}

	return client_connections_status_to_string (CLIENT_CONNECTIONS_STATUS_NONE);

}

const char *client_connections_status_description (
	const ClientConnectionsStatus status
) {

	switch (status) {
		#define XX(num, name, string, description) case CLIENT_CONNECTIONS_STATUS_##name: return #description;
		CLIENT_CONNECTIONS_STATUS_MAP(XX)
		#undef XX
	}

	return client_connections_status_description (CLIENT_CONNECTIONS_STATUS_NONE);

}

Client *client_new (void) {

	Client *client = (Client *) malloc (sizeof (Client));
	if (client) {
		client->id = 0;
		client->session_id = NULL;

		(void) memset (client->name, 0, CLIENT_NAME_SIZE);

		client->connections = NULL;

		client->drop_client = false;

		client->data = NULL;
		client->delete_data = NULL;

		client->running = false;
		client->time_started = 0;
		client->uptime = 0;

		client->num_handlers_alive = 0;
		client->num_handlers_working = 0;
		client->handlers_lock = NULL;
		client->app_packet_handler = NULL;
		client->app_error_packet_handler = NULL;
		client->custom_packet_handler = NULL;

		client->check_packets = false;

		client->lock = NULL;

		for (unsigned int i = 0; i < CLIENT_MAX_EVENTS; i++)
			client->events[i] = NULL;

		for (unsigned int i = 0; i < CLIENT_MAX_ERRORS; i++)
			client->errors[i] = NULL;

		client->n_paths = 0;
		for (unsigned int i = 0; i < CLIENT_FILES_MAX_PATHS; i++)
			client->paths[i] = NULL;

		client->uploads_path = NULL;

		client->file_upload_handler = client_file_receive;

		client->file_upload_cb = NULL;

		client->file_stats = NULL;

		client->stats = NULL;
	}

	return client;

}

void client_delete (void *ptr) {

	if (ptr) {
		Client *client = (Client *) ptr;

		str_delete (client->session_id);

		dlist_delete (client->connections);

		if (client->data) {
			if (client->delete_data) client->delete_data (client->data);
			else free (client->data);
		}

		if (client->handlers_lock) {
			pthread_mutex_destroy (client->handlers_lock);
			free (client->handlers_lock);
		}

		handler_delete (client->app_packet_handler);
		handler_delete (client->app_error_packet_handler);
		handler_delete (client->custom_packet_handler);

		if (client->lock) {
			pthread_mutex_destroy (client->lock);
			free (client->lock);
		}

		for (unsigned int i = 0; i < CLIENT_MAX_EVENTS; i++)
			if (client->events[i]) client_event_delete (client->events[i]);

		for (unsigned int i = 0; i < CLIENT_MAX_ERRORS; i++)
			if (client->errors[i]) client_error_delete (client->errors[i]);

		for (unsigned int i = 0; i < CLIENT_FILES_MAX_PATHS; i++)
			str_delete (client->paths[i]);

		str_delete (client->uploads_path);

		client_file_stats_delete (client->file_stats);

		client_stats_delete (client->stats);

		free (client);
	}

}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

void client_delete_dummy (void *ptr) {}

#pragma GCC diagnostic pop

// creates a new client and inits its values
Client *client_create (void) {

	Client *client = client_new ();
	if (client) {
		client->id = next_client_id;
		next_client_id += 1;

		(void) strncpy (client->name, CLIENT_DEFAULT_NAME, CLIENT_NAME_SIZE - 1);

		(void) time (&client->connected_timestamp);

		client->connections = dlist_init (
			connection_delete, connection_comparator
		);

		client->lock = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
		pthread_mutex_init (client->lock, NULL);

		client->file_stats = client_file_stats_new ();

		client->stats = client_stats_new ();
	}

	return client;

}

// creates a new client and registers a new connection
Client *client_create_with_connection (
	Cerver *cerver,
	const i32 sock_fd, const struct sockaddr_storage *address
) {

	Client *client = client_create ();
	if (client) {
		Connection *connection = connection_create (sock_fd, address, cerver->protocol);
		if (connection) connection_register_to_client (client, connection);
		else {
			// failed to create a new connection
			client_delete (client);
			client = NULL;
		}
	}

	return client;

}

// sets the client's name
void client_set_name (Client *client, const char *name) {

	if (client) {
		(void) strncpy (client->name, name, CLIENT_NAME_SIZE - 1);
	}

}

// sets the client's session id
// returns 0 on succes, 1 on error
u8 client_set_session_id (
	Client *client, const char *session_id
) {

	u8 retval = 1;

	if (client) {
		str_delete (client->session_id);
		client->session_id = session_id ? str_new (session_id) : NULL;

		retval = 0;
	}

	return retval;

}

// returns the client's app data
void *client_get_data (Client *client) {

	return (client ? client->data : NULL);

}

// sets client's data and a way to destroy it
// deletes the previous data of the client
void client_set_data (
	Client *client, void *data, Action delete_data
) {

	if (client) {
		if (client->data) {
			if (client->delete_data) client->delete_data (client->data);
			else free (client->data);
		}

		client->data = data;
		client->delete_data = delete_data;
	}

}

// sets customs PACKET_TYPE_APP and PACKET_TYPE_APP_ERROR packet types handlers
void client_set_app_handlers (
	Client *client,
	Handler *app_handler, Handler *app_error_handler
) {

	if (client) {
		client->app_packet_handler = app_handler;
		if (client->app_packet_handler) {
			client->app_packet_handler->type = HANDLER_TYPE_CLIENT;
			client->app_packet_handler->client = client;
		}

		client->app_error_packet_handler = app_error_handler;
		if (client->app_error_packet_handler) {
			client->app_error_packet_handler->type = HANDLER_TYPE_CLIENT;
			client->app_error_packet_handler->client = client;
		}
	}

}

// sets a PACKET_TYPE_CUSTOM packet type handler
void client_set_custom_handler (
	Client *client, Handler *custom_handler
) {

	if (client) {
		client->custom_packet_handler = custom_handler;
		if (client->custom_packet_handler) {
			client->custom_packet_handler->type = HANDLER_TYPE_CLIENT;
			client->custom_packet_handler->client = client;
		}
	}

}

// set whether to check or not incoming packets
// check packet's header protocol id & version compatibility
// if packets do not pass the checks, won't be handled and will be inmediately destroyed
// packets size must be cheked in individual methods (handlers)
// by default, this option is turned off
void client_set_check_packets (
	Client *client, bool check_packets
) {

	if (client) {
		client->check_packets = check_packets;
	}

}

// compare clients based on their client ids
int client_comparator_client_id (
	const void *a, const void *b
) {

	if (a && b) {
		Client *client_a = (Client *) a;
		Client *client_b = (Client *) b;

		if (client_a->id < client_b->id) return -1;
		else if (client_a->id == client_b->id) return 0;
		else return 1;
	}

	return 0;

}

// compare clients based on their session ids
int client_comparator_session_id (
	const void *a, const void *b
) {

	if (a && b) return str_compare (
		((Client *) a)->session_id, ((Client *) b)->session_id
	);

	if (a && !b) return -1;
	if (!a && b) return 1;

	return 0;

}

// closes all client connections
u8 client_disconnect (Client *client) {

	u8 retval = 1;

	if (client) {
		Connection *connection = NULL;
		for (ListElement *le = dlist_start (client->connections); le; le = le->next) {
			connection = (Connection *) le->data;
			connection_end (connection);
		}

		retval = 0;
	}

	return retval;

}

// the client got disconnected from the cerver, so correctly clear our data
void client_got_disconnected (Client *client) {

	if (client) {
		// close any ongoing connection
		for (ListElement *le = dlist_start (client->connections); le; le = le->next) {
			connection_end ((Connection *) le->data);
		}

		// dlist_reset (client->connections);

		// reset client
		client->running = false;
		client->time_started = 0;
	}

}

// adds a new connection to the end of the client to the client's connection list
// without adding it to any other structure
// returns 0 on success, 1 on error
u8 client_connection_add (Client *client, Connection *connection) {

	return (client && connection) ?
		(u8) dlist_insert_after (
			client->connections, dlist_end (client->connections), connection
		) : 1;

}

// removes the connection from the client
// returns 0 on success, 1 on error
u8 client_connection_remove (
	Client *client, Connection *connection
) {

	u8 retval = 1;

	if (client && connection)
		retval = dlist_remove (
			client->connections, connection, NULL
		) ? 0 : 1;

	return retval;

}

#pragma endregion

#pragma region client

static u8 client_app_handler_start (Client *client) {

	u8 retval = 0;

	if (client) {
		if (client->app_packet_handler) {
			if (!client->app_packet_handler->direct_handle) {
				if (!handler_start (client->app_packet_handler)) {
					#ifdef CLIENT_DEBUG
					client_log_success (
						"Client %s app_packet_handler has started!",
						client->name
					);
					#endif
				}

				else {
					client_log_error (
						"Failed to start client %s app_packet_handler!",
						client->name
					);

					retval = 1;
				}
			}
		}

		else {
			client_log_warning (
				"Client %s does not have an app_packet_handler",
				client->name
			);
		}
	}

	return retval;

}

static u8 client_app_error_handler_start (Client *client) {

	u8 retval = 0;

	if (client) {
		if (client->app_error_packet_handler) {
			if (!client->app_error_packet_handler->direct_handle) {
				if (!handler_start (client->app_error_packet_handler)) {
					#ifdef CLIENT_DEBUG
					client_log_success (
						"Client %s app_error_packet_handler has started!",
						client->name
					);
					#endif
				}

				else {
					client_log_error (
						"Failed to start client %s app_error_packet_handler!",
						client->name
					);

					retval = 1;
				}
			}
		}

		else {
			client_log_warning (
				"Client %s does not have an app_error_packet_handler",
				client->name
			);
		}
	}

	return retval;

}

static u8 client_custom_handler_start (Client *client) {

	u8 retval = 0;

	if (client) {
		if (client->custom_packet_handler) {
			if (!client->custom_packet_handler->direct_handle) {
				if (!handler_start (client->custom_packet_handler)) {
					#ifdef CLIENT_DEBUG
					client_log_success (
						"Client %s custom_packet_handler has started!",
						client->name
					);
					#endif
				}

				else {
					client_log_error (
						"Failed to start client %s custom_packet_handler!",
						client->name
					);

					retval = 1;
				}
			}
		}

		else {
			client_log_warning (
				"Client %s does not have a custom_packet_handler",
				client->name
			);
		}
	}

	return retval;

}

// starts all client's handlers
static u8 client_handlers_start (Client *client) {

	u8 errors = 0;

	if (client) {
		#ifdef CLIENT_DEBUG
		client_log_debug (
			"Initializing %s handlers...", client->name
		);
		#endif

		client->handlers_lock = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
		pthread_mutex_init (client->handlers_lock, NULL);

		errors |= client_app_handler_start (client);

		errors |= client_app_error_handler_start (client);

		errors |= client_custom_handler_start (client);

		if (!errors) {
			#ifdef CLIENT_DEBUG
			client_log_success (
				"Done initializing client %s handlers!", client->name
			);
			#endif
		}
	}

	return errors;

}

static u8 client_start (Client *client) {

	u8 retval = 1;

	if (client) {
		// check if we walready have the client poll running
		if (!client->running) {
			time (&client->time_started);
			client->running = true;

			if (!client_handlers_start (client)) {
				retval = 0;
			}

			else {
				client->running = false;
			}
		}

		else {
			// client is already running because of an active connection
			retval = 0;
		}
	}

	return retval;

}

// creates a new connection that is ready to connect and registers it to the client
Connection *client_connection_create (
	Client *client,
	const char *ip_address, u16 port,
	Protocol protocol, bool use_ipv6
) {

	Connection *connection = NULL;

	if (client) {
		connection = connection_create_empty ();
		if (connection) {
			connection_set_values (connection, ip_address, port, protocol, use_ipv6);
			connection_init (connection);
			connection_register_to_client (client, connection);

			connection->cond = pthread_cond_new ();
			connection->mutex = pthread_mutex_new ();
		}
	}

	return connection;

}

// registers an existing connection to a client
// retuns 0 on success, 1 on error
int client_connection_register (
	Client *client, Connection *connection
) {

	int retval = 1;

	if (client && connection) {
		retval =  dlist_insert_after (
			client->connections,
			dlist_end (client->connections),
			connection
		);
	}

	return retval;

}

// unregister an exitsing connection from the client
// returns 0 on success, 1 on error or if the connection does not belong to the client
int client_connection_unregister (
	Client *client, Connection *connection
) {

	int retval = 1;

	if (client && connection) {
		if (dlist_remove (client->connections, connection, NULL)) {
			retval = 0;
		}
	}

	return retval;

}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

static inline void client_connection_get_next_packet_handler (
	const size_t received,
	Client *client, Connection *connection,
	Packet *packet
) {

	// update stats
	client->stats->n_receives_done += 1;
	client->stats->total_bytes_received += received;

	#ifdef CONNECTION_STATS
	connection->stats->n_receives_done += 1;
	connection->stats->total_bytes_received += received;
	#endif

	// handle the actual packet
	(void) client_packet_handler (packet);

}

#pragma GCC diagnostic pop

static unsigned int client_connection_get_next_packet_actual (
	Client *client, Connection *connection
) {

	unsigned int retval = 1;

	// TODO: use a static packet
	Packet *packet = packet_new ();
	packet->client = client;
	packet->connection = connection;

	// first receive the packet header
	size_t data_size = sizeof (PacketHeader);

	if (
		client_receive_data (
			client, connection,
			(char *) &packet->header, sizeof (PacketHeader),
			data_size
		) == RECEIVE_ERROR_NONE
	) {
		#ifdef CLIENT_RECEIVE_DEBUG
		packet_header_log (&packet->header);
		#endif

		// check if need more data to complete the packet
		if (packet->header.packet_size > sizeof (PacketHeader)) {
			// TODO: add ability to configure this value
			// check that the packet is not to big
			if (packet->header.packet_size <= MAX_UDP_PACKET_SIZE) {
				(void) packet_create_data (
					packet, packet->header.packet_size - sizeof (PacketHeader)
				);

				data_size = packet->data_size;

				if (
					client_receive_data (
						client, connection,
						packet->data, packet->data_size,
						data_size
					) == RECEIVE_ERROR_NONE
				) {
					// we can safely handle the packet
					client_connection_get_next_packet_handler (
						packet->packet_size,
						client, connection,
						packet
					);

					retval = 0;
				}
			}

			else {
				// we received a bad packet
				packet_delete (packet);
			}
		}

		else {
			// we can safely handle the packet
			client_connection_get_next_packet_handler (
				packet->packet_size,
				client, connection,
				packet
			);

			retval = 0;
		}
	}

	return retval;

}

// performs a receive in the connection's socket
// to get a complete packet & handle it
// returns 0 on success, 1 on error
unsigned int client_connection_get_next_packet (
	Client *client, Connection *connection
) {

	unsigned int retval = 1;

	if (client && connection) {
		retval = client_connection_get_next_packet_actual (
			client, connection
		);
	}

	return retval;

}

#pragma endregion

#pragma region connect

// connects a client to the host with the specified values in the connection
// it can be a cerver or not
// this is a blocking method, as it will wait until the connection has been successfull or a timeout
// user must manually handle how he wants to receive / handle incomming packets and also send requests
// returns 0 when the connection has been established, 1 on error or failed to connect
unsigned int client_connect (
	Client *client, Connection *connection
) {

	unsigned int retval = 1;

	if (client && connection) {
		if (!connection_connect (connection)) {
			client_event_trigger (CLIENT_EVENT_CONNECTED, client, connection);
			// connection->active = true;
			connection->active = true;
			(void) time (&connection->connected_timestamp);

			retval = 0;     // success - connected to cerver
		}

		else {
			client_event_trigger (
				CLIENT_EVENT_CONNECTION_FAILED,
				client, connection
			);
		}
	}

	return retval;

}

// connects a client to the host with the specified values in the connection
// performs a first read to get the cerver info packet
// this is a blocking method, and works exactly the same as if only calling client_connect ()
// returns 0 when the connection has been established, 1 on error or failed to connect
unsigned int client_connect_to_cerver (
	Client *client, Connection *connection
) {

	unsigned int retval = 1;

	if (!client_connect (client, connection)) {
		// we expect to handle a packet with the cerver's information
		client_connection_get_next_packet (
			client, connection
		);

		retval = 0;
	}

	return retval;

}

static void *client_connect_thread (void *client_connection_ptr) {

	if (client_connection_ptr) {
		ClientConnection *cc = (ClientConnection *) client_connection_ptr;

		if (!connection_connect (cc->connection)) {
			// client_event_trigger (cc->client, EVENT_CONNECTED);
			// cc->connection->active = true;
			cc->connection->active = true;
			(void) time (&cc->connection->connected_timestamp);

			client_start (cc->client);
		}

		client_connection_aux_delete (cc);
	}

	return NULL;

}

// connects a client to the host with the specified values in the connection
// it can be a cerver or not
// this is NOT a blocking method, a new thread will be created to wait for a connection to be established
// open a success connection, EVENT_CONNECTED will be triggered, otherwise, EVENT_CONNECTION_FAILED will be triggered
// user must manually handle how he wants to receive / handle incomming packets and also send requests
// returns 0 on success connection thread creation, 1 on error
unsigned int client_connect_async (
	Client *client, Connection *connection
) {

	unsigned int retval = 1;

	if (client && connection) {
		ClientConnection *cc = client_connection_aux_new (client, connection);
		if (cc) {
			if (!thread_create_detachable (
				&cc->connection_thread_id, client_connect_thread, cc
			)) {
				retval = 0;         // success
			}

			else {
				#ifdef CLIENT_DEBUG
				client_log_error (
					"Failed to create client_connect_thread () detachable thread!"
				);
				#endif
			}
		}
	}

	return retval;

}

#pragma endregion

#pragma region start

static int connection_start_update (
	Client *client, Connection *connection
) {

	int retval = 1;

	if (!thread_create_detachable (
			&connection->update_thread_id,
			connection_update,
			client_connection_aux_new (client, connection)
		)) {
			retval = 0;
		}

		else {
			client_log_error (
				"client_connection_start () - "
				"Failed to create update thread for connection %s",
				connection->name
			);
		}

	return retval;

}

static int connection_start_send (
	Client *client, Connection *connection
) {

	int retval = 1;

	if (!thread_create_detachable (
			&connection->send_thread_id,
			connection_send_thread,
			client_connection_aux_new (client, connection)
		)) {
			retval = 0;
		}

		else {
			client_log_error (
				"client_connection_start () - "
				"Failed to create send thread for connection %s",
				connection->name
			);
		}

	return retval;

}

// after a client connection successfully connects to a server,
// it will start the connection's update thread to enable the connection to
// receive & handle packets in a dedicated thread
// returns 0 on success, 1 on error
int client_connection_start (
	Client *client, Connection *connection
) {

	int retval = 1;

	if (client && connection) {
		if (connection->active) {
			if (!client_start (client)) {
				int errors = 0;

				errors |= connection_start_update (client, connection);

				if (connection->use_send_queue) {
					errors |= connection_start_send (client, connection);
				}

				retval = errors;
			}

			else {
				client_log_error (
					"client_connection_start () - "
					"Failed to start client %s",
					client->name
				);
			}
		}
	}

	return retval;

}

// connects a client connection to a server
// and after a success connection, it will start the connection (create update thread for receiving messages)
// this is a blocking method, returns only after a success or failed connection
// returns 0 on success, 1 on error
int client_connect_and_start (Client *client, Connection *connection) {

	int retval = 1;

	if (client && connection) {
		if (!client_connect (client, connection)) {
			if (!client_connection_start (client, connection)) {
				retval = 0;
			}
		}

		else {
			client_log_error (
				"client_connect_and_start () - Client %s failed to connect",
				client->name
			);
		}
	}

	return retval;

}

static void *client_connection_start_wrapper (void *data_ptr) {

	if (data_ptr) {
		ClientConnection *cc = (ClientConnection *) data_ptr;
		client_connect_and_start (cc->client, cc->connection);
		client_connection_aux_delete (cc);
	}

	return NULL;

}

// connects a client connection to a server in a new thread to avoid blocking the calling thread,
// and after a success connection, it will start the connection (create update thread for receiving messages)
// returns 0 on success creating connection thread, 1 on error
u8 client_connect_and_start_async (Client *client, Connection *connection) {

	pthread_t thread_id = 0;

	return (client && connection) ? thread_create_detachable (
		&thread_id,
		client_connection_start_wrapper,
		client_connection_aux_new (client, connection)
	) : 1;

}

#pragma endregion

#pragma region requests

// when a client is already connected to the cerver, a request can be made to the cerver
// and the result will be returned
// this is a blocking method, as it will wait until a complete cerver response has been received
// the response will be handled using the client's packet handler
// this method only works if your response consists only of one packet
// neither client nor the connection will be stopped after the request has ended, the request packet won't be deleted
// retruns 0 when the response has been handled, 1 on error
unsigned int client_request_to_cerver (
	Client *client, Connection *connection, Packet *request
) {

	unsigned int retval = 1;

	if (client && connection && request) {
		// send the request to the cerver
		packet_set_network_values (request, client, connection);

		size_t sent = 0;
		if (!packet_send (request, 0, &sent, false)) {
			// printf ("Request to cerver: %ld\n", sent);

			// receive the data directly
			client_connection_get_next_packet (client, connection);

			retval = 0;
		}

		else {
			#ifdef CLIENT_DEBUG
			client_log_error (
				"client_request_to_cerver () - "
				"failed to send request packet!"
			);
			#endif
		}
	}

	return retval;

}

static void *client_request_to_cerver_thread (void *cc_ptr) {

	if (cc_ptr) {
		ClientConnection *cc = (ClientConnection *) cc_ptr;

		(void) client_connection_get_next_packet (
			cc->client, cc->connection
		);

		client_connection_aux_delete (cc);
	}

	return NULL;

}

// when a client is already connected to the cerver, a request can be made to the cerver
// the result will be placed inside the connection
// this method will NOT block and the response will be handled using the client's packet handler
// this method only works if your response consists only of one packet
// neither client nor the connection will be stopped after the request has ended, the request packet won't be deleted
// returns 0 on success request, 1 on error
unsigned int client_request_to_cerver_async (
	Client *client, Connection *connection, Packet *request
) {

	unsigned int retval = 1;

	if (client && connection && request) {
		// send the request to the cerver
		packet_set_network_values (request, client, connection);
		if (!packet_send (request, 0, NULL, false)) {
			ClientConnection *cc = client_connection_aux_new (client, connection);
			if (cc) {
				// create a new thread to receive & handle the response
				if (!thread_create_detachable (
					&cc->connection_thread_id, client_request_to_cerver_thread, cc
				)) {
					retval = 0;         // success
				}

				else {
					#ifdef CLIENT_DEBUG
					client_log_error (
						"Failed to create client_request_to_cerver_thread () "
						"detachable thread!"
					);
					#endif
				}
			}
		}

		else {
			#ifdef CLIENT_DEBUG
			client_log_error (
				"client_request_to_cerver_async () - "
				"failed to send request packet!"
			);
			#endif
		}
	}

	return retval;

}

#pragma endregion

#pragma region files

static u8 client_file_receive (
	Client *client, Connection *connection,
	FileHeader *file_header,
	const char *file_data, size_t file_data_len,
	char **saved_filename
) {

	u8 retval = 1;

	// generate a custom filename taking into account the uploads path
	*saved_filename = c_string_create (
		"%s/%ld-%s",
		client->uploads_path->str,
		time (NULL), file_header->filename
	);

	if (*saved_filename) {
		retval = file_receive_actual (
			client, connection,
			file_header,
			file_data, file_data_len,
			saved_filename
		);
	}

	return retval;

}

// adds a new file path to take into account when getting a request for a file
// returns 0 on success, 1 on error
u8 client_files_add_path (Client *client, const char *path) {

	u8 retval = 1;

	if (client && path) {
		if (client->n_paths < CLIENT_FILES_MAX_PATHS) {
			client->paths[client->n_paths] = str_new (path);
			client->n_paths += 1;
		}
	}

	return retval;

}

// sets the default uploads path to be used when receiving a file
void client_files_set_uploads_path (
	Client *client, const char *uploads_path
) {

	if (client && uploads_path) {
		str_delete (client->uploads_path);
		client->uploads_path = str_new (uploads_path);
	}

}

// sets a custom method to be used to handle a file upload (receive)
// in this method, file contents must be consumed from the sock fd
// and return 0 on success and 1 on error
void client_files_set_file_upload_handler (
	Client *client,
	u8 (*file_upload_handler) (
		struct _Client *, struct _Connection *,
		struct _FileHeader *,
		const char *file_data, size_t file_data_len,
		char **saved_filename
	)
) {

	if (client) {
		client->file_upload_handler = file_upload_handler;
	}

}

// sets a callback to be executed after a file has been successfully received
void client_files_set_file_upload_cb (
	Client *client,
	void (*file_upload_cb) (
		struct _Client *, struct _Connection *,
		const char *saved_filename
	)
) {

	if (client) {
		client->file_upload_cb = file_upload_cb;
	}

}

// search for the requested file in the configured paths
// returns the actual filename (path + directory) where it was found, NULL on error
String *client_files_search_file (
	Client *client, const char *filename
) {

	String *retval = NULL;

	if (client && filename) {
		char filename_query[DEFAULT_FILENAME_LEN * 2] = { 0 };
		for (unsigned int i = 0; i < client->n_paths; i++) {
			(void) snprintf (
				filename_query, DEFAULT_FILENAME_LEN * 2,
				"%s/%s",
				client->paths[i]->str, filename
			);

			if (file_exists (filename_query)) {
				retval = str_new (filename_query);
				break;
			}
		}
	}

	return retval;

}

// requests a file from the cerver
// the client's uploads_path should have been configured before calling this method
// returns 0 on success sending request, 1 on failed to send request
u8 client_file_get (
	Client *client, Connection *connection,
	const char *filename
) {

	u8 retval = 1;

	if (client && connection && filename) {
		if (client->uploads_path) {
			Packet *packet = packet_new ();
			if (packet) {
				size_t packet_len = sizeof (PacketHeader) + sizeof (FileHeader);

				packet->packet = malloc (packet_len);
				packet->packet_size = packet_len;

				char *end = (char *) packet->packet;
				PacketHeader *header = (PacketHeader *) end;
				header->packet_type = PACKET_TYPE_REQUEST;
				header->packet_size = packet_len;

				header->request_type = REQUEST_PACKET_TYPE_GET_FILE;

				end += sizeof (PacketHeader);

				FileHeader *file_header = (FileHeader *) end;
				(void) strncpy (file_header->filename, filename, DEFAULT_FILENAME_LEN - 1);
				file_header->len = 0;

				packet_set_network_values (packet, client, connection);

				retval = packet_send (packet, 0, NULL, false);

				packet_delete (packet);
			}
		}
	}

	return retval;

}

// sends a file to the cerver
// returns 0 on success sending request, 1 on failed to send request
u8 client_file_send (
	Client *client, Connection *connection,
	const char *filename
) {

	u8 retval = 1;

	if (client && connection && filename) {
		char *last = strrchr ((char *) filename, '/');
		const char *actual_filename = last ? last + 1 : NULL;
		if (actual_filename) {
			// try to open the file
			struct stat filestatus = { 0 };
			int file_fd = file_open_as_fd (filename, &filestatus, O_RDONLY);
			if (file_fd >= 0) {
				size_t sent = file_send_by_fd (
					client, connection,
					file_fd, actual_filename, filestatus.st_size
				);

				client->file_stats->n_files_sent += 1;
				client->file_stats->n_bytes_sent += sent;

				if (sent == (size_t) filestatus.st_size) retval = 0;

				close (file_fd);
			}

			else {
				client_log (
					LOG_TYPE_ERROR, LOG_TYPE_FILE,
					"client_file_send () - Failed to open file %s", filename
				);
			}
		}

		else {
			client_log_error ("client_file_send () - failed to get actual filename");
		}
	}

	return retval;

}

#pragma endregion

#pragma region end

// ends a connection with a cerver
// by sending a disconnect packet and the closing the connection
static void client_connection_terminate (
	Client *client, Connection *connection
) {

	if (connection) {
		if (connection->active) {
			if (connection->cerver) {
				// send a close connection packet
				Packet *packet = packet_generate_request (
					PACKET_TYPE_CLIENT, CLIENT_PACKET_TYPE_CLOSE_CONNECTION, NULL, 0
				);

				if (packet) {
					packet_set_network_values (packet, client, connection);
					if (packet_send (packet, 0, NULL, false)) {
						client_log_error ("Failed to send CLIENT_CLOSE_CONNECTION!");
					}
					packet_delete (packet);
				}
			}
		}
	}

}

// closes the connection's socket & set it to be inactive
// does not send a close connection packet to the cerver
// returns 0 on success, 1 on error
int client_connection_stop (Client *client, Connection *connection) {

	int retval = 1;

	if (client && connection) {
		client_event_trigger (CLIENT_EVENT_CONNECTION_CLOSE, client, connection);
		connection_end (connection);

		retval = 0;
	}

	return retval;

}

// terminates the connection & closes the socket
// but does NOT destroy the current connection
// returns 0 on success, 1 on error
int client_connection_close (Client *client, Connection *connection) {

	int retval = 1;

	if (client && connection) {
		client_connection_terminate (client, connection);
		retval = client_connection_stop (client, connection);
	}

	return retval;

}

// terminates and destroy a connection registered to a client
// that is connected to a cerver
// returns 0 on success, 1 on error
int client_connection_end (Client *client, Connection *connection) {

	int retval = 1;

	if (client && connection) {
		client_connection_close (client, connection);

		dlist_remove (client->connections, connection, NULL);

		if (connection->updating) {
			// wait until connection has finished updating
			pthread_mutex_lock (connection->mutex);

			while (connection->updating) {
				// printf ("client_connection_end () waiting...\n");
				pthread_cond_wait (connection->cond, connection->mutex);
			}

			pthread_mutex_unlock (connection->mutex);
		}

		connection_delete (connection);

		retval = 0;
	}

	return retval;

}

static void client_app_handler_destroy (Client *client) {

	if (client) {
		if (client->app_packet_handler) {
			if (!client->app_packet_handler->direct_handle) {
				// stop app handler
				bsem_post_all (client->app_packet_handler->job_queue->has_jobs);
			}
		}
	}

}

static void client_app_error_handler_destroy (Client *client) {

	if (client) {
		if (client->app_error_packet_handler) {
			if (!client->app_error_packet_handler->direct_handle) {
				// stop app error handler
				bsem_post_all (client->app_error_packet_handler->job_queue->has_jobs);
			}
		}
	}

}

static void client_custom_handler_destroy (Client *client) {

	if (client) {
		if (client->custom_packet_handler) {
			if (!client->custom_packet_handler->direct_handle) {
				// stop custom handler
				bsem_post_all (client->custom_packet_handler->job_queue->has_jobs);
			}
		}
	}

}

static void client_handlers_destroy (Client *client) {

	if (client) {
		client_log_debug (
			"Client %s num_handlers_alive: %d",
			client->name, client->num_handlers_alive
		);

		client_app_handler_destroy (client);

		client_app_error_handler_destroy (client);

		client_custom_handler_destroy (client);

		// poll remaining handlers
		while (client->num_handlers_alive) {
			if (client->app_packet_handler)
				bsem_post_all (client->app_packet_handler->job_queue->has_jobs);

			if (client->app_error_packet_handler)
				bsem_post_all (client->app_error_packet_handler->job_queue->has_jobs);

			if (client->custom_packet_handler)
				bsem_post_all (client->custom_packet_handler->job_queue->has_jobs);

			sleep (1);
		}
	}

}

static void *client_teardown_internal (void *client_ptr) {

	if (client_ptr) {
		Client *client = (Client *) client_ptr;

		pthread_mutex_lock (client->lock);

		// end any ongoing connection
		for (ListElement *le = dlist_start (client->connections); le; le = le->next) {
			client_connection_close (client, (Connection *) le->data);
		}

		client_handlers_destroy (client);

		// delete all connections
		dlist_delete (client->connections);
		client->connections = NULL;

		pthread_mutex_unlock (client->lock);

		client_delete (client);
	}

	return NULL;

}

// stop any on going connection and process and destroys the client
// returns 0 on success, 1 on error
u8 client_teardown (Client *client) {

	u8 retval = 1;

	if (client) {
		client->running = false;

		// wait for all connections to end
		(void) sleep (4);

		client_teardown_internal (client);

		retval = 0;
	}

	return retval;

}

// calls client_teardown () in a new thread as handlers might need time to stop
// that will cause the calling thread to wait at least a second
// returns 0 on success creating thread, 1 on error
u8 client_teardown_async (Client *client) {

	pthread_t thread_id = 0;
	return client ? thread_create_detachable (
		&thread_id,
		client_teardown_internal,
		client
	) : 1;

}

#pragma endregion