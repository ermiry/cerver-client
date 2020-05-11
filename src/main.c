#include <stdio.h>

#include <unistd.h>
#include <signal.h>

#include "cengine/cerver/client.h"
#include "cengine/cerver/packets.h"

#include "cengine/utils/log.h"

#include "version.h"

typedef enum AppRequest {

	TEST_MSG		= 0

} AppRequest;

static Client *client = NULL;
static Connection *connection = NULL;

static void app_handler (void *packet_ptr) {

	if (packet_ptr) {
        Packet *packet = (Packet *) packet_ptr;
        if (packet) {
            if (packet->data_size >= sizeof (RequestData)) {
                RequestData *req = (RequestData *) (packet->data);

                switch (req->type) {
                    case TEST_MSG: cengine_log_msg (stdout, LOG_DEBUG, LOG_NO_TYPE, "Got a test message from magic cerver!"); break;

                    default: 
                        cengine_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE, "Got an unknown app request.");
                        break;
                }
            }
        }
    }

}

static int cerver_connect (const char *ip, unsigned int port) {

    int retval = 1;

    if (ip) {
        fprintf (stdout, "\nConnecting to cerver...\n");

        client = client_create ();
        if (client) {
            client_set_app_handlers (client, NULL, NULL);

            if (!client_connection_create (client, "main", ip, port, PROTOCOL_TCP, false)) {
                connection = client_connection_get_by_name (client, "main");
                connection_set_max_sleep (connection, 30);

                if (!client_connection_start (client, connection)) {
                    cengine_log_msg (stdout, LOG_SUCCESS, LOG_NO_TYPE, "Connected to cerver!");
                    retval = 0;
                }

                else {
                    cengine_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to connect to cerver!");
                } 
            }

            else {
                cengine_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to create connection!");
            }
        }

        else {
            cengine_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to create client!");
        }
    }

    return retval;

}

static void cerver_disconnect (void) {

    client_connection_end (client, connection);

    // TODO: 26/01/2020 -- 21:31 -- possible seg fault when quitting client with active connection
    client_teardown (client);

}

static int test_msg_send (unsigned int handler_id) {

    int retval = 1;

    Packet *packet = packet_generate_request (APP_PACKET, TEST_MSG, NULL, 0);
    if (packet) {
        packet_set_network_values (packet, client, connection);
        size_t sent = 0;
        if (packet_send (packet, 0, &sent, false)) {
            cengine_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to send test to cerver");
        }

        else {
            // printf ("Test sent to cerver: %ld\n", sent);
            retval = 0;
        } 

        packet_delete (packet);
    }

}

static void end (int dummy) {
	
	cerver_disconnect ();

	exit (0);

}

int main (int argc, const char **argv) {

    // register to the quit signal
	signal (SIGINT, end);

    cerver_client_version_print_full ();

    if (!cerver_connect ("127.0.0.1", 8007)) {
        while (1) {
            sleep (1);
        }
    }

    return 0;

}