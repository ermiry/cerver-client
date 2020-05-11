#include <stdio.h>

#include <unistd.h>
#include <signal.h>

#include "cengine/types/types.h"

#include "cengine/cerver/client.h"
#include "cengine/cerver/packets.h"

#include "cengine/utils/log.h"

#include "version.h"

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

static void app_handler (void *packet_ptr) {

	if (packet_ptr) {
        Packet *packet = (Packet *) packet_ptr;
        if (packet) {
            if (packet->data_size >= sizeof (RequestData)) {
                RequestData *req = (RequestData *) (packet->data);

                switch (req->type) {
                    case TEST_MSG: cengine_log_msg (stdout, LOG_DEBUG, LOG_NO_TYPE, "Got a test message from cerver!"); break;

                    case GET_MSG: {
                        char *end = (char *) packet->data;
                        end += sizeof (RequestData);

                        AppMessage *app_message = (AppMessage *) end;
                        printf ("%s - %d\n", app_message->message, app_message->len);
                    } break;

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
            client_set_app_handlers (client, app_handler, NULL);

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

static u8 max_handler_id = 3;
static u8 handler_id = 0;

static int test_msg_send (void) {

    int retval = 1;

    // manually create a packet to send
    Packet *packet = packet_new ();
    if (packet) {
        size_t packet_len = sizeof (PacketHeader) + sizeof (RequestData);
        packet->packet = malloc (packet_len);
        packet->packet_size = packet_len;

        char *end = (char *) packet->packet;
        PacketHeader *header = (PacketHeader *) end;
        header->protocol_id = packets_get_protocol_id ();
        header->protocol_version = packets_get_protocol_version ();
        header->packet_type = APP_PACKET;
        header->packet_size = packet_len;

        header->handler_id = handler_id;
        handler_id += 1;
        if (handler_id > max_handler_id) handler_id = 0;

        end += sizeof (PacketHeader);
        RequestData *req_data = (RequestData *) end;
        req_data->type = TEST_MSG;

        packet_set_network_values (packet, client, connection);

        size_t sent = 0;
        if (packet_send (packet, 0, &sent, false)) {
            cengine_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to send test to cerver");
        }

        else {
            printf ("Test sent to cerver: %ld\n", sent);
            retval = 0;
        } 
        
        packet_delete (packet);
    }

}

static int request_message (void) {

    int retval = 1;

    // manually create a packet to send
    Packet *packet = packet_new ();
    if (packet) {
        size_t packet_len = sizeof (PacketHeader) + sizeof (RequestData);
        packet->packet = malloc (packet_len);
        packet->packet_size = packet_len;

        char *end = (char *) packet->packet;
        PacketHeader *header = (PacketHeader *) end;
        header->protocol_id = packets_get_protocol_id ();
        header->protocol_version = packets_get_protocol_version ();
        header->packet_type = APP_PACKET;
        header->packet_size = packet_len;

        header->handler_id = handler_id;
        handler_id += 1;
        if (handler_id > max_handler_id) handler_id = 0;

        end += sizeof (PacketHeader);
        RequestData *req_data = (RequestData *) end;
        req_data->type = GET_MSG;

        packet_set_network_values (packet, client, connection);

        size_t sent = 0;
        if (packet_send (packet, 0, &sent, false)) {
            cengine_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to send message request to cerver");
        }

        else {
            printf ("Request sent to cerver: %ld\n", sent);
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
            // send a test message every second
            // test_msg_send ();

            // request unique message from each handler
            request_message ();

            sleep (1);
        }
    }

    return 0;

}