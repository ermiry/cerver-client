#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>

#include "client/version.h"

#include "client/client.h"
#include "client/packets.h"
#include "client/events.h"

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

static void client_event_connection_close (void *client_event_data_ptr) {

	if (client_event_data_ptr) {
		// ClientEventData *client_event_data = (ClientEventData *) client_event_data_ptr;

		client_log_warning ("client_event_connection_close () - connection has been closed!");
	}

}

static void client_event_success_auth (void *client_event_data_ptr) {

	if (client_event_data_ptr) {
		// ClientEventData *client_event_data = (ClientEventData *) client_event_data_ptr;

		client_log_success ("client_event_success_auth () - connection has been authenticated!");
	}

}

static int cerver_connect (const char *ip, unsigned int port) {

	int retval = 1;

	if (ip) {
		fprintf (stdout, "\nConnecting to cerver...\n");

		client = client_create ();
		if (client) {
			client_set_app_handlers (client, app_handler, NULL);

			client_event_register (
				client, 
				EVENT_CONNECTION_CLOSE, 
				client_event_connection_close, NULL, NULL, 
				false, false
			);

			client_event_register (
				client,
				EVENT_SUCCESS_AUTH,
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
					credentials_delete
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

static int test_msg_send (void) {

	int retval = 1;

	if ((client->running) && (connection->connected)) {
		Packet *packet = packet_generate_request (APP_PACKET, TEST_MSG, NULL, 0);
		if (packet) {
			packet_set_network_values (packet, client, connection);
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

	if (!cerver_connect ("127.0.0.1", 8007)) {
		while (1) {
			// send a test message every second
			// test_msg_send ();
			// test_msg_send ();
			// test_msg_send ();

			sleep (1);
		}

		// for (unsigned int i = 0; i < 5; i++) {
		//     test_msg_send ();
		//     sleep (1);
		// }

		// cerver_disconnect ();
	}

	return 0;

}

#pragma endregion