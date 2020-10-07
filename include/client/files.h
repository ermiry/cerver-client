#ifndef _CLIENT_FILES_H_
#define _CLIENT_FILES_H_

#include <stdio.h>

#include <sys/stat.h>

#include "client/types/types.h"

#include "client/collections/dlist.h"

#include "client/config.h"

#pragma region main

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

// returns true if the filename exists
CLIENT_EXPORT bool file_exists (const char *filename);

// opens a file and returns it as a FILE
CLIENT_EXPORT FILE *file_open_as_file (
	const char *filename,
	const char *modes, struct stat *filestatus
);

// opens and reads a file into a buffer
// sets file size to the amount of bytes read
CLIENT_EXPORT char *file_read (const char *filename, size_t *file_size);

// opens a file with the required flags
// returns fd on success, -1 on error
CLIENT_EXPORT int file_open_as_fd (const char *filename, struct stat *filestatus, int flags);

#pragma endregion

#pragma region send

#define DEFAULT_FILENAME_LEN			1024

struct _FileHeader {

	char filename[DEFAULT_FILENAME_LEN];
	size_t len;

};

typedef struct _FileHeader FileHeader;

// opens a file and sends the content back to the client
// first the FileHeader in a regular packet, then the file contents between sockets
// returns the number of bytes sent
CLIENT_PUBLIC ssize_t file_send (
	Client *client, Connection *connection,
	const char *filename
);

// receives an incomming file in the socket and splice its information to a local file
// returns 0 on success, 1 on error
CLIENT_PUBLIC u8 file_receive (
	Client *client, Connection *connection,
	FileHeader *file_header, char **saved_filename
);

#pragma endregion

#endif