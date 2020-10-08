#include <stdio.h>

#include <unistd.h>
#include <signal.h>

#include "client/version.h"

#include "client/client.h"
#include "client/packets.h"

#include "client/utils/log.h"

static Client *client = NULL;
static Connection *connection = NULL;

#pragma region connect

static int cerver_connect (const char *ip, unsigned int port) {

	int retval = 1;

	if (ip) {
		fprintf (stdout, "\nConnecting to cerver...\n");

		client = client_create ();
		if (client) {
			connection = client_connection_create (client, ip, port, PROTOCOL_TCP, false);
			if (connection) {
				connection_set_name (connection, "main");
				connection_set_max_sleep (connection, 30);

				if (!client_connect_and_start (client, connection)) {
					client_log_msg (stdout, LOG_TYPE_SUCCESS, LOG_TYPE_NONE, "Connected to cerver!");
					retval = 0;
				}

				else {
					client_log_msg (stderr, LOG_TYPE_ERROR, LOG_TYPE_NONE, "Failed to connect to cerver!");
				}
			}

			else {
				client_log_msg (stderr, LOG_TYPE_ERROR, LOG_TYPE_NONE, "Failed to create connection!");
			}
		}

		else {
			client_log_msg (stderr, LOG_TYPE_ERROR, LOG_TYPE_NONE, "Failed to create client!");
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

#pragma region request

static int request_file (void) {

	int retval = 1;

	if ((client->running) && (connection->connected)) {
		if (!client_file_get (client, connection, "test.txt")) {
			client_log_success ("Requested file to cerver!");
		}

		else {
			client_log_error ("Failed to request file to cerver!");
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

	client_log_debug ("Files Example");
	printf ("\n");

	if (!cerver_connect ("127.0.0.1", 7000)) {
		// add client's files configuration
		client_files_add_path (client, "./data");
		client_files_set_uploads_path (client, "./uploads");

		sleep (2);

		request_file ();

		// wait for file
		sleep (5);

		cerver_disconnect ();
	}

	return 0;

}

#pragma endregion