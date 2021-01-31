#include <stdlib.h>
#include <string.h>

#include "client/packets.h"
#include "client/receive.h"

const char *receive_error_to_string (
	const ReceiveError error
) {

	switch (error) {
		#define XX(num, name, string) case RECEIVE_ERROR_##name: return #string;
		RECEIVE_ERROR_MAP(XX)
		#undef XX
	}

	return receive_error_to_string (RECEIVE_ERROR_NONE);

}

const char *receive_type_to_string (
	const ReceiveType type
) {

	switch (type) {
		#define XX(num, name, string) case RECEIVE_TYPE_##name: return #string;
		RECEIVE_TYPE_MAP(XX)
		#undef XX
	}

	return receive_type_to_string (RECEIVE_TYPE_NONE);

}

const char *receive_handle_state_to_string (
	const ReceiveHandleState state
) {

	switch (state) {
		#define XX(num, name, string) case RECEIVE_HANDLE_STATE_##name: return #string;
		RECEIVE_HANDLE_STATE_MAP(XX)
		#undef XX
	}

	return receive_handle_state_to_string (RECEIVE_HANDLE_STATE_NONE);

}

ReceiveHandle *receive_handle_new (void) {

	ReceiveHandle *receive_handle =
		(ReceiveHandle *) malloc (sizeof (ReceiveHandle));

	if (receive_handle) {
		receive_handle->type = RECEIVE_TYPE_NONE;

		receive_handle->socket = NULL;
		receive_handle->connection = NULL;
		receive_handle->client = NULL;

		receive_handle->buffer = NULL;
		receive_handle->buffer_size = 0;

		receive_handle->state = RECEIVE_HANDLE_STATE_NONE;

		(void) memset (&receive_handle->header, 0, sizeof (PacketHeader));
		receive_handle->header_end = NULL;
		receive_handle->remaining_header = 0;

		receive_handle->spare_packet = NULL;
	}

	return receive_handle;

}

void receive_handle_delete (void *receive_ptr) {
	
	if (receive_ptr) free (receive_ptr);
	
}