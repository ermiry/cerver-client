#ifndef _CLIENT_CERVER_H_
#define _CLIENT_CERVER_H_

#include <stdbool.h>

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/config.h"
#include "client/network.h"
#include "client/packets.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _Cerver;
struct _Packet;
struct _Token;

struct _Client;
struct _Connection;

#pragma region types

#define CERVER_TYPE_MAP(XX)					\
	XX(0,	NONE, 		None)				\
	XX(1,	CUSTOM, 	Custom)				\
	XX(2,	GAME, 		Game)				\
	XX(3,	WEB, 		Web)				\
	XX(4,	FILES, 		Files)				\
	XX(5,	BALANCER, 	Balancer)

typedef enum CerverType {

	#define XX(num, name, string) CERVER_TYPE_##name = num,
	CERVER_TYPE_MAP (XX)
	#undef XX

} CerverType;

CLIENT_EXPORT const char *cerver_type_to_string (CerverType type);

#pragma endregion

#pragma region stats

struct _CerverStats {

	time_t threshold_time;                          // every time we want to reset cerver stats (like packets), defaults 24hrs

	u64 client_n_packets_received;                  // packets received from clients
	u64 client_receives_done;                       // receives done to clients
	u64 client_bytes_received;                      // bytes received from clients

	u64 on_hold_n_packets_received;                 // packets received from on hold connections
	u64 on_hold_receives_done;                      // received done to on hold connections
	u64 on_hold_bytes_received;                     // bytes received from on hold connections

	u64 total_n_packets_received;                   // total number of cerver packets received (packet header + data)
	u64 total_n_receives_done;                      // total amount of actual calls to recv ()
	u64 total_bytes_received;                       // total amount of bytes received in the cerver

	u64 n_packets_sent;                             // total number of packets that were sent
	u64 total_bytes_sent;                           // total amount of bytes sent by the cerver

	u64 current_active_client_connections;          // all of the current active connections for all current clients (active in main poll array)
	u64 current_n_connected_clients;                // the current number of clients connected
	u64 current_n_hold_connections;                 // current numbers of on hold connections (only if the cerver requires authentication)
	u64 total_on_hold_connections;                  // the total amount of on hold connections
	u64 total_n_clients;                            // the total amount of clients that were registered to the cerver (no auth required)
	u64 unique_clients;                             // n unique clients connected in a threshold time (check used authentication)
	u64 total_client_connections;                   // the total amount of client connections that have been done to the cerver

	struct _PacketsPerType *received_packets;
	struct _PacketsPerType *sent_packets;

};

typedef struct _CerverStats CerverStats;

#pragma endregion

CLIENT_PUBLIC void cerver_stats_print (struct _Cerver *cerver);

struct _Cerver {

	CerverType type;

	String *name;
	String *welcome;

	bool use_ipv6;
	Protocol protocol;
	u16 port;

	bool auth_required;
	bool uses_sessions;

	CerverStats *stats;

};

typedef struct _Cerver Cerver;

CLIENT_PUBLIC Cerver *cerver_new (void);

CLIENT_PUBLIC void cerver_delete (void *ptr);

#pragma region handler

// compare the info the server sent us with the one we expected
// and ajust our connection values if necessary
CLIENT_PRIVATE u8 cerver_check_info (
	Cerver *cerver, struct _Client *client, struct _Connection *connection
);

#pragma endregion

#pragma region serialization

#define S_CERVER_NAME_LENGTH                128
#define S_CERVER_WELCOME_LENGTH             512

// serialized cerver structure
typedef struct SCerver {

	CerverType type;

	char name[S_CERVER_NAME_LENGTH];
	char welcome[S_CERVER_WELCOME_LENGTH];

	bool use_ipv6;
	Protocol protocol;
	u16 port;

	bool auth_required;
	bool uses_sessions;

} SCerver;

CLIENT_PRIVATE Cerver *cerver_deserialize (SCerver *scerver);

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif