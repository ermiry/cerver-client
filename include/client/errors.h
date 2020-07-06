#ifndef _CLIENT_ERRORS_H_
#define _CLIENT_ERRORS_H_

#include "client/types/string.h"

#include "client/packets.h"

struct _Packet;

typedef enum ErrorType {

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

} ErrorType;

// when a client error happens, it sets the appropaited msg (if any)
// and an event is triggered
typedef struct Error {

    // TODO: maybe add time?
    u32 error_type;
    String *msg;

} Error;

extern Error *error_new (const char *msg);
extern void error_delete (void *ptr);

// handles error packets
extern void error_packet_handler (struct _Packet *packet);

// serialized error data
typedef struct SError {

    time_t timestamp;
    u32 error_type;
    char msg[64];

} SError;

#endif