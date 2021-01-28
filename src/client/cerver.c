#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/cerver.h"
#include "client/connection.h"
#include "client/packets.h"

#include "client/utils/utils.h"
#include "client/utils/log.h"

static Cerver *cerver_deserialize (SCerver *scerver);

#pragma region types

const char *cerver_type_to_string (CerverType type) {

	switch (type) {
		#define XX(num, name, string) case CERVER_TYPE_##name: return #string;
		CERVER_TYPE_MAP(XX)
		#undef XX
	}

	return cerver_type_to_string (CERVER_TYPE_NONE);

}

#pragma endregion

#pragma region stats

static CerverStats *cerver_stats_new (void) {

	CerverStats *cerver_stats = (CerverStats *) malloc (sizeof (CerverStats));
	if (cerver_stats) {
		memset (cerver_stats, 0, sizeof (CerverStats));
		cerver_stats->received_packets = packets_per_type_new ();
		cerver_stats->sent_packets = packets_per_type_new ();
	}

	return cerver_stats;

}

static void cerver_stats_delete (CerverStats *cerver_stats) {

	if (cerver_stats) {
		packets_per_type_delete (cerver_stats->received_packets);
		packets_per_type_delete (cerver_stats->sent_packets);

		free (cerver_stats);
	}

}

void cerver_stats_print (Cerver *cerver) {

	if (cerver) {
		if (cerver->stats) {
			client_log_msg ("\nCerver's %s stats: \n", cerver->name->str);
			client_log_msg ("Threshold time:                %ld\n", cerver->stats->threshold_time);

			if (cerver->auth_required) {
				client_log_msg ("Client packets received:       %ld\n", cerver->stats->client_n_packets_received);
				client_log_msg ("Client receives done:          %ld\n", cerver->stats->client_receives_done);
				client_log_msg ("Client bytes received:         %ld\n\n", cerver->stats->client_bytes_received);

				client_log_msg ("On hold packets received:       %ld\n", cerver->stats->on_hold_n_packets_received);
				client_log_msg ("On hold receives done:          %ld\n", cerver->stats->on_hold_receives_done);
				client_log_msg ("On hold bytes received:         %ld\n\n", cerver->stats->on_hold_bytes_received);
			}

			client_log_msg ("\n");
			client_log_msg ("Total packets received:        %ld\n", cerver->stats->total_n_packets_received);
			client_log_msg ("Total receives done:           %ld\n", cerver->stats->total_n_receives_done);
			client_log_msg ("Total bytes received:          %ld\n\n", cerver->stats->total_bytes_received);

			client_log_msg ("\n");
			client_log_msg ("N packets sent:                %ld\n", cerver->stats->n_packets_sent);
			client_log_msg ("Total bytes sent:              %ld\n", cerver->stats->total_bytes_sent);

			client_log_msg ("\n");
			client_log_msg ("Current active client connections:         %ld\n", cerver->stats->current_active_client_connections);
			client_log_msg ("Current connected clients:                 %ld\n", cerver->stats->current_n_connected_clients);
			client_log_msg ("Current on hold connections:               %ld\n", cerver->stats->current_n_hold_connections);
			client_log_msg ("Total on hold connections:                 %ld\n", cerver->stats->total_on_hold_connections);
			client_log_msg ("Total clients:                             %ld\n", cerver->stats->total_n_clients);
			client_log_msg ("Unique clients:                            %ld\n", cerver->stats->unique_clients);
			client_log_msg ("Total client connections:                  %ld\n", cerver->stats->total_client_connections);

			client_log_msg ("\nReceived packets:\n");
			packets_per_type_print (cerver->stats->received_packets);

			client_log_msg ("\nSent packets:\n");
			packets_per_type_print (cerver->stats->sent_packets);
		}

		else {
			client_log (
				LOG_TYPE_ERROR, LOG_TYPE_CERVER,
				"Cerver %s does not have a reference to cerver stats!",
				cerver->name->str
			);
		}
	}

	else {
		client_log (
			LOG_TYPE_WARNING, LOG_TYPE_CERVER,
			"Cant print stats of a NULL cerver!"
		);
	}

}

#pragma endregion

#pragma region cerver

Cerver *cerver_new (void) {

	Cerver *cerver = (Cerver *) malloc (sizeof (Cerver));
	if (cerver) {
		memset (cerver, 0, sizeof (Cerver));

		cerver->name = NULL;
		cerver->welcome = NULL;

		cerver->stats = NULL;
	}

	return cerver;

}

void cerver_delete (void *ptr) {

	if (ptr) {
		Cerver *cerver = (Cerver *) ptr;

		str_delete (cerver->name);
		str_delete (cerver->welcome);

		cerver_stats_delete (cerver->stats);

		free (cerver);
	}

}

static void cerver_check_info_handle_auth (Cerver *cerver, Client *client, Connection *connection) {

	if (cerver && connection) {
		if (cerver->auth_required) {
			// #ifdef CLIENT_DEBUG
			client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Cerver requires authentication.");
			// #endif
			if (connection->auth_data) {
				#ifdef CLIENT_DEBUG
				client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Sending auth data to cerver...");
				#endif

				if (!connection->auth_packet) {
					if (!connection_generate_auth_packet (connection)) {
						client_log_success (
							"cerver_check_info () - Generated connection %s auth packet!",
							connection->name->str
						);
					}

					else {
						client_log_error (
							"cerver_check_info () - Failed to generate connection %s auth packet!",
							connection->name->str
						);
					}
				}

				if (connection->auth_packet) {
					packet_set_network_values (connection->auth_packet, NULL, connection);

					if (!packet_send (connection->auth_packet, 0, NULL, false)) {
						client_log_success (
							"cerver_check_info () - Sent connection %s auth packet!",
							connection->name->str
						);

						client_event_trigger (CLIENT_EVENT_AUTH_SENT, client, connection);
					}

					else {
						client_log_error (
							"cerver_check_info () - Failed to send connection %s auth packet!",
							connection->name->str
						);
					}
				}

				if (cerver->uses_sessions) {
					client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Cerver supports sessions.");
				}
			}

			else {
				client_log_error (
					"Connection %s does NOT have an auth packet!",
					connection->name->str
				);
			}
		}

		else {
			#ifdef CLIENT_DEBUG
			client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Cerver does NOT require authentication.");
			#endif
		}
	}

}

// compare the info the server sent us with the one we expected
// and ajust our connection values if necessary
static u8 cerver_check_info (Cerver *cerver, Client *client, Connection *connection) {

	u8 retval = 1;

	if (cerver && connection) {
		connection->cerver = cerver;

		#ifdef CLIENT_DEBUG
		client_log (
			LOG_TYPE_DEBUG, LOG_TYPE_NONE,
			"Connected to cerver %s.", cerver->name->str
		);

		if (cerver->welcome) {
			printf ("%s\n", cerver->welcome->str);
		}

		switch (cerver->protocol) {
			case PROTOCOL_TCP:
				client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Cerver using TCP protocol.");
				break;
			case PROTOCOL_UDP:
				client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Cerver using UDP protocol.");
				break;

			default:
				client_log (LOG_TYPE_WARNING, LOG_TYPE_NONE, "Cerver using unknown protocol.");
				break;
		}
		#endif

		if (cerver->use_ipv6) {
			#ifdef CLIENT_DEBUG
			client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Cerver is configured to use ipv6");
			#endif
		}

		#ifdef CLIENT_DEBUG
		switch (cerver->type) {
			case CERVER_TYPE_CUSTOM:
				client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Cerver type: CUSTOM");
				break;

			case CERVER_TYPE_GAME:
				client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Cerver type: GAME");
				break;
			case CERVER_TYPE_WEB:
				client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Cerver type: WEB");
				break;
			case CERVER_TYPE_FILES:
				client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Cerver type: FILES");
				break;

			case CERVER_TYPE_BALANCER:
				client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Cerver type: BALANCER");
				break;

			default:
				client_log (LOG_TYPE_ERROR, LOG_TYPE_NONE, "Cerver type: UNKNOWN");
				break;
		}
		#endif

		cerver_check_info_handle_auth (cerver, client, connection);

		retval = 0;
	}

	return retval;

}

#pragma endregion

#pragma region handler

static void cerver_packet_handle_info (Packet *packet) {

	if (packet->data && (packet->data_size > 0)) {
		char *end = (char *) packet->data;

		#ifdef CLIENT_DEBUG
		client_log (LOG_TYPE_DEBUG, LOG_TYPE_NONE, "Received a cerver info packet.");
		#endif
		Cerver *cerver = cerver_deserialize ((SCerver *) end);
		if (cerver_check_info (cerver, packet->client, packet->connection))
			client_log (LOG_TYPE_ERROR, LOG_TYPE_NONE, "Failed to correctly check cerver info!");
	}

}

// handles cerver type packets
void cerver_packet_handler (Packet *packet) {

	if (packet->header) {
		switch (packet->header->request_type) {
			case CERVER_PACKET_TYPE_INFO:
				cerver_packet_handle_info (packet);
				break;

			// the cerves is going to be teardown, we have to disconnect
			case CERVER_PACKET_TYPE_TEARDOWN:
				#ifdef CLIENT_DEBUG
				client_log (LOG_TYPE_WARNING, LOG_TYPE_NONE, "---> Server teardown! <---");
				#endif
				client_got_disconnected (packet->client);
				client_event_trigger (CLIENT_EVENT_DISCONNECTED, packet->client, NULL);
				break;

			default:
				client_log (LOG_TYPE_WARNING, LOG_TYPE_NONE, "Unknown cerver type packet.");
				break;
		}
	}

}

#pragma endregion

#pragma region serialization

static Cerver *cerver_deserialize (SCerver *scerver) {

	Cerver *cerver = NULL;

	if (scerver) {
		cerver = cerver_new ();
		if (cerver) {
			cerver->type = scerver->type;

			cerver->name = str_new (scerver->name);
			if (strlen (scerver->welcome)) cerver->welcome = str_new (scerver->welcome);

			cerver->use_ipv6 = scerver->use_ipv6;
			cerver->protocol = scerver->protocol;
			cerver->port = scerver->port;

			cerver->auth_required = scerver->auth_required;
			cerver->uses_sessions = scerver->uses_sessions;

			cerver->stats = cerver_stats_new ();
		}
	}

	return cerver;

}

#pragma endregion