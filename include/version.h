#ifndef _CERVER_CLIENT_VERSION_H_
#define _CERVER_CLIENT_VERSION_H_

#define VERSION                 "1.0"
#define VERSION_NAME            "Release 1.0"
#define VERSION_DATE			"20/01/2020"
#define VERSION_TIME			"18:01 CST"
#define VERSION_AUTHOR			"Erick Salas"

// print full cengine version information 
extern void version_print_full (void);

// print the version id
extern void version_print_version_id (void);

// print the version name
extern void version_print_version_name (void);

#endif