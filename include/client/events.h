#ifndef _CLIENT_EVENTS_H_
#define _CLIENT_EVENTS_H_

#include <stdbool.h>

#include "client/types/types.h"

#include "client/config.h"

#define CLIENT_MAX_EVENTS				32

#ifdef __cplusplus
extern "C" {
#endif

struct _Client;
struct _Connection;

#define CLIENT_EVENT_MAP(XX)																													\
	XX(0,	NONE, 				No event)																										\
	XX(1,	CONNECTED, 			Connected to cerver)																							\
	XX(2,	DISCONNECTED, 		Disconnected from the cerver; either by the cerver or by losing connection)										\
	XX(3,	CONNECTION_FAILED, 	Failed to connect to cerver)																					\
	XX(4,	CONNECTION_CLOSE, 	The connection was clossed directly by client. This happens when a call to a recv () methods returns <= 0)		\
	XX(5,	CONNECTION_DATA, 	Data has been received; only triggered from client request methods)												\
	XX(6,	CLIENT_INFO, 		Received cerver info from the cerver)																			\
	XX(7,	CLIENT_TEARDOWN, 	The cerver is going to teardown & the client will disconnect)													\
	XX(8,	CLIENT_STATS, 		Received cerver stats)																							\
	XX(9,	CLIENT_GAME_STATS, 	Received cerver game stats)																						\
	XX(10,	AUTH_SENT, 			Auth data has been sent to the cerver)																			\
	XX(11,	SUCCESS_AUTH, 		Auth with cerver has been successfull)																			\
	XX(12,	MAX_AUTH_TRIES, 	Maxed out attempts to authenticate to cerver; need to try again)												\
	XX(13,	LOBBY_CREATE, 		A new lobby was successfully created)																			\
	XX(14,	LOBBY_JOIN, 		Correctly joined a new lobby)																					\
	XX(15,	LOBBY_LEAVE, 		Successfully exited a lobby)																					\
	XX(16,	LOBBY_START, 		The game in the lobby has started)																				\
	XX(17,	UNKNOWN, 			Unknown event)

typedef enum ClientEventType {

	#define XX(num, name, description) CLIENT_EVENT_##name = num,
	CLIENT_EVENT_MAP (XX)
	#undef XX

} ClientEventType;

// get the description for the current error type
CLIENT_EXPORT const char *client_event_type_description (
	const ClientEventType type
);

struct _ClientEvent {

	ClientEventType type;         // the event we are waiting to happen
	bool create_thread;                 // create a detachable thread to run action
	bool drop_after_trigger;            // if we only want to trigger the event once

	// the request that triggered the event
	// this is usefull for custom events
	u32 request_type;
	void *response_data;                // data that came with the response
	Action delete_response_data;

	Work work;                      	// the action to be triggered
	void *work_args;                  // the action arguments
	Action delete_action_args;          // how to get rid of the data

};

typedef struct _ClientEvent ClientEvent;

CLIENT_PRIVATE void client_event_delete (void *ptr);

// registers an action to be triggered when the specified event occurs
// if there is an existing action registered to an event, it will be overrided
// a newly allocated ClientEventData structure will be passed to your method
// that should be free using the client_event_data_delete () method
// returns 0 on success, 1 on error
CLIENT_EXPORT u8 client_event_register (
	struct _Client *client,
	const ClientEventType event_type,
	Work work, void *work_args, Action delete_action_args,
	bool create_thread, bool drop_after_trigger
);

// unregister the action associated with an event
// deletes the action args using the delete_action_args () if NOT NULL
// returns 0 on success, 1 on error or if event is NOT registered
CLIENT_EXPORT u8 client_event_unregister (
	struct _Client *client, const ClientEventType event_type
);

CLIENT_PRIVATE void client_event_set_response (
	struct _Client *client,
	const ClientEventType event_type,
	void *response_data, Action delete_response_data
);

// triggers all the actions that are registred to an event
CLIENT_PRIVATE void client_event_trigger (
	const ClientEventType event_type,
	const struct _Client *client, const struct _Connection *connection
);

// structure that is passed to the user registered method
typedef struct ClientEventData {

	const struct _Client *client;
	const struct _Connection *connection;

	void *response_data;                // data that came with the response
	Action delete_response_data;

	void *action_args;                  // the action arguments
	Action delete_action_args;

} ClientEventData;

CLIENT_PUBLIC void client_event_data_delete (
	ClientEventData *event_data
);

#ifdef __cplusplus
}
#endif

#endif