#include <stdio.h>

#include <unistd.h>
#include <signal.h>

#include "client/version.h"

#include "client/types/types.h"

#include "client/client.h"
#include "client/packets.h"

#include "client/utils/log.h"

typedef enum AppRequest {

	TEST_MSG		= 0,

	GET_MSG			= 1

} AppRequest;

// message from the cerver
typedef struct AppMessage {

	unsigned int len;
	char message[128];

} AppMessage;

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
                
                if (!client_connect_to_cerver (client, connection)) {
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

    // TODO: 26/01/2020 -- 21:31 -- possible seg fault when quitting client with active connection
    client_teardown (client);

}

#pragma endregion

#pragma region handler

static void app_handler (void *packet_ptr) {

	if (packet_ptr) {
        Packet *packet = (Packet *) packet_ptr;
        
        switch (packet->header->request_type) {
            case TEST_MSG: client_log_msg (stdout, LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Got a test message from cerver!"); break;

            case GET_MSG: {
                char *end = (char *) packet->data;

                AppMessage *app_message = (AppMessage *) end;
                printf ("%s - %d\n", app_message->message, app_message->len);
            } break;

            default: 
                client_log_msg (stderr, LOG_TYPE_WARNING, LOG_TYPE_NONE, "Got an unknown app request.");
                break;
        }
    }

}

#pragma endregion

#pragma region request

static int request_message (void) {

    int retval = 1;

    // manually create a packet to send
    Packet *packet = packet_new ();
    if (packet) {
        size_t packet_len = sizeof (PacketHeader);
        packet->packet = malloc (packet_len);
        packet->packet_size = packet_len;

        char *end = (char *) packet->packet;
        PacketHeader *header = (PacketHeader *) end;
        header->packet_type = APP_PACKET;
        header->packet_size = packet_len;
        header->handler_id = 0;

        header->request_type = GET_MSG;

        printf ("Requesting to cerver...\n");
        if (client_request_to_cerver (client, connection, packet)) {
            client_log_msg (stderr, LOG_TYPE_ERROR, LOG_TYPE_NONE, "Failed to send message request to cerver");
        }
        printf ("Request has ended\n");

        packet_delete (packet);
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

    client_log_debug ("Request Example");
	printf ("\n");

    if (!cerver_connect ("127.0.0.1", 7000)) {
        while (1) {
            request_message ();

            sleep (1);
        }
    }

    return 0;

}

#pragma endregion