#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>

#include "client/version.h"

#include "client/types/string.h"

#include "client/client.h"
#include "client/packets.h"
#include "client/timer.h"

#include "client/utils/log.h"

static Client *client = NULL;
static Connection *connection = NULL;

static void app_handler (void *packet_ptr);

#pragma region app

#define APP_MESSAGE_LEN			512

#define MESSAGE_0				"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua."
#define MESSAGE_1				"Sagittis nisl rhoncus mattis rhoncus urna. Vitae congue eu consequat ac felis donec et odio. "
#define MESSAGE_2				"Commodo sed egestas egestas fringilla phasellus. Tellus id interdum velit laoreet id donec ultrices tincidunt. Porttitor massa id neque aliquam vestibulum morbi blandit cursus."
#define MESSAGE_3				"Malesuada pellentesque elit eget gravida cum. Pharetra vel turpis nunc eget lorem dolor sed viverra."
#define MESSAGE_4				"Justo donec enim diam vulputate. Dui nunc mattis enim ut. Quis vel eros donec ac odio tempor. Lorem ipsum dolor sit amet consectetur."

typedef enum AppRequest {

	TEST_MSG		= 0,
	APP_MSG			= 1,
	MULTI_MSG		= 2,

} AppRequest;

typedef struct AppData {

	time_t timestamp;
	size_t message_len;
	char message[APP_MESSAGE_LEN];

} AppData;

static void app_data_print (AppData *app_data) {

	if (app_data) {
		String *date = timer_time_to_string (gmtime (&app_data->timestamp));
		if (date) {
			printf ("Timestamp: %s\n", date->str);
			str_delete (date);
		}

		printf ("Message (%ld): %s\n", app_data->message_len, app_data->message);
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
			client_set_app_handlers (client, app_handler, NULL);

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

static void handle_app_message (Packet *packet) {

	if (packet) {
		char *end = (char *) packet->data;

		AppData *app_data = (AppData *) end;
		app_data_print (app_data);
		printf ("\n");
	}

}

static void app_handler (void *packet_ptr) {

	if (packet_ptr) {
		Packet *packet = (Packet *) packet_ptr;
		
		switch (packet->header->request_type) {
			case TEST_MSG: client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Got a test message from cerver!"); break;

			case APP_MSG: handle_app_message (packet); break;

			default: 
				client_log (LOG_TYPE_WARNING, LOG_TYPE_NONE, "Got an unknown app request.");
				break;
		}
	}

}

#pragma endregion

#pragma region request

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

static unsigned int send_packet (Packet *packet) {

	unsigned int retval = 1;

	if (packet) {
		packet_set_network_values (packet, client, connection);
		size_t sent = 0;
		if (packet_send (packet, 0, &sent, false)) {
			client_log (LOG_TYPE_ERROR, LOG_TYPE_NONE, "Failed to send packet!");
		}

		else {
			printf ("Sent to cerver: %ld\n", sent);
			retval = 0;
		}
	}

	return retval;

}

static unsigned int test_msg_send (void) {

	unsigned int retval = 1;

	if ((client->running) && (connection->connected)) {
		Packet *packet = packet_generate_request (PACKET_TYPE_APP, TEST_MSG, NULL, 0);
		if (packet) {
			retval = send_packet (packet);

			packet_delete (packet);
		}
	}

	return retval;

}

static unsigned int app_msg_send_generate_manual (const char *message) {

	unsigned int retval = 1;

	if (message) {
		Packet *req = packet_new ();
		if (req) {
			size_t packet_len = sizeof (PacketHeader) + sizeof (AppData);
			req->packet = malloc (packet_len);
			req->packet_size = packet_len;

			char *end = (char *) req->packet;
			PacketHeader *header = (PacketHeader *) end;
			header->packet_type = PACKET_TYPE_APP;
			header->packet_size = packet_len;

			header->request_type = APP_MSG;

			end += sizeof (PacketHeader);

			AppData *app_data = (AppData *) end;
			memset (app_data, 0, sizeof (AppData));
			time (&app_data->timestamp);

			if (message) {
				app_data->message_len = strlen (message);
				strncpy (app_data->message, message, APP_MESSAGE_LEN);
			}

			send_packet (req);

			packet_delete (req);
		}
	}

	return retval;

}

#pragma GCC diagnostic pop

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

	client_log_debug ("Packets Example");
	printf ("\n");

	String *messages[5];
	messages[0] = str_new (MESSAGE_0);
	messages[1] = str_new (MESSAGE_1);
	messages[2] = str_new (MESSAGE_2);
	messages[3] = str_new (MESSAGE_3);
	messages[4] = str_new (MESSAGE_4);

	if (!cerver_connect ("127.0.0.1", 7000)) {
		sleep (1);

		int idx = 0;
		while (1) {
			// printf ("test_msg_send ()\n");
			// test_msg_send ();
			// printf ("\n");

			printf ("app_msg_send_generate_manual ()\n");
			app_msg_send_generate_manual (messages[idx]->str);
			printf ("\n");

			sleep (1);

			idx += 1;
			if (idx > 4) idx = 0;
		}

		sleep (1);

		cerver_disconnect ();
	}

	str_delete (messages[0]);
	str_delete (messages[1]);
	str_delete (messages[2]);
	str_delete (messages[3]);
	str_delete (messages[4]);

	return 0;

}

#pragma endregion