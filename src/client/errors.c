#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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

void client_error_unregister (Client *client, ClientErrorType error_type);

#pragma region data

static ClientErrorData *client_error_data_new (void) {

	ClientErrorData *error_data = (ClientErrorData *) malloc (sizeof (ClientErrorData));
	if (error_data) {
		error_data->client = NULL;
		error_data->connection = NULL;

		error_data->action_args = NULL;
	}

	return error_data;

}

void client_error_data_delete (ClientErrorData *error_data) {

	if (error_data) free (error_data);

}

static ClientErrorData *client_error_data_create (Client *client, Connection *connection, void *args) {

	ClientErrorData *error_data = client_error_data_new ();
	if (error_data) {
		error_data->client = client;
		error_data->connection = connection;
		error_data->action_args = args;
	}

	return error_data;

}

#pragma endregion

#pragma region errors

static ClientError *client_error_new (void) {

	ClientError *client_error = (ClientError *) malloc (sizeof (ClientError));
	if (client_error) {
		client_error->type = ERR_NONE;

		client_error->create_thread = false;
		client_error->drop_after_trigger = false;

		client_error->action = NULL;
		client_error->action_args = NULL;
		client_error->delete_action_args = NULL;
	}

	return client_error;

}

static void client_error_delete (void *client_error_ptr) {

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
u8 client_error_register (Client *client, ClientErrorType error_type,
	Action action, void *action_args, Action delete_action_args, 
    bool create_thread, bool drop_after_trigger) {

	u8 retval = 1;

	if (client) {
		if (client->registered_errors) {
			ClientError *error = client_error_new ();
			if (error) {
				error->type = error_type;

				error->create_thread = create_thread;
				error->drop_after_trigger = drop_after_trigger;

				error->action = action;
				error->action_args = action_args;
				error->delete_action_args = delete_action_args;

				// search if there is an action already registred for that error and remove it
				client_error_unregister (client, error_type);

				if (!dlist_insert_after (
					client->registered_errors,
					dlist_end (client->registered_errors),
					error
				)) {
					retval = 0;
				}
			}
		}
	}

	return retval;

}

#pragma endregion

#pragma region handler

// handles error packets
void error_packet_handler (Packet *packet) {

	if (packet) {
		if (packet->data_size >= sizeof (SError)) {
			char *end = (char *) packet->data;
			SError *s_error = (SError *) end;

			switch (s_error->error_type) {
				case ERR_CERVER_ERROR: break;
				case ERR_CREATE_LOBBY: break;
				case ERR_JOIN_LOBBY: break;
				case ERR_LEAVE_LOBBY: break;
				case ERR_FIND_LOBBY: break;
				case ERR_GAME_INIT: break;

				case ERR_FAILED_AUTH: {
					// #ifdef CLIENT_DEBUG
					client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, 
						c_string_create ("Failed to authenticate - %s", s_error->msg)); 
					// #endif
					// last_error.type = ERR_FAILED_AUTH;
					// memset (last_error.msg, 0, sizeof (last_error.msg));
					// strcpy (last_error.msg, error->msg);
					// if (pack_info->client->errorType == ERR_FAILED_AUTH)
						// pack_info->client->errorAction (pack_info->client->errorArgs);
				}
					
				break;

				default: 
					client_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE, "Unknown error received from server."); 
					break;
			}
		}
	}

}

#pragma endregion

#pragma region main

u8 client_errors_init (Client *client) {

    u8 retval = 1;

    if (client) {
        client->registered_errors = dlist_init (client_error_delete, NULL);
        retval = client->registered_errors ? 0 : 1;
    }

    return retval;

}

void client_events_end (Client *client) { 

    if (client) dlist_delete (client->registered_errors);

}

#pragma endregion