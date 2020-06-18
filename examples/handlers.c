#include <stdio.h>

#include <unistd.h>
#include <signal.h>

#include "client/version.h"

#include "client/types/types.h"

#include "client/cerver/client.h"
#include "client/cerver/packets.h"

#include "client/utils/log.h"

typedef enum AppRequest {

	TEST_MSG		= 0,

	GET_MSG			= 1

} AppRequest;

static Client *client = NULL;
static Connection *connection = NULL;

static void my_app_handler (void *data);
static void my_app_error_handler (void *data);
static void my_custom_handler (void *data);

#pragma region connect

static int cerver_connect (const char *ip, unsigned int port) {

    int retval = 1;

    if (ip) {
        fprintf (stdout, "\nConnecting to cerver...\n");

        client = client_create ();
        if (client) {
            client_set_app_handlers (client, my_app_handler, my_app_error_handler);
            client_set_custom_handler (client, my_custom_handler);

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

    client_teardown (client);

}

#pragma endregion

#pragma region handler

static void my_app_handler (void *data) {

	if (data) {
		Packet *packet = (Packet *) data;
		if (packet->data_size >= sizeof (RequestData)) {
			RequestData *req = (RequestData *) (packet->data);

			switch (req->type) {
				case TEST_MSG: 
					client_log_debug ("Got a APP_PACKET test!");
					break;

				default: 
					client_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE, "Got an unknown APP_PACKET request.");
					break;
			}
		}
	}

}

static void my_app_error_handler (void *data) {

	if (data) {
		Packet *packet = (Packet *) data;
		if (packet->data_size >= sizeof (RequestData)) {
			RequestData *req = (RequestData *) (packet->data);

			switch (req->type) {
				case TEST_MSG: 
					client_log_debug ("Got a APP_ERROR_PACKET test!");
					break;

				default: 
					client_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE, "Got an unknown APP_ERROR_PACKET request.");
					break;
			}
		}
	}

}

static void my_custom_handler (void *data) {

	if (data) {
		Packet *packet = (Packet *) data;
		if (packet->data_size >= sizeof (RequestData)) {
			RequestData *req = (RequestData *) (packet->data);

			switch (req->type) {
				case TEST_MSG: 
					client_log_debug ("Got a CUSTOM_PACKET test!");
					break;

				default: 
					client_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE, "Got an unknown CUSTOM_PACKET request.");
					break;
			}
		}
	}

}

#pragma endregion

#pragma region request

static int test_app_msg_send (void) {

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
                printf ("APP_PACKET sent to cerver: %ld\n", sent);
                retval = 0;
            } 

            packet_delete (packet);
        }
    }

    return retval;

}

static int test_app_error_msg_send (void) {

    int retval = 1;

    if ((client->running) && (connection->connected)) {
        Packet *packet = packet_generate_request (APP_ERROR_PACKET, TEST_MSG, NULL, 0);
        if (packet) {
            packet_set_network_values (packet, client, connection);
            size_t sent = 0;
            if (packet_send (packet, 0, &sent, false)) {
                client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to send test to cerver");
            }

            else {
                printf ("APP_ERROR_PACKET sent to cerver: %ld\n", sent);
                retval = 0;
            } 

            packet_delete (packet);
        }
    }

    return retval;

}

static int test_custom_msg_send (void) {

    int retval = 1;

    if ((client->running) && (connection->connected)) {
        Packet *packet = packet_generate_request (CUSTOM_PACKET, TEST_MSG, NULL, 0);
        if (packet) {
            packet_set_network_values (packet, client, connection);
            size_t sent = 0;
            if (packet_send (packet, 0, &sent, false)) {
                client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to send test to cerver");
            }

            else {
                printf ("CUSTOM_PACKET sent to cerver: %ld\n", sent);
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

    client_log_debug ("Multiple Handlers Example");
	printf ("\n");

    if (!cerver_connect ("127.0.0.1", 8007)) {
        while (1) {
            test_app_msg_send ();

            test_app_error_msg_send ();

            test_custom_msg_send ();

            sleep (1);
        }
    }

    return 0;

}

#pragma endregion