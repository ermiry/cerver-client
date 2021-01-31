#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include "client/types/types.h"

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

#include "client/threads/jobs.h"
#include "client/threads/thread.h"

#include "client/utils/log.h"
#include "client/utils/utils.h"

static int unique_handler_id = 0;

static HandlerData *handler_data_new (void) {

	HandlerData *handler_data = (HandlerData *) malloc (sizeof (HandlerData));
	if (handler_data) {
		handler_data->handler_id = 0;

		handler_data->data = NULL;
		handler_data->packet = NULL;
	}

	return handler_data;

}

static void handler_data_delete (HandlerData *handler_data) {

	if (handler_data) free (handler_data);

}

static Handler *handler_new (void) {

	Handler *handler = (Handler *) malloc (sizeof (Handler));
	if (handler) {
		handler->type = HANDLER_TYPE_NONE;
		handler->unique_id = -1;

		handler->id = -1;
		handler->thread_id = 0;

		handler->data = NULL;
		handler->data_create = NULL;
		handler->data_create_args = NULL;
		handler->data_delete = NULL;

		handler->handler = NULL;
		handler->direct_handle = false;

		handler->job_queue = NULL;

		handler->cerver = NULL;
		handler->client = NULL;
	}

	return handler;

}

void handler_delete (void *handler_ptr) {

	if (handler_ptr) {
		Handler *handler = (Handler *) handler_ptr;

		job_queue_delete (handler->job_queue);

		free (handler_ptr);
	}

}

// creates a new handler
// handler method is your actual app packet handler
Handler *handler_create (Action handler_method) {

	Handler *handler = handler_new ();
	if (handler) {
		handler->unique_id = unique_handler_id;
		unique_handler_id += 1;

		handler->handler = handler_method;

		handler->job_queue = job_queue_create ();
	}

	return handler;

}

// creates a new handler that will be used for cerver's multiple app handlers configuration
// it should be registered to the cerver before it starts
// the user is responsible for setting the unique id, which will be used to match
// incoming packets
// handler method is your actual app packet handler
Handler *handler_create_with_id (int id, Action handler_method) {

	Handler *handler = handler_create (handler_method);
	if (handler) {
		handler->id = id;
	}

	return handler;

}

// sets the handler's data directly
// this data will be passed to the handler method using a HandlerData structure
void handler_set_data (Handler *handler, void *data) {

	if (handler) handler->data = data;

}

// set a method to create the handler's data before it starts handling any packet
// this data will be passed to the handler method using a HandlerData structure
void handler_set_data_create (
	Handler *handler,
	void *(*data_create) (void *args), void *data_create_args
) {

	if (handler) {
		handler->data_create = data_create;
		handler->data_create_args = data_create_args;
	}

}

// set the method to be used to delete the handler's data
void handler_set_data_delete (Handler *handler, Action data_delete) {

	if (handler) handler->data_delete = data_delete;

}

// used to avoid pushing job to the queue and instead handle
// the packet directly in the same thread
void handler_set_direct_handle (Handler *handler, bool direct_handle) {

	if (handler) handler->direct_handle = direct_handle;

}

// while client is running, check for new jobs and handle them
static void handler_do_while_client (Handler *handler) {

	Job *job = NULL;
	Packet *packet = NULL;
	HandlerData *handler_data = handler_data_new ();
	while (handler->client->running) {
		bsem_wait (handler->job_queue->has_jobs);

		if (handler->client->running) {
			(void) pthread_mutex_lock (handler->client->handlers_lock);
			handler->client->num_handlers_working += 1;
			(void) pthread_mutex_unlock (handler->client->handlers_lock);

			// read job from queue
			job = job_queue_pull (handler->job_queue);
			if (job) {
				packet = (Packet *) job->args;

				handler_data->handler_id = handler->id;
				handler_data->data = handler->data;
				handler_data->packet = packet;

				handler->handler (handler_data);

				job_delete (job);
				packet_delete (packet);
			}

			(void) pthread_mutex_lock (handler->client->handlers_lock);
			handler->client->num_handlers_working -= 1;
			(void) pthread_mutex_unlock (handler->client->handlers_lock);
		}
	}

	handler_data_delete (handler_data);

}

static void *handler_do (void *handler_ptr) {

	if (handler_ptr) {
		Handler *handler = (Handler *) handler_ptr;

		pthread_mutex_t *handlers_lock = NULL;
		switch (handler->type) {
			case HANDLER_TYPE_CLIENT:
				handlers_lock = handler->client->handlers_lock;
				break;
			
			default: break;
		}

		// set the thread name
		if (handler->id >= 0) {
			char thread_name[THREAD_NAME_BUFFER_LEN] = { 0 };

			switch (handler->type) {
				case HANDLER_TYPE_CLIENT:
					(void) snprintf (
						thread_name, THREAD_NAME_BUFFER_LEN,
						"client-handler-%d", handler->unique_id
					);
					break;
				default: break;
			}

			(void) thread_set_name (thread_name);
		}

		// TODO: register to signals to handle multiple actions

		if (handler->data_create)
			handler->data = handler->data_create (handler->data_create_args);

		// mark the handler as alive and ready
		(void) pthread_mutex_lock (handlers_lock);
		switch (handler->type) {
			case HANDLER_TYPE_CLIENT: handler->client->num_handlers_alive += 1; break;
			default: break;
		}
		(void) pthread_mutex_unlock (handlers_lock);

		// while cerver / client is running, check for new jobs and handle them
		switch (handler->type) {
			case HANDLER_TYPE_CLIENT: handler_do_while_client (handler); break;
			default: break;
		}

		if (handler->data_delete)
			handler->data_delete (handler->data);

		(void) pthread_mutex_lock (handlers_lock);
		switch (handler->type) {
			case HANDLER_TYPE_CLIENT: handler->client->num_handlers_alive -= 1; break;
			default: break;
		}
		(void) pthread_mutex_unlock (handlers_lock);
	}

	return NULL;

}

// starts the new handler by creating a dedicated thread for it
// called by internal cerver methods
int handler_start (Handler *handler) {

	int retval = 1;

	if (handler) {
		if (handler->type != HANDLER_TYPE_NONE) {
			if (!thread_create_detachable (
				&handler->thread_id,
				(void *(*)(void *)) handler_do,
				(void *) handler
			)) {
				#ifdef HANDLER_DEBUG
				client_log (
					LOG_TYPE_DEBUG, LOG_TYPE_HANDLER,
					"Created handler %d thread!",
					handler->unique_id
				);
				#endif

				retval = 0;
			}
		}

		else {
			client_log_error (
				"handler_start () - Handler %d is of invalid type!",
				handler->unique_id
			);
		}
	}

	return retval;

}

const char *client_handler_error_to_string (
	const ClientHandlerError error
) {

	switch (error) {
		#define XX(num, name, string, description) case CLIENT_HANDLER_ERROR_##name: return #string;
		CLIENT_HANDLER_ERROR_MAP(XX)
		#undef XX
	}

	return client_handler_error_to_string (CLIENT_HANDLER_ERROR_NONE);

}

const char *client_handler_error_description (
	const ClientHandlerError error
) {

	switch (error) {
		#define XX(num, name, string, description) case CLIENT_HANDLER_ERROR_##name: return #description;
		CLIENT_HANDLER_ERROR_MAP(XX)
		#undef XX
	}

	return client_handler_error_description (CLIENT_HANDLER_ERROR_NONE);

}

static void client_cerver_packet_handle_info (Packet *packet) {

	if (packet->data && (packet->data_size > 0)) {
		char *end = (char *) packet->data;

		#ifdef CLIENT_DEBUG
		client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Received a cerver info packet.");
		#endif

		Cerver *cerver_report = cerver_deserialize ((SCerver *) end);
		if (cerver_check_info (
			cerver_report, packet->client, packet->connection
		)) {
			client_log (
				LOG_TYPE_ERROR, LOG_TYPE_NONE,
				"Failed to correctly check cerver info!"
			);
		}
	}

}

// handles cerver type packets
static ClientHandlerError client_cerver_packet_handler (Packet *packet) {

	ClientHandlerError error = CLIENT_HANDLER_ERROR_NONE;

	switch (packet->header.request_type) {
		case CERVER_PACKET_TYPE_INFO:
			client_cerver_packet_handle_info (packet);
			break;

		// the cerves is going to be teardown, we have to disconnect
		case CERVER_PACKET_TYPE_TEARDOWN:
			#ifdef CLIENT_DEBUG
			client_log (
				LOG_TYPE_WARNING, LOG_TYPE_NONE,
				"---> Cerver teardown <---"
			);
			#endif

			client_got_disconnected (packet->client);
			client_event_trigger (CLIENT_EVENT_DISCONNECTED, packet->client, NULL);

			error = CLIENT_HANDLER_ERROR_CLOSED;
			break;

		default:
			client_log (
				LOG_TYPE_WARNING, LOG_TYPE_NONE,
				"Unknown cerver type packet"
			);
			break;
	}

	return error;

}

// handles a client type packet
static ClientHandlerError client_client_packet_handler (Packet *packet) {

	ClientHandlerError error = CLIENT_HANDLER_ERROR_NONE;

	switch (packet->header.request_type) {
		// the cerver close our connection
		case CLIENT_PACKET_TYPE_CLOSE_CONNECTION:
			client_connection_end (packet->client, packet->connection);
			error = CLIENT_HANDLER_ERROR_CLOSED;
			break;

		// the cerver has disconneted us
		case CLIENT_PACKET_TYPE_DISCONNECT:
			client_got_disconnected (packet->client);
			client_event_trigger (CLIENT_EVENT_DISCONNECTED, packet->client, NULL);
			error = CLIENT_HANDLER_ERROR_CLOSED;
			break;

		default:
			client_log (
				LOG_TYPE_WARNING, LOG_TYPE_NONE,
				"Unknown client packet type"
			);
			break;
	}

	return error;

}

// handles a request from a cerver to get a file
static void client_request_get_file (Packet *packet) {

	Client *client = packet->client;

	client->file_stats->n_files_requests += 1;

	// get the necessary information to fulfil the request
	if (packet->data_size >= sizeof (FileHeader)) {
		char *end = (char *) packet->data;
		FileHeader *file_header = (FileHeader *) end;

		// search for the requested file in the configured paths
		String *actual_filename = client_files_search_file (
			client, file_header->filename
		);

		if (actual_filename) {
			#ifdef CLIENT_DEBUG
			client_log_debug (
				"client_request_get_file () - Sending %s...\n",
				actual_filename->str
			);
			#endif

			// if found, pipe the file contents to the client's socket fd
			// the socket should be blocked during the entire operation
			ssize_t sent = file_send (
				client, packet->connection,
				actual_filename->str
			);

			if (sent > 0) {
				client->file_stats->n_success_files_requests += 1;
				client->file_stats->n_files_sent += 1;
				client->file_stats->n_bytes_sent += sent;

				client_log_success ("Sent file %s", actual_filename->str);
			}

			else {
				client_log_error ("Failed to send file %s", actual_filename->str);

				client->file_stats->n_bad_files_sent += 1;
			}

			str_delete (actual_filename);
		}

		else {
			#ifdef CLIENT_DEBUG
			client_log_warning (
				"client_request_get_file () - "
				"file not found"
			);
			#endif

			// if not found, return an error to the client
			(void) client_error_packet_generate_and_send (
				CLIENT_ERROR_FILE_NOT_FOUND, "File not found",
				packet->client, packet->connection
			);

			client->file_stats->n_bad_files_requests += 1;
		}

	}

	else {
		#ifdef CLIENT_DEBUG
		client_log_warning (
			"client_request_get_file () - "
			"missing file header"
		);
		#endif

		// return a bad request error packet
		(void) client_error_packet_generate_and_send (
			CLIENT_ERROR_GET_FILE, "Missing file header",
			packet->client, packet->connection
		);

		client->file_stats->n_bad_files_requests += 1;
	}

}

static void client_request_send_file_actual (Packet *packet) {

	Client *client = packet->client;

	client->file_stats->n_files_upload_requests += 1;

	// get the necessary information to fulfil the request
	if (packet->data_size >= sizeof (FileHeader)) {
		char *end = (char *) packet->data;
		FileHeader *file_header = (FileHeader *) end;

		const char *file_data = NULL;
		size_t file_data_len = 0;
		// printf (
		// 	"\n\npacket->data_size %ld > sizeof (FileHeader) %ld\n\n",
		// 	packet->data_size, sizeof (FileHeader)
		// );
		if (packet->data_size > sizeof (FileHeader)) {
			file_data = end += sizeof (FileHeader);
			file_data_len = packet->data_size - sizeof (FileHeader);
		}

		char *saved_filename = NULL;
		if (!client->file_upload_handler (
			client, packet->connection,
			file_header,
			file_data, file_data_len,
			&saved_filename
		)) {
			client->file_stats->n_success_files_uploaded += 1;

			client->file_stats->n_bytes_received += file_header->len;

			if (client->file_upload_cb) {
				client->file_upload_cb (
					packet->client, packet->connection,
					saved_filename
				);
			}

			if (saved_filename) free (saved_filename);
		}

		else {
			client_log_error (
				"client_request_send_file () - "
				"Failed to receive file"
			);

			client->file_stats->n_bad_files_received += 1;
		}
	}

	else {
		#ifdef CLIENT_DEBUG
		client_log_warning (
			"client_request_send_file () - "
			"missing file header"
		);
		#endif

		// return a bad request error packet
		(void) client_error_packet_generate_and_send (
			CLIENT_ERROR_SEND_FILE, "Missing file header",
			client, packet->connection
		);

		client->file_stats->n_bad_files_upload_requests += 1;
	}

}

// request from a cerver to receive a file
static void client_request_send_file (Packet *packet) {

	// check if the client is able to process the request
	if (packet->client->file_upload_handler && packet->client->uploads_path) {
		client_request_send_file_actual (packet);
	}

	else {
		// return a bad request error packet
		(void) client_error_packet_generate_and_send (
			CLIENT_ERROR_SEND_FILE, "Unable to process request",
			packet->client, packet->connection
		);

		#ifdef CLIENT_DEBUG
		client_log_warning (
			"Client %s is unable to handle REQUEST_PACKET_TYPE_SEND_FILE packets!",
			packet->client->name
		);
		#endif
	}

}

// handles a request made from the cerver
static void client_request_packet_handler (Packet *packet) {

	switch (packet->header.request_type) {
		// request from a cerver to get a file
		case REQUEST_PACKET_TYPE_GET_FILE:
			client_request_get_file (packet);
			break;

		// request from a cerver to receive a file
		case REQUEST_PACKET_TYPE_SEND_FILE:
			client_request_send_file (packet);
			break;

		default:
			client_log (
				LOG_TYPE_WARNING, LOG_TYPE_HANDLER,
				"Unknown request from cerver"
			);
			break;
	}

}

// get the token from the packet data
// returns 0 on succes, 1 on error
static u8 auth_strip_token (Packet *packet, Client *client) {

	u8 retval = 1;

	// check we have a big enough packet
	if (packet->data_size > 0) {
		char *end = (char *) packet->data;

		// check if we have a token
		if (packet->data_size == (sizeof (SToken))) {
			SToken *s_token = (SToken *) (end);
			retval = client_set_session_id (client, s_token->token);
		}
	}

	return retval;

}

static void client_auth_success_handler (Packet *packet) {

	packet->connection->authenticated = true;

	if (packet->connection->cerver) {
		if (packet->connection->cerver->uses_sessions) {
			if (!auth_strip_token (packet, packet->client)) {
				#ifdef AUTH_DEBUG
				client_log_debug (
					"Got client's <%s> session id <%s>",
					packet->client->name,
					packet->client->session_id->str
				);
				#endif
			}
		}
	}

	client_event_trigger (
		CLIENT_EVENT_SUCCESS_AUTH,
		packet->client, packet->connection
	);

}

static void client_auth_packet_handler (Packet *packet) {

	switch (packet->header.request_type) {
		// cerver requested authentication, if not, we will be disconnected
		case AUTH_PACKET_TYPE_REQUEST_AUTH:
			break;

		// we recieve a token from the cerver to use in sessions
		case AUTH_PACKET_TYPE_CLIENT_AUTH:
			break;

		// we have successfully authenticated with the server
		case AUTH_PACKET_TYPE_SUCCESS:
			client_auth_success_handler (packet);
			break;

		default:
			client_log (
				LOG_TYPE_WARNING, LOG_TYPE_NONE,
				"Unknown auth packet type"
			);
			break;
	}

}

// handles a PACKET_TYPE_APP packet type
static void client_app_packet_handler (Packet *packet) {

	if (packet->client->app_packet_handler) {
		if (packet->client->app_packet_handler->direct_handle) {
			// printf ("app_packet_handler - direct handle!\n");
			packet->client->app_packet_handler->handler (packet);
			packet_delete (packet);
		}

		else {
			// add the packet to the handler's job queueu to be handled
			// as soon as the handler is available
			if (job_queue_push (
				packet->client->app_packet_handler->job_queue,
				job_create (NULL, packet)
			)) {
				client_log_error (
					"Failed to push a new job to client's %s app_packet_handler!",
					packet->client->name
				);
			}
		}
	}

	else {
		client_log_warning (
			"Client %s does not have a app_packet_handler!",
			packet->client->name
		);
	}

}

// handles a PACKET_TYPE_APP_ERROR packet type
static void client_app_error_packet_handler (Packet *packet) {

	if (packet->client->app_error_packet_handler) {
		if (packet->client->app_error_packet_handler->direct_handle) {
			// printf ("app_error_packet_handler - direct handle!\n");
			packet->client->app_error_packet_handler->handler (packet);
			packet_delete (packet);
		}

		else {
			// add the packet to the handler's job queueu to be handled
			// as soon as the handler is available
			if (job_queue_push (
				packet->client->app_error_packet_handler->job_queue,
				job_create (NULL, packet)
			)) {
				client_log_error (
					"Failed to push a new job to client's %s app_error_packet_handler!",
					packet->client->name
				);
			}
		}
	}

	else {
		client_log_warning (
			"Client %s does not have a app_error_packet_handler!",
			packet->client->name
		);
	}

}

// handles a PACKET_TYPE_CUSTOM packet type
static void client_custom_packet_handler (Packet *packet) {

	if (packet->client->custom_packet_handler) {
		if (packet->client->custom_packet_handler->direct_handle) {
			// printf ("custom_packet_handler - direct handle!\n");
			packet->client->custom_packet_handler->handler (packet);
			packet_delete (packet);
		}

		else {
			// add the packet to the handler's job queueu to be handled
			// as soon as the handler is available
			if (job_queue_push (
				packet->client->custom_packet_handler->job_queue,
				job_create (NULL, packet)
			)) {
				client_log_error (
					"Failed to push a new job to client's %s custom_packet_handler!",
					packet->client->name
				);
			}
		}
	}

	else {
		client_log_warning (
			"Client %s does not have a custom_packet_handler!",
			packet->client->name
		);
	}

}

// the client handles a packet based on its type
static ClientHandlerError client_packet_handler_actual (
	Packet *packet
) {

	ClientHandlerError error = CLIENT_HANDLER_ERROR_NONE;	

	switch (packet->header.packet_type) {
		case PACKET_TYPE_NONE: break;

		// handles cerver type packets
		case PACKET_TYPE_CERVER:
			packet->client->stats->received_packets->n_cerver_packets += 1;
			packet->connection->stats->received_packets->n_cerver_packets += 1;
			error = client_cerver_packet_handler (packet);
			packet_delete (packet);
			break;

		// handles a client type packet
		case PACKET_TYPE_CLIENT:
			error = client_client_packet_handler (packet);
			break;

		// handles an error from the server
		case PACKET_TYPE_ERROR:
			packet->client->stats->received_packets->n_error_packets += 1;
			packet->connection->stats->received_packets->n_error_packets += 1;
			client_error_packet_handler (packet);
			packet_delete (packet);
			break;

		// handles a request made from the server
		case PACKET_TYPE_REQUEST:
			packet->client->stats->received_packets->n_request_packets += 1;
			packet->connection->stats->received_packets->n_request_packets += 1;
			client_request_packet_handler (packet);
			packet_delete (packet);
			break;

		// handles authentication packets
		case PACKET_TYPE_AUTH:
			packet->client->stats->received_packets->n_auth_packets += 1;
			packet->connection->stats->received_packets->n_auth_packets += 1;
			client_auth_packet_handler (packet);
			packet_delete (packet);
			break;

		// handles a game packet sent from the server
		case PACKET_TYPE_GAME:
			packet->client->stats->received_packets->n_game_packets += 1;
			packet->connection->stats->received_packets->n_game_packets += 1;
			packet_delete (packet);
			break;

		// user set handler to handler app specific packets
		case PACKET_TYPE_APP:
			packet->client->stats->received_packets->n_app_packets += 1;
			packet->connection->stats->received_packets->n_app_packets += 1;
			client_app_packet_handler (packet);
			break;

		// user set handler to handle app specific errors
		case PACKET_TYPE_APP_ERROR:
			packet->client->stats->received_packets->n_app_error_packets += 1;
			packet->connection->stats->received_packets->n_app_error_packets += 1;
			client_app_error_packet_handler (packet);
			break;

		// custom packet hanlder
		case PACKET_TYPE_CUSTOM:
			packet->client->stats->received_packets->n_custom_packets += 1;
			packet->connection->stats->received_packets->n_custom_packets += 1;
			client_custom_packet_handler (packet);
			break;

		// handles a test packet form the cerver
		case PACKET_TYPE_TEST:
			packet->client->stats->received_packets->n_test_packets += 1;
			packet->connection->stats->received_packets->n_test_packets += 1;
			client_log (LOG_TYPE_TEST, LOG_TYPE_NONE, "Got a test packet from cerver");
			packet_delete (packet);
			break;

		default:
			packet->client->stats->received_packets->n_bad_packets += 1;
			packet->connection->stats->received_packets->n_bad_packets += 1;
			#ifdef CLIENT_DEBUG
			client_log (
				LOG_TYPE_WARNING, LOG_TYPE_NONE,
				"Got a packet of unknown type"
			);
			#endif
			packet_delete (packet);
			break;
	}

	return error;

}

static ClientHandlerError client_packet_handler_check_version (
	Packet *packet
) {

	ClientHandlerError error = CLIENT_HANDLER_ERROR_NONE;

	// we expect the packet version in the packet's data
	if (packet->data) {
		(void) memcpy (&packet->version, packet->data_ptr, sizeof (PacketVersion));
		packet->data_ptr += sizeof (PacketVersion);
		
		// TODO: return errors to cerver/client
		// TODO: drop client on max bad packets
		if (packet_check (packet)) {
			error = CLIENT_HANDLER_ERROR_PACKET;
		}
	}

	else {
		client_log_error (
			"client_packet_handler () - No packet version to check!"
		);
		
		// TODO: add to bad packets count

		error = CLIENT_HANDLER_ERROR_PACKET;
	}

	return error;

}

u8 client_packet_handler (Packet *packet) {

	u8 retval = 1;

	// update general stats
	packet->client->stats->n_packets_received += 1;

	ClientHandlerError error = CLIENT_HANDLER_ERROR_NONE;
	if (packet->client->check_packets) {
		if (!client_packet_handler_check_version (packet)) {
			error = client_packet_handler_actual (packet);
		}
	}

	else {
		error = client_packet_handler_actual (packet);
	}

	switch (error) {
		case CLIENT_HANDLER_ERROR_NONE:
			retval = 0;
			break;

		default: break;
	}

	return retval;

}

static void client_receive_handle_buffer_actual (
	ReceiveHandle *receive_handle,
	char *end, size_t buffer_pos,
	size_t remaining_buffer_size
) {

	PacketHeader *header = NULL;
	size_t packet_size = 0;

	Packet *packet = NULL;

	u8 stop_handler = 0;

	#ifdef CLIENT_RECEIVE_DEBUG
	(void) printf ("WHILE has started!\n\n");
	#endif

	do {
		#ifdef CLIENT_RECEIVE_DEBUG
		(void) printf ("[0] remaining_buffer_size: %lu\n", remaining_buffer_size);
		(void) printf ("[0] buffer pos: %lu\n", buffer_pos);
		#endif

		switch (receive_handle->state) {
			// check if we have a complete packet header in the buffer
			case RECEIVE_HANDLE_STATE_NORMAL: {
				if (remaining_buffer_size >= sizeof (PacketHeader)) {
					#ifdef CLIENT_RECEIVE_DEBUG
					(void) printf (
						"Complete header in current buffer\n"
					);
					#endif

					header = (PacketHeader *) end;
					end += sizeof (PacketHeader);
					buffer_pos += sizeof (PacketHeader);

					#ifdef CLIENT_RECEIVE_DEBUG
					packet_header_print (header);
					(void) printf ("[1] buffer pos: %lu\n", buffer_pos);
					#endif

					packet_size = header->packet_size;
					remaining_buffer_size -= sizeof (PacketHeader);
				}

				// we need to handle just a part of the header
				else {
					#ifdef CLIENT_RECEIVE_DEBUG
					(void) printf (
						"Only %lu of %lu header bytes left in buffer\n",
						remaining_buffer_size, sizeof (PacketHeader)
					);
					#endif

					// reset previous header
					(void) memset (&receive_handle->header, 0, sizeof (PacketHeader));

					// the remaining buffer must contain a part of the header
					// so copy it to our aux structure
					receive_handle->header_end = (char *) &receive_handle->header;
					(void) memcpy (
						receive_handle->header_end, (void *) end, remaining_buffer_size
					);

					// for (size_t i = 0; i < sizeof (PacketHeader); i++)
					// 	printf ("%4x", (unsigned int) receive_handle->header_end[i]);

					// printf ("\n");

					// for (size_t i = 0; i < sizeof (PacketHeader); i++) {
					// 	printf ("%4x", (unsigned int) *end);
					// 	end += 1;
					// }

					// printf ("\n");

					// packet_header_print (&receive_handle->header);

					// pointer to the last byte of the new header
					receive_handle->header_end += remaining_buffer_size;

					// keep track of how much header's data we are missing
					receive_handle->remaining_header =
						sizeof (PacketHeader) - remaining_buffer_size;

					buffer_pos += remaining_buffer_size;

					#ifdef CLIENT_RECEIVE_DEBUG
					(void) printf ("[1] buffer pos: %lu\n", buffer_pos);
					#endif

					receive_handle->state = RECEIVE_HANDLE_STATE_SPLIT_HEADER;

					#ifdef CLIENT_RECEIVE_DEBUG
					(void) printf ("while loop should end now!\n");
					#endif
				}
			} break;

			// we already have a complete header from the spare packet
			// we just need to check if it is correct
			case RECEIVE_HANDLE_STATE_COMP_HEADER: {
				header = &receive_handle->header;
				packet_size = header->packet_size;
				// remaining_buffer_size -= buffer_pos;

				receive_handle->state = RECEIVE_HANDLE_STATE_NORMAL;
			} break;

			default: break;
		}

		#ifdef CLIENT_RECEIVE_DEBUG
		(void) printf (
			"State BEFORE CHECKING for packet size: %s\n",
			receive_handle_state_to_string (receive_handle->state)
		);
		#endif

		if (
			(receive_handle->state == RECEIVE_HANDLE_STATE_NORMAL)
			|| (receive_handle->state == RECEIVE_HANDLE_STATE_LOST)
		) {
			// TODO: make max value a variable
			// check that we have a valid packet size
			if ((packet_size > 0) && (packet_size < 65536)) {
				// we can safely process the complete packet
				packet = packet_create_with_data (
					header->packet_size - sizeof (PacketHeader)
				);

				// set packet's values
				(void) memcpy (&packet->header, header, sizeof (PacketHeader));
				// packet->cerver = receive_handle->cerver;
				packet->client = receive_handle->client;
				packet->connection = receive_handle->connection;
				// packet->lobby = receive_handle->lobby;

				packet->packet_size = packet->header.packet_size;

				if (packet->data_size == 0) {
					#ifdef CLIENT_RECEIVE_DEBUG
					(void) printf (
						"Packet has no more data\n"
					);
					#endif

					// we can safely handle the packet
					stop_handler = client_packet_handler (packet);

					#ifdef CLIENT_RECEIVE_DEBUG
					(void) printf ("[2] buffer pos: %lu\n", buffer_pos);
					#endif
				}

				// check how much of the packet's data is in the current buffer
				else if (packet->data_size <= remaining_buffer_size) {
					#ifdef CLIENT_RECEIVE_DEBUG
					(void) printf (
						"Complete packet in current buffer\n"
					);
					#endif

					// the full packet's data is in the current buffer
					// so we can safely copy the complete packet
					(void) memcpy (packet->data, end, packet->data_size);

					// we can safely handle the packet
					stop_handler = client_packet_handler (packet);

					// update buffer positions & values
					end += packet->data_size;
					buffer_pos += packet->data_size;
					remaining_buffer_size -= packet->data_size;

					#ifdef CLIENT_RECEIVE_DEBUG
					(void) printf ("[2] buffer pos: %lu\n", buffer_pos);
					#endif
				}

				else {
					// just some part of the packet's data is in the current buffer
					// we should copy all the remaining buffer and wait for the next read
					#ifdef CLIENT_RECEIVE_DEBUG
					(void) printf ("RECEIVE_HANDLE_STATE_SPLIT_PACKET\n");
					#endif
					
					if (remaining_buffer_size > 0) {
						#ifdef CLIENT_RECEIVE_DEBUG
						(void) printf (
							"We can only get %lu / %lu from the current buffer\n",
							remaining_buffer_size, packet->data_size
						);
						#endif

						// TODO: handle errors
						(void) packet_add_data (
							packet, end, remaining_buffer_size
						);

						// update buffer positions & values
						end += packet->data_size;
						buffer_pos += packet->data_size;
						remaining_buffer_size -= packet->data_size;
					}

					else {
						#ifdef CLIENT_RECEIVE_DEBUG
						(void) printf (
							"We have NO more data left in current buffer\n"
						);
						#endif
					}

					// set the newly created packet as spare
					receive_handle->spare_packet = packet;

					receive_handle->state = RECEIVE_HANDLE_STATE_SPLIT_PACKET;

					#ifdef CLIENT_RECEIVE_DEBUG
					(void) printf ("while loop should end now!\n");
					#endif
				}
			}

			else {
				// we must likely have a bad packet
				// we need to keep reading the buffer until we find
				// the start of the next one and we can continue
				#ifdef CLIENT_RECEIVE_DEBUG
				client_log (
					LOG_TYPE_WARNING, LOG_TYPE_PACKET,
					"Got a packet of invalid size: %ld", packet_size
				);
				#endif

				#ifdef CLIENT_RECEIVE_DEBUG
				(void) printf ("\n\nWE ARE LOST!\n\n");
				#endif

				receive_handle->state = RECEIVE_HANDLE_STATE_LOST;

				// FIXME: this is just for testing!
				break;
			}
		}

		// reset common loop values
		header = NULL;
		packet = NULL;
	} while ((buffer_pos < receive_handle->received_size) && !stop_handler);

	#ifdef CLIENT_RECEIVE_DEBUG
	(void) printf ("WHILE has ended!\n\n");
	#endif

}

static void client_receive_handle_buffer (
	ReceiveHandle *receive_handle
) {

	char *end = receive_handle->buffer;
	size_t buffer_pos = 0;

	size_t remaining_buffer_size = receive_handle->received_size;

	u8 stop_handler = 0;

	#ifdef CLIENT_RECEIVE_DEBUG
	(void) printf ("Received size: %lu\n", receive_handle->received_size);

	(void) printf (
		"State BEFORE checking for SPARE PARTS: %s\n",
		receive_handle_state_to_string (receive_handle->state)
	);
	#endif

		// check if we have any spare parts 
	switch (receive_handle->state) {
		// check if we have a spare header
		// that was incompleted from the last buffer
		case RECEIVE_HANDLE_STATE_SPLIT_HEADER: {
			// copy the remaining header size
			(void) memcpy (
				receive_handle->header_end,
				(void *) end,
				receive_handle->remaining_header
			);

			#ifdef CLIENT_RECEIVE_DEBUG
			(void) printf (
				"Copied %u missing header bytes\n",
				receive_handle->remaining_header
			);
			#endif

			// receive_handle->header_end = (char *) &receive_handle->header;
			// for (size_t i = 0; i < receive_handle->remaining_header; i++)
			// 	(void) printf ("%4x", (unsigned int) receive_handle->header_end[i]);

			// (void) printf ("\n");
			
			#ifdef CLIENT_RECEIVE_DEBUG
			packet_header_print (&receive_handle->header);
			#endif

			// update buffer positions
			end += receive_handle->remaining_header;
			buffer_pos += receive_handle->remaining_header;

			// update how much we have still left to handle from the current buffer
			remaining_buffer_size -= receive_handle->remaining_header;

			// reset receive handler values
			receive_handle->header_end = NULL;
			receive_handle->remaining_header = 0;

			// we can expect to get the packet's data from the current buffer
			receive_handle->state = RECEIVE_HANDLE_STATE_COMP_HEADER;

			#ifdef CLIENT_RECEIVE_DEBUG
			(void) printf ("We have a COMPLETE HEADER!\n");
			#endif
		} break;

		// check if we have a spare packet
		case RECEIVE_HANDLE_STATE_SPLIT_PACKET: {
			// check if the current buffer is big enough
			if (
				receive_handle->spare_packet->remaining_data <= receive_handle->received_size
			) {
				size_t to_copy_data_size = receive_handle->spare_packet->remaining_data; 
				
				// copy packet's remaining data
				(void) packet_add_data (
					receive_handle->spare_packet,
					end,
					receive_handle->spare_packet->remaining_data
				);

				#ifdef CLIENT_RECEIVE_DEBUG
				(void) printf (
					"Copied %lu missing packet bytes\n",
					to_copy_data_size
				);

				(void) printf ("Spare packet is COMPLETED!\n");
				#endif

				// we can safely handle the packet
				stop_handler = client_packet_handler (
					receive_handle->spare_packet
				);

				// update buffer positions
				end += to_copy_data_size;
				buffer_pos += to_copy_data_size;

				// update how much we have still left to handle from the current buffer
				remaining_buffer_size -= to_copy_data_size;

				// we still need to process more data from the buffer
				receive_handle->state = RECEIVE_HANDLE_STATE_NORMAL;
			}

			else {
				#ifdef CLIENT_RECEIVE_DEBUG
				(void) printf (
					"We can only get %lu / %lu of the remaining packet's data\n",
					receive_handle->spare_packet->remaining_data,
					receive_handle->received_size
				);
				#endif

				// copy the complete buffer
				(void) packet_add_data (
					receive_handle->spare_packet,
					end,
					receive_handle->received_size
				);

				#ifdef CLIENT_RECEIVE_DEBUG
				(void) printf (
					"We are still missing %lu to complete the packet!\n",
					receive_handle->spare_packet->remaining_data
				);
				#endif
			}
		} break;

		default: break;
	}

	#ifdef CLIENT_RECEIVE_DEBUG
	(void) printf (
		"State BEFORE LOOP: %s\n",
		receive_handle_state_to_string (receive_handle->state)
	);
	#endif

	if (
		!stop_handler
		&& (buffer_pos < receive_handle->received_size)
		&& (
			receive_handle->state == RECEIVE_HANDLE_STATE_NORMAL
			|| (receive_handle->state == RECEIVE_HANDLE_STATE_COMP_HEADER)
		)
	) {
		client_receive_handle_buffer_actual (
			receive_handle,
			end, buffer_pos,
			remaining_buffer_size
		);
	}

}

// handles a failed recive from a connection associatd with a client
// end sthe connection to prevent seg faults or signals for bad sock fd
static void client_receive_handle_failed (
	Client *client, Connection *connection
) {

	if (connection->active) {
		if (!client_connection_end (client, connection)) {
			// check if the client has any other active connection
			if (client->connections->size <= 0) {
				client->running = false;
			}
		}
	}

}

// performs the actual recv () method on the connection's sock fd
// handles if the receive method failed
// the amount of bytes read from the socket is placed in rc
ReceiveError client_receive_actual (
	Client *client, Connection *connection,
	char *buffer, const size_t buffer_size,
	size_t *rc
) {

	ReceiveError error = RECEIVE_ERROR_NONE;

	ssize_t received = recv (
		connection->socket->sock_fd,
		buffer, buffer_size,
		0
	);

	switch (received) {
		case -1: {
			if (errno == EAGAIN) {
				#ifdef SOCKET_DEBUG
				client_log (
					LOG_TYPE_DEBUG, LOG_TYPE_CLIENT,
					"client_receive_internal () - connection %s sock fd: %d timed out",
					connection->name, connection->socket->sock_fd
				);
				#endif

				error = RECEIVE_ERROR_TIMEOUT;
			}

			else {
				#ifdef CONNECTION_DEBUG
				client_log (
					LOG_TYPE_ERROR, LOG_TYPE_CLIENT,
					"client_receive_internal () - rc < 0 - connection %s sock fd: %d",
					connection->name, connection->socket->sock_fd
				);

				perror ("Error ");
				#endif

				client_receive_handle_failed (client, connection);

				error = RECEIVE_ERROR_FAILED;
			}
		} break;

		case 0: {
			#ifdef CONNECTION_DEBUG
			client_log (
				LOG_TYPE_DEBUG, LOG_TYPE_CLIENT,
				"client_receive_internal () - rc == 0 - connection %s sock fd: %d",
				connection->name, connection->socket->sock_fd
			);

			// perror ("Error ");
			#endif

			client_receive_handle_failed (client, connection);

			error = RECEIVE_ERROR_EMPTY;
		} break;

		default: {
			// #ifdef CLIENT_RECEIVE_DEBUG
			client_log (
				LOG_TYPE_DEBUG, LOG_TYPE_CLIENT,
				"client_receive_actual () - received %ld from connection %s",
				received, connection->name
			);
			// #endif
		} break;
	}

	*rc = (size_t) ((received > 0) ? received : 0);

	return error;

}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

// request to read x amount of bytes from the connection's sock fd
// into the specified buffer
// this method will only return once the requested bytes
// have been received or on any error
ReceiveError client_receive_data (
	Client *client, Connection *connection,
	char *buffer, const size_t buffer_size,
	size_t requested_data
) {

	ReceiveError error = RECEIVE_ERROR_NONE;
	size_t received = 0;

	size_t data_size = requested_data;

	char *buffer_end = buffer;
	// size_t buffer_pos = 0;

	do {
		error = client_receive_actual (
			client, connection,
			buffer_end, data_size,
			&received
		);

		if (error == RECEIVE_ERROR_NONE) {
			// we got some data
			data_size -= received;

			buffer_end += received;
		}

		else if (RECEIVE_ERROR_TIMEOUT) {
			// we are still waiting to get more data
		}

		else {
			// an error has ocurred or we have been disconnected
			// so end the loop
			break;
		}
	} while (data_size > 0);

	return (data_size > 0) ? RECEIVE_ERROR_FAILED : RECEIVE_ERROR_NONE;

}

#pragma GCC diagnostic pop

// receive data from connection's socket
// this method does not perform any checks and expects a valid buffer
// to handle incomming data
// returns 0 on success, 1 on error
unsigned int client_receive_internal (
	Client *client, Connection *connection,
	char *buffer, const size_t buffer_size
) {

	unsigned int retval = 1;

	size_t received = 0;
	
	ReceiveError error = client_receive_actual (
		client, connection,
		buffer, buffer_size,
		&received
	);

	client->stats->n_receives_done += 1;
	client->stats->total_bytes_received += received;

	#ifdef CONNECTION_STATS
	connection->stats->n_receives_done += 1;
	connection->stats->total_bytes_received += received;
	#endif

	switch (error) {
		case RECEIVE_ERROR_NONE: {
			connection->receive_handle.buffer = buffer;
			connection->receive_handle.buffer_size = buffer_size;
			connection->receive_handle.received_size = received;

			client_receive_handle_buffer (
				&connection->receive_handle
			);

			retval = 0;
		} break;

		case RECEIVE_ERROR_TIMEOUT: {
			retval = 0;
		};

		default: break;
	}

	return retval;

}

// allocates a new packet buffer to receive incoming data from the connection's socket
// returns 0 on success handle
// returns 1 if any error ocurred and must likely the connection was ended
unsigned int client_receive (
	Client *client, Connection *connection
) {

	unsigned int retval = 1;

	if (client && connection) {
		char *packet_buffer = (char *) calloc (
			connection->receive_packet_buffer_size, sizeof (char)
		);

		if (packet_buffer) {
			retval = client_receive_internal (
				client, connection,
				packet_buffer, connection->receive_packet_buffer_size
			);

			free (packet_buffer);
		}

		else {
			// #ifdef CLIENT_DEBUG
			client_log (
				LOG_TYPE_ERROR, LOG_TYPE_CONNECTION,
				"client_receive () - Failed to allocate a new packet buffer!"
			);
			// #endif
		}
	}

	return retval;

}