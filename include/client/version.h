#ifndef _CLIENT_VERSION_H_
#define _CLIENT_VERSION_H_

#include "client/config.h"

#define CLIENT_VERSION                      "1.3.1rc-5"
#define CLIENT_VERSION_NAME                 "Release 1.3.1rc-5"
#define CLIENT_VERSION_DATE			        "03/10/2020"
#define CLIENT_VERSION_TIME			        "17:15 CST"
#define CLIENT_VERSION_AUTHOR			    "Erick Salas"

// print full cerver client version information 
CLIENT_EXPORT void cerver_client_version_print_full (void);

// print the version id
CLIENT_EXPORT void cerver_client_version_print_version_id (void);

// print the version name
CLIENT_EXPORT void cerver_client_version_print_version_name (void);

#endif