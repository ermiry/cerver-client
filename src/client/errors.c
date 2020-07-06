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