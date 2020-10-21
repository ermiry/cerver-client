#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>

#include "client/version.h"

#include "client/client.h"
#include "client/packets.h"

#include "client/utils/utils.h"
#include "client/utils/log.h"

typedef enum AppRequest {

	TEST_MSG		= 0

} AppRequest;

static Client *client = NULL;
static Connection *connection = NULL;

#pragma region events

static void client_error_file_not_found (void *client_error_data_ptr) {

	if (client_error_data_ptr) {
		ClientErrorData *client_error_data = (ClientErrorData *) client_error_data_ptr;

		client_log_warning ("client_error_file_not_found () - request file was not found!");

		client_error_data_delete (client_error_data);
	}

}

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

#pragma endregion

#pragma region connect

static int cerver_connect (const char *ip, unsigned int port) {

	int retval = 1;

	if (ip) {
		fprintf (stdout, "\nConnecting to cerver...\n");

		client = client_create ();
		if (client) {
			client_event_register (
				client, 
				CLIENT_EVENT_CONNECTION_CLOSE, 
				client_event_connection_close, NULL, NULL, 
				false, false
			);

			client_error_register (
				client,
				CLIENT_ERROR_FILE_NOT_FOUND,
				client_error_file_not_found, NULL, NULL,
				false, false
			);

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

static int test_msg_send (void) {

    int retval = 1;

    if ((client->running) && (connection->connected)) {
        Packet *packet = packet_generate_request (PACKET_TYPE_APP, TEST_MSG, NULL, 0);
        if (packet) {
            packet_set_network_values (packet, client, connection);
            size_t sent = 0;
            if (packet_send (packet, 0, &sent, false)) {
                client_log_msg (stderr, LOG_TYPE_ERROR, LOG_TYPE_NONE, "Failed to send test to cerver");
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

static void request_file (const char *filename) {

	if (!client_file_get (client, connection, filename)) {
		client_log_success ("REQUESTED file to cerver!");
	}

	else {
		client_log_error ("Failed to REQUEST file to cerver!");
	}

}

static void send_file (const char *filename) {

	if (!client_file_send (client, connection, filename)) {
		client_log_success ("SENT file to cerver!");
	}

	else {
		client_log_error ("Failed to SEND file to cerver!");
	}

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

static void start (const char *action, const char *filename) {

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

		test_msg_send ();
		test_msg_send ();

		if (!strcmp ("get", action)) request_file (filename);
		else if (!strcmp ("send", action)) send_file (filename);
		else {
			char *status = c_string_create ("Unknown action %s", action);
			if (status) {
				printf ("\n");
				client_log_error (status);
				printf ("\n");
				free (status);
			}
		}

		test_msg_send ();
		test_msg_send ();

		sleep (5);

		cerver_disconnect ();
	}

}

int main (int argc, const char **argv) {

	if (argc >= 3) {
		start (argv[1], argv[2]);
	}

	else {
		printf ("\nUsage: %s get/send [filename]\n\n", argv[0]);
	}

	return 0;

}

#pragma endregion