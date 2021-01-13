#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include <client/types/string.h>

#include <client/collections/dlist.h>

#include <client/utils/utils.h>
#include <client/utils/log.h>

#include "users.h"

static DoubleList *users = NULL;

User *user_new (void) {

	User *user = (User *) malloc (sizeof (User));
	if (user) {
		user->id = NULL;
		user->name = NULL;
		user->username = NULL;
		user->password = NULL;
		user->role = NULL;
		user->iat = 0;
	}

	return user;

}

void user_delete (void *user_ptr) {

	if (user_ptr) {
		User *user = (User *) user_ptr;

		str_delete (user->id);
		str_delete (user->name);
		str_delete (user->username);
		str_delete (user->password);
		str_delete (user->role);

		free (user_ptr);

		printf ("user_delete () - User has been deleted!\n");
	}

}

int user_comparator (const void *a, const void *b) {

	return strcmp (((User *) a)->username->str, ((User *) b)->username->str);

}

void user_print (User *user) {

	if (user) {
		(void) printf ("id: %s\n", user->id->str);
		(void) printf ("name: %s\n", user->name->str);
		(void) printf ("username: %s\n", user->username->str);
		(void) printf ("role: %s\n", user->role->str);
	}

}

User *user_get_by_username (const char *username) {

	User *retval = NULL;

	for (ListElement *le = dlist_start (users); le; le = le->next) {
		if (!strcmp (((User *) le->data)->username->str, username)) {
			retval = (User *) le->data;
			break;
		}
	}

	return retval;

}

void user_add (User *user) {

	dlist_insert_after (users, dlist_end (users), user);

}

void users_init (void) {

	users = dlist_init (user_delete, user_comparator);

}

void users_end (void) {

	dlist_delete (users);

}