#ifndef _CERVER_FILES_H_
#define _CERVER_FILES_H_

#include <stdio.h>

#include <sys/stat.h>

#include "client/collections/dlist.h"

#include "client/config.h"

// check if a directory already exists, and if not, creates it
// returns 0 on success, 1 on error
CLIENT_EXPORT unsigned int files_create_dir (const char *dir_path, mode_t mode);

// returns an allocated string with the file extension
// NULL if no file extension
CLIENT_EXPORT char *files_get_file_extension (const char *filename);

// returns a list of strings containg the names of all the files in the directory
CLIENT_EXPORT DoubleList *files_get_from_dir (const char *dir);

// reads eachone of the file's lines into a newly created string and returns them inside a dlist
CLIENT_EXPORT DoubleList *file_get_lines (const char *filename);

// opens a file and returns it as a FILE
CLIENT_EXPORT FILE *file_open_as_file (const char *filename, 
    const char *modes, struct stat *filestatus);

// opens and reads a file into a buffer
// sets file size to the amount of bytes read
CLIENT_EXPORT char *file_read (const char *filename, size_t *file_size);

// opens a file with the required flags
// returns fd on success, -1 on error
CLIENT_EXPORT int file_open_as_fd (const char *filename, struct stat *filestatus, int flags);

// sends a file to the sock fd
// returns 0 on success, 1 on error
CLIENT_EXPORT int file_send (const char *filename, int sock_fd);

#endif