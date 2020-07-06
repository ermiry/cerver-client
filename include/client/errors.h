#ifndef _CLIENT_ERRORS_H_
#define _CLIENT_ERRORS_H_

#include <stdbool.h>

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/packets.h"

struct _Client;
struct _Connection;
struct _Packet;

typedef enum ClientErrorType {

	ERR_NONE                    = 0,

	// internal server error, like no memory
	ERR_CERVER_ERROR            = 1,   

	ERR_CREATE_LOBBY            = 2,
	ERR_JOIN_LOBBY              = 3,
	ERR_LEAVE_LOBBY             = 4,
	ERR_FIND_LOBBY              = 5,

	ERR_GAME_INIT               = 6,
	ERR_GAME_START              = 7,

	ERR_FAILED_AUTH             = 8,

} ClientErrorType;

typedef struct ClientError {

	ClientErrorType error_type;
	bool create_thread;                 // create a detachable thread to run action
    bool drop_after_trigger;            // if we only want to trigger the event once

	Action action;                      // the action to be triggered
    void *action_args;                  // the action arguments
    Action delete_action_args;          // how to get rid of the data

} ClientError;

#pragma region data

// structure that is passed to the user registered method
typedef struct ClientErrorData {

    struct _Client *client;
    struct _Connection *connection;

    void *action_args;                  // the action arguments set by the user

} ClientErrorData;

extern void client_error_data_delete (ClientErrorData *error_data);

#pragma endregion

#pragma region handler

// handles error packets
extern void error_packet_handler (struct _Packet *packet);

#pragma endregion

#pragma region serialization

// serialized error data
typedef struct SError {

	time_t timestamp;
	u32 error_type;
	char msg[64];

} SError;

#pragma endregion

#endif