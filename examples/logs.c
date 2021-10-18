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
            Handler *app_packet_handler = handler_create (app_handler);
			handler_set_direct_handle (app_packet_handler, true);
			client_set_app_handlers (client, app_packet_handler, NULL);

            connection = client_connection_create (client, ip, port, PROTOCOL_TCP, false);
            if (connection) {
                connection_set_name (connection, "main");
                connection_set_max_sleep (connection, 30);

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
        if (packet) {
            switch (packet->header.request_type) {
                case TEST_MSG: client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Got a test message from cerver!"); break;

                default: 
                    client_log (LOG_TYPE_WARNING, LOG_TYPE_NONE, "Got an unknown app request.");
                    break;
            }
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

    client_log_debug ("Basic test client example with custom logs configurations");
	printf ("\n");

    if (!cerver_connect ("127.0.0.1", 7000)) {
        // send a test message every second
        while (1) {
            test_msg_send ();

            sleep (1);
        }
    }

    client_end ();

    return 0;

}

#pragma endregion