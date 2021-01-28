#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <time.h>

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/collections/dlist.h"

#include "client/client.h"
#include "client/connection.h"
#include "client/errors.h"
#include "client/packets.h"

#include "client/threads/thread.h"

#include "client/utils/utils.h"
#include "client/utils/log.h"

u8 client_error_unregister (Client *client, const ClientErrorType error_type);

#pragma region types

// get the description for the current error type
const char *client_error_type_description (ClientErrorType type) {

	switch (type) {
		#define XX(num, name, description) case CLIENT_ERROR_##name: return #description;
		CLIENT_ERROR_MAP(XX)
		#undef XX
	}

	return client_error_type_description (CLIENT_ERROR_UNKNOWN);

}


#pragma endregion

#pragma region data

static ClientErrorData *client_error_data_new (void) {

	ClientErrorData *error_data = (ClientErrorData *) malloc (sizeof (ClientErrorData));
	if (error_data) {
		error_data->client = NULL;
		error_data->connection = NULL;

		error_data->action_args = NULL;

		error_data->error_message = NULL;
	}

	return error_data;

}

void client_error_data_delete (ClientErrorData *error_data) {

	if (error_data) {
		str_delete (error_data->error_message);

		free (error_data);
	}

}

static ClientErrorData *client_error_data_create (
	const Client *client, const Connection *connection,
	void *args,
	const char *error_message
) {

	ClientErrorData *error_data = client_error_data_new ();
	if (error_data) {
		error_data->client = client;
		error_data->connection = connection;

		error_data->action_args = args;

		error_data->error_message = error_message ? str_new (error_message) : NULL;
	}

	return error_data;

}

#pragma endregion

#pragma region errors

static ClientError *client_error_new (void) {

	ClientError *client_error = (ClientError *) malloc (sizeof (ClientError));
	if (client_error) {
		client_error->type = CLIENT_ERROR_NONE;

		client_error->create_thread = false;
		client_error->drop_after_trigger = false;

		client_error->action = NULL;
		client_error->action_args = NULL;
		client_error->delete_action_args = NULL;
	}

	return client_error;

}

void client_error_delete (void *client_error_ptr) {

	if (client_error_ptr) {
		ClientError *client_error = (ClientError *) client_error_ptr;

		if (client_error->action_args) {
			if (client_error->delete_action_args)
				client_error->delete_action_args (client_error->action_args);
		}

		free (client_error_ptr);
	}

}

// registers an action to be triggered when the specified error occurs
// if there is an existing action registered to an error, it will be overrided
// a newly allocated ClientErrorData structure will be passed to your method
// that should be free using the client_error_data_delete () method
// returns 0 on success, 1 on error
u8 client_error_register (
	Client *client,
	const ClientErrorType error_type,
	Action action, void *action_args, Action delete_action_args,
	bool create_thread, bool drop_after_trigger
) {

	u8 retval = 1;

	if (client) {
		ClientError *error = client_error_new ();
		if (error) {
			error->type = error_type;

			error->create_thread = create_thread;
			error->drop_after_trigger = drop_after_trigger;

			error->action = action;
			error->action_args = action_args;
			error->delete_action_args = delete_action_args;

			// search if there is an action already registred for that error and remove it
			(void) client_error_unregister (client, error_type);

			client->errors[error_type] = error;

			retval = 0;
		}
	}

	return retval;

}

// unregisters the action associated with the error types
// deletes the action args using the delete_action_args () if NOT NULL
// returns 0 on success, 1 on error or if error is NOT registered
u8 client_error_unregister (Client *client, const ClientErrorType error_type) {

	u8 retval = 1;

	if (client) {
		if (client->errors[error_type]) {
			client_error_delete (client->errors[error_type]);
			client->errors[error_type] = NULL;

			retval = 0;
		}
	}

	return retval;

}

// triggers all the actions that are registred to an error
// returns 0 on success, 1 on error
u8 client_error_trigger (
	const ClientErrorType error_type,
	const Client *client, const Connection *connection,
	const char *error_message
) {

	u8 retval = 1;

	if (client) {
		ClientError *error = client->errors[error_type];
		if (error) {
			// trigger the action
			if (error->action) {
				if (error->create_thread) {
					pthread_t thread_id = 0;
					retval = thread_create_detachable (
						&thread_id,
						(void *(*)(void *)) error->action,
						client_error_data_create (
							client, connection,
							error,
							error_message
						)
					);
				}

				else {
					error->action (client_error_data_create (
						client, connection,
						error,
						error_message
					));

					retval = 0;
				}

				if (error->drop_after_trigger) {
					(void) client_error_unregister ((Client *) client, error_type);
				}
			}
		}
	}

	return retval;

}

#pragma endregion

#pragma region handler

// handles error packets
void client_error_packet_handler (Packet *packet) {

	if (packet->data_size >= sizeof (SError)) {
		char *end = (char *) packet->data;
		SError *s_error = (SError *) end;

		switch (s_error->error_type) {
			case CLIENT_ERROR_NONE: break;

			case CLIENT_ERROR_CERVER_ERROR:
				client_error_trigger (
					CLIENT_ERROR_CERVER_ERROR,
					packet->client, packet->connection,
					s_error->msg
				);
				break;
			case CLIENT_ERROR_PACKET_ERROR:
				client_error_trigger (
					CLIENT_ERROR_PACKET_ERROR,
					packet->client, packet->connection,
					s_error->msg
				);
				break;

			case CLIENT_ERROR_FAILED_AUTH: {
				if (client_error_trigger (
					CLIENT_ERROR_FAILED_AUTH,
					packet->client, packet->connection,
					s_error->msg
				)) {
					// not error action is registered to handle the error
					client_log_error ("Failed to authenticate - %s", s_error->msg);
				}
			} break;

			case CLIENT_ERROR_GET_FILE:
				client_error_trigger (
					CLIENT_ERROR_GET_FILE,
					packet->client, packet->connection,
					s_error->msg
				);
				break;
			case CLIENT_ERROR_SEND_FILE:
				client_error_trigger (
					CLIENT_ERROR_SEND_FILE,
					packet->client, packet->connection,
					s_error->msg
				);
				break;
			case CLIENT_ERROR_FILE_NOT_FOUND:
				client_error_trigger (
					CLIENT_ERROR_FILE_NOT_FOUND,
					packet->client, packet->connection,
					s_error->msg
				);
				break;

			case CLIENT_ERROR_CREATE_LOBBY:
				client_error_trigger (
					CLIENT_ERROR_CREATE_LOBBY,
					packet->client, packet->connection,
					s_error->msg
				);
				break;
			case CLIENT_ERROR_JOIN_LOBBY:
				client_error_trigger (
					CLIENT_ERROR_JOIN_LOBBY,
					packet->client, packet->connection,
					s_error->msg
				);
				break;
			case CLIENT_ERROR_LEAVE_LOBBY:
				client_error_trigger (
					CLIENT_ERROR_LEAVE_LOBBY,
					packet->client, packet->connection,
					s_error->msg
				);
				break;
			case CLIENT_ERROR_FIND_LOBBY:
				client_error_trigger (
					CLIENT_ERROR_FIND_LOBBY,
					packet->client, packet->connection,
					s_error->msg
				);
				break;

			case CLIENT_ERROR_GAME_INIT:
				client_error_trigger (
					CLIENT_ERROR_GAME_INIT,
					packet->client, packet->connection,
					s_error->msg
				);
				break;
			case CLIENT_ERROR_GAME_START:
				client_error_trigger (
					CLIENT_ERROR_GAME_START,
					packet->client, packet->connection,
					s_error->msg
				);
				break;

			default: {
				client_error_trigger (
					CLIENT_ERROR_UNKNOWN,
					packet->client, packet->connection,
					NULL
				);
			} break;
		}
	}

}

#pragma endregion

#pragma region packets

// creates an error packet ready to be sent
Packet *error_packet_generate (const ClientErrorType type, const char *msg) {

	Packet *packet = packet_new ();
	if (packet) {
		size_t packet_len = sizeof (PacketHeader) + sizeof (SError);

		packet->packet = malloc (packet_len);
		packet->packet_size = packet_len;

		char *end = (char *) packet->packet;
		PacketHeader *header = (PacketHeader *) end;
		header->packet_type = PACKET_TYPE_ERROR;
		header->packet_size = packet_len;

		header->request_type = REQUEST_PACKET_TYPE_NONE;

		end += sizeof (PacketHeader);

		SError *s_error = (SError *) end;
		s_error->error_type = type;
		s_error->timestamp = time (NULL);
		memset (s_error->msg, 0, ERROR_MESSAGE_LENGTH);
		if (msg) strncpy (s_error->msg, msg, ERROR_MESSAGE_LENGTH);
	}

	return packet;

}

// creates and send a new error packet
// returns 0 on success, 1 on error
u8 error_packet_generate_and_send (
	const ClientErrorType type, const char *msg,
	Client *client, Connection *connection
) {

	u8 retval = 1;

	Packet *error_packet = error_packet_generate (type, msg);
	if (error_packet) {
		packet_set_network_values (error_packet, client, connection);
		retval = packet_send (error_packet, 0, NULL, false);
		packet_delete (error_packet);
	}

	return retval;

}

#pragma endregion