#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "client/types/types.h"

#include "client/collections/dlist.h"

#include "client/client.h"
#include "client/connection.h"
#include "client/events.h"

#include "client/threads/thread.h"

u8 client_event_unregister (Client *client, ClientEventType event_type);

#pragma region types

// get the description for the current event type
const char *client_event_type_description (ClientEventType type) {

	switch (type) {
		#define XX(num, name, description) case CLIENT_EVENT_##name: return #description;
		CLIENT_EVENT_MAP(XX)
		#undef XX
	}

	return client_event_type_description (CLIENT_EVENT_UNKNOWN);

}


#pragma endregion

#pragma region data

static ClientEventData *client_event_data_new (void) {

	ClientEventData *event_data = (ClientEventData *) malloc (sizeof (ClientEventData));
	if (event_data) {
		event_data->client = NULL;
		event_data->connection = NULL;

		event_data->response_data = NULL;
		event_data->delete_response_data = NULL;

		event_data->action_args = NULL;
		event_data->delete_action_args = NULL;
	}

	return event_data;

}

void client_event_data_delete (ClientEventData *event_data) {

	if (event_data) free (event_data);

}

static ClientEventData *client_event_data_create (
	const Client *client, const Connection *connection,
	ClientEvent *event
) {

	ClientEventData *event_data = client_event_data_new ();
	if (event_data) {
		event_data->client = client;
		event_data->connection = connection;

		event_data->response_data = event->response_data;
		event_data->delete_response_data = event->delete_response_data;

		event_data->action_args = event->action_args;
		event_data->delete_action_args = event->delete_action_args;
	}

	return event_data;

}

#pragma endregion

#pragma region events

static ClientEvent *client_event_new (void) {

	ClientEvent *event = (ClientEvent *) malloc (sizeof (ClientEvent));
	if (event) {
		event->type = CLIENT_EVENT_NONE;

		event->create_thread = false;
		event->drop_after_trigger = false;

		event->request_type = 0;
		event->response_data = NULL;
		event->delete_response_data = NULL;

		event->action = NULL;
		event->action_args = NULL;
		event->delete_action_args = NULL;
	}

	return event;

}

void client_event_delete (void *ptr) {

	if (ptr) {
		ClientEvent *event = (ClientEvent *) ptr;

		if (event->response_data) {
			if (event->delete_response_data)
				event->delete_response_data (event->response_data);
		}

		if (event->action_args) {
			if (event->delete_action_args)
				event->delete_action_args (event->action_args);
		}

		free (event);
	}

}

// registers an action to be triggered when the specified event occurs
// if there is an existing action registered to an event, it will be overrided
// a newly allocated ClientEventData structure will be passed to your method
// that should be free using the client_event_data_delete () method
// returns 0 on success, 1 on error
u8 client_event_register (
	Client *client,
	const ClientEventType event_type,
	Action action, void *action_args, Action delete_action_args,
	bool create_thread, bool drop_after_trigger
) {

	u8 retval = 1;

	if (client) {
		ClientEvent *event = client_event_new ();
		if (event) {
			event->type = event_type;

			event->create_thread = create_thread;
			event->drop_after_trigger = drop_after_trigger;

			event->action = action;
			event->action_args = action_args;
			event->delete_action_args = delete_action_args;

			// search if there is an action already registred for that event and remove it
			(void) client_event_unregister (client, event_type);

			client->events[event_type] = event;

			retval = 0;
		}
	}

	return retval;

}

// unregister the action associated with an event
// deletes the action args using the delete_action_args () if NOT NULL
// returns 0 on success, 1 on error or if event is NOT registered
u8 client_event_unregister (Client *client, const ClientEventType event_type) {

	u8 retval = 1;

	if (client) {
		if (client->events[event_type]) {
			client_event_delete (client->events[event_type]);
			client->events[event_type] = NULL;

			retval = 0;
		}
	}

	return retval;

}

void client_event_set_response (
	Client *client,
	const ClientEventType event_type,
	void *response_data, Action delete_response_data
) {

	if (client) {
		ClientEvent *event = client->events[event_type];
		if (event) {
			event->response_data = response_data;
			event->delete_response_data = delete_response_data;
		}
	}

}

// triggers all the actions that are registred to an event
void client_event_trigger (
	const ClientEventType event_type,
	const Client *client, const Connection *connection
) {

	if (client) {
		ClientEvent *event = client->events[event_type];
		if (event) {
			// trigger the action
			if (event->action) {
				if (event->create_thread) {
					pthread_t thread_id = 0;
					thread_create_detachable (
						&thread_id,
						(void *(*)(void *)) event->action,
						client_event_data_create (
							client, connection,
							event
						)
					);
				}

				else {
					event->action (client_event_data_create (
						client, connection,
						event
					));
				}

				if (event->drop_after_trigger) {
					(void) client_event_unregister ((Client *) client, event_type);
				}
			}
		}
	}

}

#pragma endregion