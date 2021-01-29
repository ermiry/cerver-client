#include "client/version.h"

#include "client/utils/log.h"

// print full erver version information
void client_version_print_full (void) {

	client_log_both (
		LOG_TYPE_NONE, LOG_TYPE_NONE,
		"Cerver Version: %s", CLIENT_VERSION_NAME
	);

	client_log_both (
		LOG_TYPE_NONE, LOG_TYPE_NONE,
		"Release Date & time: %s - %s",
		CLIENT_VERSION_DATE, CLIENT_VERSION_TIME
	);

	client_log_both (
		LOG_TYPE_NONE, LOG_TYPE_NONE,
		"Author: %s\n",
		CLIENT_VERSION_AUTHOR
	);

}

// print the version id
void client_version_print_version_id (void) {

	client_log_both (
		LOG_TYPE_NONE, LOG_TYPE_NONE,
		"Cerver Version ID: %s\n",
		CLIENT_VERSION
	);

}

// print the version name
void client_version_print_version_name (void) {

	client_log_both (
		LOG_TYPE_NONE, LOG_TYPE_NONE,
		"Cerver Version: %s\n",
		CLIENT_VERSION_NAME
	);

}