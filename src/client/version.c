#include <stdio.h>

#include "client/version.h"

// print full version information 
void cerver_client_version_print_full (void) {

    printf ("\n\nCerver Client Version: %s\n", CERVER_CLIENT_VERSION_NAME);
    printf ("Release Date & time: %s - %s\n", CERVER_CLIENT_VERSION_DATE, CERVER_CLIENT_VERSION_TIME);
    printf ("Author: %s\n\n", CERVER_CLIENT_VERSION_AUTHOR);

}

// print the version id
void cerver_client_version_print_version_id (void) {

    printf ("\n\nCengine Version ID: %s\n", CERVER_CLIENT_VERSION);

}

// print the version name
void cerver_client_version_print_version_name (void) {

    printf ("\n\nCengine Version: %s\n", CERVER_CLIENT_VERSION_NAME);

}