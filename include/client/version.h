#ifndef _CLIENT_VERSION_H_
#define _CLIENT_VERSION_H_

#include "client/config.h"

#define CLIENT_VERSION				"1.4"
#define CLIENT_VERSION_NAME			"Release 1.4"
#define CLIENT_VERSION_DATE			"18/10/2021"
#define CLIENT_VERSION_TIME			"10:26 CST"
#define CLIENT_VERSION_AUTHOR		"Erick Salas"

#ifdef __cplusplus
extern "C" {
#endif

// print full cerver client version information
CLIENT_EXPORT void cerver_client_version_print_full (void);

// print the version id
CLIENT_EXPORT void cerver_client_version_print_version_id (void);

// print the version name
CLIENT_EXPORT void cerver_client_version_print_version_name (void);

#ifdef __cplusplus
}
#endif

#endif