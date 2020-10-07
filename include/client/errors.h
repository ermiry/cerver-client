#ifndef _CLIENT_ERRORS_H_
#define _CLIENT_ERRORS_H_

#include <stdbool.h>

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/config.h"
#include "client/packets.h"

struct _Client;
struct _Connection;
struct _Packet;

#pragma region types

#define CLIENT_MAX_ERRORS				32

#define CLIENT_ERROR_MAP(XX)													\
	XX(0,	NONE, 				No error)										\
	XX(1,	CERVER_ERROR, 		The cerver had an internal error)				\
	XX(2,	PACKET_ERROR, 		The cerver was unable to handle the packet)		\
	XX(3,	FAILED_AUTH, 		Client failed to authenticate)					\
	XX(4,	GET_FILE, 			Bad get file request)							\
	XX(5,	SEND_FILE, 			Bad upload file request)						\
	XX(6,	CREATE_LOBBY, 		Failed to create a new game lobby)				\
	XX(7,	JOIN_LOBBY, 		The player failed to join an existing lobby)	\
	XX(8,	LEAVE_LOBBY, 		The player failed to exit the lobby)			\
	XX(9,	FIND_LOBBY, 		Failed to find a suitable game lobby)			\
	XX(10,	GAME_INIT, 			The game failed to init)						\
	XX(11,	GAME_START, 		The game failed to start)						\
	XX(12,	UNKNOWN, 			Unknown error)

typedef enum ClientErrorType {

	#define XX(num, name, description) CLIENT_ERROR_##name = num,
	CLIENT_ERROR_MAP (XX)
	#undef XX

} ClientErrorType;

// get the description for the current error type
CLIENT_EXPORT const char *client_error_type_description (ClientErrorType type);

#pragma endregion

#pragma region errors

typedef struct ClientError {

	ClientErrorType type;
	bool create_thread;                 // create a detachable thread to run action
	bool drop_after_trigger;            // if we only want to trigger the event once

	Action action;                      // the action to be triggered
	void *action_args;                  // the action arguments
	Action delete_action_args;          // how to get rid of the data

} ClientError;

CLIENT_PRIVATE void client_error_delete (void *client_error_ptr);

// registers an action to be triggered when the specified error occurs
// if there is an existing action registered to an error, it will be overrided
// a newly allocated ClientErrorData structure will be passed to your method 
// that should be free using the client_error_data_delete () method
// returns 0 on success, 1 on error
CLIENT_EXPORT u8 client_error_register (
	struct _Client *client,
	const ClientErrorType error_type,
	Action action, void *action_args, Action delete_action_args, 
	bool create_thread, bool drop_after_trigger
);

// unregisters the action associated with the error types
// deletes the action args using the delete_action_args () if NOT NULL
// returns 0 on success, 1 on error or if error is NOT registered
CLIENT_EXPORT u8 client_error_unregister (struct _Client *client, const ClientErrorType error_type);

// triggers all the actions that are registred to an error
// returns 0 on success, 1 on error
CLIENT_PRIVATE u8 client_error_trigger (
	const ClientErrorType error_type, 
	const struct _Client *client, const struct _Connection *connection, 
	const char *error_message
);

#pragma endregion

#pragma region data

// structure that is passed to the user registered method
typedef struct ClientErrorData {

	const struct _Client *client;
	const struct _Connection *connection;

	void *action_args;                  // the action arguments set by the user

	String *error_message;

} ClientErrorData;

CLIENT_PUBLIC void client_error_data_delete (ClientErrorData *error_data);

#pragma endregion

#pragma region handler

// handles error packets
CLIENT_PRIVATE void error_packet_handler (struct _Packet *packet);

#pragma endregion

#pragma region packets

// creates an error packet ready to be sent
CLIENT_PUBLIC struct _Packet *error_packet_generate (const ClientErrorType type, const char *msg);

// creates and send a new error packet
// returns 0 on success, 1 on error
CLIENT_PUBLIC u8 error_packet_generate_and_send (
	const ClientErrorType type, const char *msg,
	struct _Client *client, struct _Connection *connection
);

#pragma endregion

#pragma region serialization

#define ERROR_MESSAGE_LENGTH        128

// serialized error data
typedef struct SError {

	time_t timestamp;
	u32 error_type;
	char msg[ERROR_MESSAGE_LENGTH];

} SError;

#pragma endregion

#endif