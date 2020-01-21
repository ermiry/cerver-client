#include <stdio.h>

#include "version.h"

// print full version information 
void version_print_full (void) {

    printf ("\n\nCengine Version: %s\n", VERSION_NAME);
    printf ("Release Date & time: %s - %s\n", VERSION_DATE, VERSION_TIME);
    printf ("Author: %s\n\n", VERSION_AUTHOR);

}

// print the version id
void version_print_version_id (void) {

    printf ("\n\nCengine Version ID: %s\n", VERSION);

}

// print the version name
void version_print_version_name (void) {

    printf ("\n\nCengine Version: %s\n", VERSION_NAME);

}