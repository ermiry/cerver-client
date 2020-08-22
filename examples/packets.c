#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>

#include "client/version.h"

#include "client/types/string.h"

#include "client/client.h"
#include "client/packets.h"

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

static AppData *app_data_new (void) {

	AppData *app_data = (AppData *) malloc (sizeof (AppData));
	if (app_data) {
		memset (app_data, 0, sizeof (AppData));
	}

	return app_data;

}

static void app_data_delete (void *app_data_ptr) {

	if (app_data_ptr) free (app_data_ptr);

}

static AppData *app_data_create (const char *message) {

	AppData *app_data = app_data_new ();
	if (app_data) {
		time (&app_data->timestamp);

		if (message) {
			app_data->message_len = strlen (message);
			strncpy (app_data->message, message, APP_MESSAGE_LEN);
		}
	}

	return app_data;

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
		
		switch (packet->header->request_type) {
			case TEST_MSG: client_log_msg (stdout, LOG_DEBUG, LOG_NO_TYPE, "Got a test message from cerver!"); break;

			default: 
				client_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE, "Got an unknown app request.");
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
			client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to send packet!");
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
		Packet *packet = packet_generate_request (APP_PACKET, TEST_MSG, NULL, 0);
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
			header->packet_type = APP_PACKET;
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

static unsigned int app_msg_send_generate_request (const char *message) {

	unsigned int retval = 1;

	if ((client->running) && (connection->connected)) {
		AppData *app_data = app_data_create (message);
		if (app_data) {
			Packet *packet = packet_generate_request (
				APP_PACKET, APP_MSG, 
				app_data, sizeof (AppData)
			);

			if (packet) {
				retval = send_packet (packet);

				packet_delete (packet);
			}

			app_data_delete (app_data);
		}

		else {
			client_log_error ("app_msg_send_generate_request () - no app message!");
		}
	}

	return retval;

}

static unsigned int app_msg_send_generate_split (const char *message) {

	unsigned int retval = 1;

	if (message) {
		Packet *packet = packet_new ();
		if (packet) {	
			size_t data_size = sizeof (AppData);
			size_t packet_len = sizeof (PacketHeader) + data_size;

			packet->header = packet_header_new ();
			packet->header->packet_type = APP_PACKET;
			packet->header->packet_size = packet_len;

			packet->header->request_type = APP_MSG;

			packet->data = malloc (data_size);
			packet->data_size = data_size;
			
			char *end = packet->data;
			AppData *app_data = (AppData *) end;
			memset (app_data, 0, sizeof (AppData));
			time (&app_data->timestamp);

			if (message) {
				app_data->message_len = strlen (message);
				strncpy (app_data->message, message, APP_MESSAGE_LEN);
			}

			packet_set_network_values (packet, client, connection);
			size_t sent = 0;
			if (packet_send_split (packet, 0, &sent)) {
				client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to send packet!");
			}

			else {
				printf ("Sent to cerver: %ld\n", sent);
				retval = 0;
			}

			packet_delete (packet);
		}
	}

	return retval;

}

static unsigned int app_msg_send_generate_pieces (void) {

	unsigned int retval = 1;

	Packet *packet = packet_new ();
	if (packet) {
		u32 n_messages = 5;
		size_t data_size = sizeof (AppData) * n_messages;
		size_t packet_len = sizeof (PacketHeader) + data_size;

		packet->header = packet_header_new ();
		packet->header->packet_type = APP_PACKET;
		packet->header->packet_size = packet_len;

		packet->header->request_type = MULTI_MSG;

		void **messages = calloc (n_messages, sizeof (AppData));
		size_t *sizes = (size_t *) calloc (n_messages, sizeof (size_t));
		if (messages && sizes) {
			for (u32 i = 0; i < n_messages; i++) {
				messages[i] = malloc (sizeof (AppData));
				sizes[i] = sizeof (AppData);
			}

			AppData *app_data = NULL;

			app_data = (AppData *) messages[0];
			memset (app_data, 0, sizeof (AppData));
			time (&app_data->timestamp);
			app_data->message_len = strlen (MESSAGE_0);
			strncpy (app_data->message, MESSAGE_0, APP_MESSAGE_LEN);

			app_data = (AppData *) messages[1];
			memset (app_data, 0, sizeof (AppData));
			time (&app_data->timestamp);
			app_data->message_len = strlen (MESSAGE_1);
			strncpy (app_data->message, MESSAGE_1, APP_MESSAGE_LEN);

			app_data = (AppData *) messages[2];
			memset (app_data, 0, sizeof (AppData));
			time (&app_data->timestamp);
			app_data->message_len = strlen (MESSAGE_2);
			strncpy (app_data->message, MESSAGE_2, APP_MESSAGE_LEN);

			app_data = (AppData *) messages[3];
			memset (app_data, 0, sizeof (AppData));
			time (&app_data->timestamp);
			app_data->message_len = strlen (MESSAGE_3);
			strncpy (app_data->message, MESSAGE_3, APP_MESSAGE_LEN);

			app_data = (AppData *) messages[4];
			memset (app_data, 0, sizeof (AppData));
			time (&app_data->timestamp);
			app_data->message_len = strlen (MESSAGE_4);
			strncpy (app_data->message, MESSAGE_4, APP_MESSAGE_LEN);

			packet_set_network_values (packet, client, connection);
			size_t sent = 0;
			if (packet_send_pieces (packet, messages, sizes, n_messages, 0, &sent)) {
				client_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to send packet!");
			}

			else {
				printf ("Sent to cerver: %ld / %ld\n", sent, packet_len);
				retval = 0;
			}

			for (u32 i = 0; i < n_messages; i++) free (messages[i]);
			free (messages);

			free (sizes);
		}

		packet_delete (packet);
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

	String *message = str_new ("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");

	if (!cerver_connect ("127.0.0.1", 7000)) {
		sleep (1);

		// while (1) {
			printf ("test_msg_send ()\n");
			test_msg_send ();
			printf ("\n");

			printf ("app_msg_send_generate_manual ()\n");
			app_msg_send_generate_manual (message->str);
			printf ("\n");

			printf ("app_msg_send_generate_request ()\n");
			app_msg_send_generate_request (message->str);
			printf ("\n");

			printf ("app_msg_send_generate_split ()\n");
			app_msg_send_generate_split (message->str);
			printf ("\n");

			printf ("app_msg_send_generate_pieces ()\n");
			app_msg_send_generate_pieces ();
			printf ("\n");

			sleep (1);
		// }

		sleep (1);

		cerver_disconnect ();
	}

	str_delete (message);

	return 0;

}

#pragma endregion