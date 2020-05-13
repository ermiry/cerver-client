#ifndef _CERVER_CLIENT_VERSION_H_
#define _CERVER_CLIENT_VERSION_H_

#define CERVER_CLIENT_VERSION                   "1.1"
#define CERVER_CLIENT_VERSION_NAME              "Release 1.1"
#define CERVER_CLIENT_VERSION_DATE			    "12/05/2020"
#define CERVER_CLIENT_VERSION_TIME			    "22:47 CST"
#define CERVER_CLIENT_VERSION_AUTHOR			"Erick Salas"

// print full cerver client version information 
extern void cerver_client_version_print_full (void);

// print the version id
extern void cerver_client_version_print_version_id (void);

// print the version name
extern void cerver_client_version_print_version_name (void);

#endif