#ifndef _CERVER_CLIENT_VERSION_H_
#define _CERVER_CLIENT_VERSION_H_

#define CERVER_CLIENT_VERSION                   "1.2.1"
#define CERVER_CLIENT_VERSION_NAME              "Release 1.2.1"
#define CERVER_CLIENT_VERSION_DATE			    "18/06/2020"
#define CERVER_CLIENT_VERSION_TIME			    "03:06 CST"
#define CERVER_CLIENT_VERSION_AUTHOR			"Erick Salas"

// print full cerver client version information 
extern void cerver_client_version_print_full (void);

// print the version id
extern void cerver_client_version_print_version_id (void);

// print the version name
extern void cerver_client_version_print_version_name (void);

#endif