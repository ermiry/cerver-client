#ifndef _CLIENT_VERSION_H_
#define _CLIENT_VERSION_H_

#define CLIENT_VERSION                      "1.2.2"
#define CLIENT_VERSION_NAME                 "Release 1.2.2"
#define CLIENT_VERSION_DATE			        "19/06/2020"
#define CLIENT_VERSION_TIME			        "05:51 CST"
#define CLIENT_VERSION_AUTHOR			    "Erick Salas"

// print full cerver client version information 
extern void cerver_client_version_print_full (void);

// print the version id
extern void cerver_client_version_print_version_id (void);

// print the version name
extern void cerver_client_version_print_version_name (void);

#endif