#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>

#include "client/version.h"

#include "client/client.h"
#include "client/packets.h"
#include "client/events.h"
#include "client/errors.h"

#include "client/utils/utils.h"
#include "client/utils/log.h"

#define CERVER_IP			"127.0.0.1"
#define CERVER_PORT			8007

typedef enum AppRequest {

	TEST_MSG		= 0

} AppRequest;

static Client *client = NULL;
static Connection *connection = NULL;
static Connection *connection_with_session_id = NULL;

static u8 cerver_connect_with_session_id (void);

static void app_handler (void *packet_ptr);

#pragma region auth

typedef struct Credentials {

	char username[64];
	char password[64];

} Credentials;

Credentials *credentials_new (const char *username, const char *password) {

	Credentials *credentials = (Credentials *) malloc (sizeof (Credentials));
	if (credentials) {
		strncpy (credentials->username, username, 64);
		strncpy (credentials->password, password, 64);
	}

	return credentials;

}

void credentials_delete (void *credentials_ptr) { if (credentials_ptr) free (credentials_ptr); }

#pragma endregion

#pragma region connect

static void client_event_connection_close (void *client_event_data_ptr) {

	if (client_event_data_ptr) {
		ClientEventData *client_event_data = (ClientEventData *) client_event_data_ptr;

		if (client_event_data->connection) {
			char *status = c_string_create ("client_event_connection_close () - connection <%s> has been closed!",
				client_event_data->connection->name->str);
			if (status) {
				client_log_warning (status);
				free (status);
			}
		}

		client_event_data_delete (client_event_data);
	}

}

static void client_error_failed_auth (void *client_error_data_ptr) {

	if (client_error_data_ptr) {
		ClientErrorData *client_error_data = (ClientErrorData *) client_error_data_ptr;

		if (client_error_data->connection) {
			char *status = c_string_create ("client_error_failed_auth () - connection <%s> failed to authenticate!",
				client_error_data->connection->name->str);
			if (status) {
				client_log_error (status);
				free (status);
			}
		}

		client_error_data_delete (client_error_data);
	}

}

static void client_event_success_auth (void *client_event_data_ptr) {

	if (client_event_data_ptr) {
		ClientEventData *client_event_data = (ClientEventData *) client_event_data_ptr;

		if (client_event_data->connection) {
			char *status = c_string_create ("client_event_success_auth () - connection <%s> has been authenticated!",
				client_event_data->connection->name->str);
			if (status) {
				client_log_success (status);
				free (status);
			}
		}

		if (!connection_with_session_id) {
			client_log_debug ("Creating new connection using the session id...");
			cerver_connect_with_session_id ();
		}

		client_event_data_delete (client_event_data);
	}

}

static int cerver_connect (void) {

	int retval = 1;

	fprintf (stdout, "\nConnecting to cerver...\n");

	client = client_create ();
	if (client) {
		// client_set_name (client, "main");
		client_set_app_handlers (client, app_handler, NULL);

		client_event_register (
			client, 
			CLIENT_EVENT_CONNECTION_CLOSE, 
			client_event_connection_close, NULL, NULL, 
			false, false
		);

		client_error_register (
			client,
			CLIENT_ERR_FAILED_AUTH,
			client_error_failed_auth, NULL, NULL,
			false, false
		);

		client_event_register (
			client,
			CLIENT_EVENT_SUCCESS_AUTH,
			client_event_success_auth, NULL, NULL,
			true, false
		);

		connection = client_connection_create (client, CERVER_IP, CERVER_PORT, PROTOCOL_TCP, false);
		if (connection) {
			connection_set_name (connection, "main");
			connection_set_max_sleep (connection, 30);

			// auth configuration
			Credentials *credentials = credentials_new ("ermiry", "hola12");
			connection_set_auth_data (
				connection, 
				credentials, sizeof (Credentials), 
				credentials_delete,
				false
			);

			if (!client_connect_and_start (client, connection)) {
				client_log_msg (stdout, LOG_SUCCESS, LOG_NO_TYPE, "Connected to cerver!");
				retval = 0;
			}

			else {
				client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to connect to cerver!");
			}
		}

		else {
			client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to create connection!");
		}
	}

	else {
		client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to create client!");
	}

	return retval;

}

// use the received session id to create a new connection to the cerver
// returns 0 on succes, 1 on error
static u8 cerver_connect_with_session_id (void) {

	u8 retval = 1;

	connection_with_session_id = client_connection_create (client, CERVER_IP, CERVER_PORT, PROTOCOL_TCP, false);
	if (connection_with_session_id) {
		connection_set_name (connection_with_session_id, "sessions");
		connection_set_max_sleep (connection_with_session_id, 30);

		// set the recived session id as the connection's auth data
		if (client->session_id) {
			SToken *s_token = (SToken *) malloc (sizeof (SToken));
			strncpy (s_token->token, client->session_id->str, TOKEN_SIZE);

			connection_set_auth_data (
				connection_with_session_id, 
				s_token, sizeof (SToken), 
				NULL,
				false
			);

			if (!client_connect_and_start (client, connection_with_session_id)) {
				client_log_msg (stdout, LOG_SUCCESS, LOG_NO_TYPE, "Connected to cerver!");
				retval = 0;
			}

			else {
				client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to connect to cerver!");
			}
		}

		else {
			client_log_error ("Client does not have a session id!");
		}
	}

	else {
		client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to create connection!");
	}

	return retval;

}

static void cerver_disconnect (void) {

	client_connection_end (client, connection);
	client_connection_end (client, connection_with_session_id);

	if (!client_teardown (client)) {
		client_log_success ("client_teardown ()!");
	}

}

#pragma endregion

#pragma region handler

static void app_handler (void *packet_ptr) {

	if (packet_ptr) {
		Packet *packet = (Packet *) packet_ptr;
		if (packet) {
			if (packet->data_size >= sizeof (RequestData)) {
				RequestData *req = (RequestData *) (packet->data);

				switch (req->type) {
					case TEST_MSG: client_log_msg (stdout, LOG_DEBUG, LOG_NO_TYPE, "Got a test message from cerver!"); break;

					default: 
						client_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE, "Got an unknown app request.");
						break;
				}
			}
		}
	}

}

#pragma endregion

#pragma region request

static int test_msg_send (Connection *con) {

	int retval = 1;

	if ((client->running) && (con->connected)) {
		Packet *packet = packet_generate_request (APP_PACKET, TEST_MSG, NULL, 0);
		if (packet) {
			packet_set_network_values (packet, client, con);
			size_t sent = 0;
			if (packet_send (packet, 0, &sent, false)) {
				client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to send test to cerver");
			}

			else {
				printf ("Test sent to cerver: %ld\n", sent);
				retval = 0;
			} 

			packet_delete (packet);
		}
	}

	return retval;

}

#pragma endregion

#pragma region main

static void end (int dummy) {
	
	cerver_disconnect ();

	printf ("\n");
	client_log_success ("Done!");
	printf ("\n");

	exit (0);

}

int main (int argc, const char **argv) {

	// register to the quit signal
	signal (SIGINT, end);

	cerver_client_version_print_full ();

	client_log_debug ("Basic Auth Example");
	printf ("\n");

	if (!cerver_connect ()) {
		while (1) {
			// send a test message every second
			// if (connection->authenticated) {
			// 	test_msg_send (connection);
			// }

			if (connection_with_session_id) {
				if (connection_with_session_id->authenticated) {
					test_msg_send (connection_with_session_id);
				}
			}

			sleep (1);
		}
	}

	return 0;

}

#pragma endregion