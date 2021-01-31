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

typedef enum AppRequest {

	TEST_MSG		= 0

} AppRequest;

static Client *client = NULL;
static Connection *connection = NULL;

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

static void *client_event_connection_close (void *client_event_data_ptr) {

	if (client_event_data_ptr) {
		ClientEventData *client_event_data = (ClientEventData *) client_event_data_ptr;

		if (client_event_data->connection) {
			client_log_warning (
				"client_event_connection_close () - connection <%s> has been closed!",
				client_event_data->connection->name
			);
		}

		client_event_data_delete (client_event_data);
	}

	return NULL;

}

static void *client_error_failed_auth (void *client_error_data_ptr) {

	if (client_error_data_ptr) {
		ClientErrorData *client_error_data = (ClientErrorData *) client_error_data_ptr;

		if (client_error_data->connection) {
			client_log_error (
				"client_error_failed_auth () - connection <%s> failed to authenticate!",
				client_error_data->connection->name
			);
		}

		client_error_data_delete (client_error_data);
	}

	return NULL;

}

static void *client_event_success_auth (void *client_event_data_ptr) {

	if (client_event_data_ptr) {
		ClientEventData *client_event_data = (ClientEventData *) client_event_data_ptr;

		if (client_event_data->connection) {
			client_log_success (
				"client_event_success_auth () - connection <%s> has been authenticated!",
				client_event_data->connection->name
			);
		}

		client_event_data_delete (client_event_data);
	}

	return NULL;

}

static int cerver_connect (const char *ip, unsigned int port) {

	int retval = 1;

	if (ip) {
		fprintf (stdout, "\nConnecting to cerver...\n");

		client = client_create ();
		if (client) {
			Handler *app_packet_handler = handler_create (app_handler);
			handler_set_direct_handle (app_packet_handler, true);
			client_set_app_handlers (client, app_packet_handler, NULL);

			client_event_register (
				client, 
				CLIENT_EVENT_CONNECTION_CLOSE, 
				client_event_connection_close, NULL, NULL, 
				false, false
			);

			client_error_register (
				client,
				CLIENT_ERROR_FAILED_AUTH,
				client_error_failed_auth, NULL, NULL,
				false, false
			);

			client_event_register (
				client,
				CLIENT_EVENT_SUCCESS_AUTH,
				client_event_success_auth, NULL, NULL,
				false, false
			);

			connection = client_connection_create (client, ip, port, PROTOCOL_TCP, false);
			if (connection) {
				connection_set_name (connection, "main");
				connection_set_max_sleep (connection, 30);

				// auth configuration
				Credentials *credentials = credentials_new ("ermiry", "hola12");
				connection_set_auth_data (
					connection, 
					credentials, sizeof (Credentials), 
					credentials_delete,
					true
				);

				if (!client_connect_and_start (client, connection)) {
					client_log (LOG_TYPE_SUCCESS, LOG_TYPE_NONE, "Connected to cerver!");
					retval = 0;
				}

				else {
					client_log (LOG_TYPE_ERROR, LOG_TYPE_NONE, "Failed to connect to cerver!");
				}
			}

			else {
				client_log (LOG_TYPE_ERROR, LOG_TYPE_NONE, "Failed to create connection!");
			}
		}

		else {
			client_log (LOG_TYPE_ERROR, LOG_TYPE_NONE, "Failed to create client!");
		}
	}

	return retval;

}

static void cerver_disconnect (void) {

	client_connection_end (client, connection);

	if (!client_teardown (client)) {
		client_log_success ("client_teardown ()!");
	}

}

#pragma endregion

#pragma region handler

static void app_handler (void *packet_ptr) {

	if (packet_ptr) {
		Packet *packet = (Packet *) packet_ptr;
		
		switch (packet->header.request_type) {
			case TEST_MSG: client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Got a test message from cerver!"); break;

			default: 
				client_log (LOG_TYPE_WARNING, LOG_TYPE_NONE, "Got an unknown app request.");
				break;
		}
	}

}

#pragma endregion

#pragma region request

static int test_msg_send (void) {

	int retval = 1;

	if ((client->running) && (connection->active)) {
		Packet *packet = packet_generate_request (PACKET_TYPE_APP, TEST_MSG, NULL, 0);
		if (packet) {
			packet_set_network_values (packet, client, connection);
			size_t sent = 0;
			if (packet_send (packet, 0, &sent, false)) {
				client_log (LOG_TYPE_ERROR, LOG_TYPE_NONE, "Failed to send test to cerver");
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

	client_end ();

	exit (0);

}

int main (int argc, const char **argv) {

	// register to the quit signal
	signal (SIGINT, end);

	client_init ();

	cerver_client_version_print_full ();

	client_log_debug ("Basic Admin Example");
	printf ("\n");

	if (!cerver_connect ("127.0.0.1", 7000)) {
		while (1) {
			// send a test message every second
			// if (connection->authenticated) {
				test_msg_send ();
			// }

			sleep (1);
		}
	}

	client_end ();

	return 0;

}

#pragma endregion