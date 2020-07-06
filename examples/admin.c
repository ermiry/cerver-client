#include <stdio.h>

#include <unistd.h>
#include <signal.h>

#include "client/version.h"

#include "client/client.h"
#include "client/packets.h"

#include "client/utils/log.h"

typedef enum AppRequest {

	TEST_MSG		= 0

} AppRequest;

static Client *client = NULL;
static Connection *connection = NULL;

static void app_handler (void *packet_ptr);

#pragma region connect

static int cerver_connect (const char *ip, unsigned int port) {

	int retval = 1;

	if (ip) {
		fprintf (stdout, "\nConnecting to cerver...\n");

		client = client_create ();
		if (client) {
			client_set_app_handlers (client, app_handler, NULL);

			connection = client_connection_create (client, ip, port, PROTOCOL_TCP, false);
			if (connection) {
				connection_set_name (connection, "main");
				connection_set_max_sleep (connection, 30);

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

	client_log_debug ("Basic Admin Example");
	printf ("\n");

	if (!cerver_connect ("127.0.0.1", 8007)) {
		while (1) {
			// send a test message every second
			test_msg_send ();
			test_msg_send ();
			test_msg_send ();

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