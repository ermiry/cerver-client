#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#define _XOPEN_SOURCE 700
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/collections/dlist.h"

#include "client/network.h"
#include "client/packets.h"

#include "client/utils/utils.h"
#include "client/utils/log.h"

#pragma region main

// check if a directory already exists, and if not, creates it
// returns 0 on success, 1 on error
unsigned int files_create_dir (const char *dir_path, mode_t mode) {

	unsigned int retval = 1;

	if (dir_path) {
		struct stat st = { 0 };
		int ret = stat (dir_path, &st);
		switch (ret) {
			case -1: {
				if (!mkdir (dir_path, mode)) {
					retval = 0;		// success
				}

				else {
					char *s = c_string_create ("Failed to create dir %s!", dir_path);
					if (s) {
						client_log_error (s);
						free (s);
					}
				}
			} break;
			case 0: {
				char *s = c_string_create ("Dir %s already exists!", dir_path);
				if (s) {
					client_log_warning (s);
					free (s);
				}
			} break;

			default: break;
		}
	}

	return retval;

}

// returns an allocated string with the file extension
// NULL if no file extension
char *files_get_file_extension (const char *filename) {

	char *retval = NULL;

	if (filename) {
		char *ptr = strrchr ((char *) filename, '.');
		if (ptr) {
			// *ptr++;
			size_t ext_len = 0;
			char *p = ptr;
			while (*p++) ext_len++;

			if (ext_len) {
				retval = (char *) calloc (ext_len + 1, sizeof (char));
				if (retval) {
					memcpy (retval, ptr + 1, ext_len);
					retval[ext_len] = '\0';
				}
			}
		}

	}

	return retval;

}

// returns a list of strings containg the names of all the files in the directory
DoubleList *files_get_from_dir (const char *dir) {

	DoubleList *images = NULL;

	if (dir) {
		DIR *dp;
		struct dirent *ep;

		images = dlist_init (str_delete, str_comparator);

		dp = opendir (dir);
		if (dp) {
			String *file = NULL;
			while ((ep = readdir (dp)) != NULL) {
				if (strcmp (ep->d_name, ".") && strcmp (ep->d_name, "..")) {
					file = str_create ("%s/%s", dir, ep->d_name);

					dlist_insert_after (images, dlist_end (images), file);
				}
			}

			(void) closedir (dp);
		}

		else {
			char *status = c_string_create ("Failed to open dir %s", dir);
			if (status) {
				client_log_error (status);
				free (status);
			}
		}
	}

	return images;

}

static String *file_get_line (FILE *file) {

	String *str = NULL;

	if (file) {
		if (!feof (file)) {
			char line[1024] = { 0 };
			if (fgets (line, 1024, file)) {
				size_t curr = strlen(line);
				if(line[curr - 1] == '\n') line[curr - 1] = '\0';

				str = str_new (line);
			}
		}
	}

	return str;

}

// reads eachone of the file's lines into a newly created string and returns them inside a dlist
DoubleList *file_get_lines (const char *filename) {

	DoubleList *lines = NULL;

	if (filename) {
		FILE *file = fopen (filename, "r");
		if (file) {
			lines = dlist_init (str_delete, str_comparator);

			String *line = NULL;
			while ((line = file_get_line (file))) {
				dlist_insert_after (lines, dlist_end (lines), line);
			}

			fclose (file);
		}

		else {
			char *status = c_string_create ("Failed to open file: %s", filename);
			if (status) {
				client_log_error (status);
				free (status);
			}
		}
	}

	return lines;

}

// returns true if the filename exists
bool file_exists (const char *filename) {

	bool retval = false;

	if (filename) {
		struct stat filestatus = { 0 };
		if (!stat (filename, &filestatus)) {
			retval = true;
		}
	}

	return retval;

}

// opens a file and returns it as a FILE
FILE *file_open_as_file (const char *filename, const char *modes, struct stat *filestatus) {

	FILE *fp = NULL;

	if (filename) {
		memset (filestatus, 0, sizeof (struct stat));
		if (!stat (filename, filestatus))
			fp = fopen (filename, modes);

		else {
			#ifdef CLIENT_DEBUG
			char *s = c_string_create ("File %s not found!", filename);
			if (s) {
				client_log_msg (stderr, LOG_TYPE_ERROR, LOG_TYPE_FILE, s);
				free (s);
			}
			#endif
		}
	}

	return fp;

}

// opens and reads a file into a buffer
// sets file size to the amount of bytes read
char *file_read (const char *filename, size_t *file_size) {

	char *file_contents = NULL;

	if (filename) {
		struct stat filestatus = { 0 };
		FILE *fp = file_open_as_file (filename, "rt", &filestatus);
		if (fp) {
			*file_size = filestatus.st_size;
			file_contents = (char *) malloc (filestatus.st_size);

			// read the entire file into the buffer
			if (fread (file_contents, filestatus.st_size, 1, fp) != 1) {
				#ifdef CLIENT_DEBUG
				char *s = c_string_create ("Failed to read file (%s) contents!");
				if (s) {
					client_log_msg (stderr, LOG_TYPE_ERROR, LOG_TYPE_FILE, s);
					free (s);
				}
				#endif

				free (file_contents);
			}

			fclose (fp);
		}

		else {
			#ifdef CLIENT_DEBUG
			char *s = c_string_create ("Unable to open file %s.", filename);
			if (s) {
				client_log_msg (stderr, LOG_TYPE_ERROR, LOG_TYPE_FILE, s);
				free (s);
			}
			#endif
		}
	}

	return file_contents;

}

// opens a file with the required flags
// returns fd on success, -1 on error
int file_open_as_fd (const char *filename, struct stat *filestatus, int flags) {

	int retval = -1;

	if (filename) {
		memset (filestatus, 0, sizeof (struct stat));
		if (!stat (filename, filestatus))
			retval = open (filename, flags);

		else {
			#ifdef CLIENT_DEBUG
			char *s = c_string_create ("File %s not found!", filename);
			if (s) {
				client_log_msg (stderr, LOG_TYPE_ERROR, LOG_TYPE_FILE, s);
				free (s);
			}
			#endif
		}
	}

	return retval;

}

#pragma endregion

#pragma region send

// sends a file to the sock fd
// returns 0 on success, 1 on error
int file_send (const char *filename, int sock_fd) {

	int retval = 1;

	if (filename) {
		// try to open the file
		struct stat filestatus;
		int fd = file_open_as_fd (filename, &filestatus, O_RDONLY);
		if (fd >= 0) {
			// send a first packet with file info
			// TODO:
		}

		else {
			#ifdef CLIENT_DEBUG
			char *s = c_string_create ("Failed to open file %s.", filename);
			if (s) {
				client_log_msg (stderr, LOG_TYPE_ERROR, LOG_TYPE_FILE, s);
				free (s);
			}
			#endif
		}
	}

	return retval;

}

#pragma endregion