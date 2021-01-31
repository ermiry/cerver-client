#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "client/config.h"

#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/collections/dlist.h"

#include "client/client.h"
#include "client/errors.h"
#include "client/files.h"
#include "client/network.h"
#include "client/packets.h"

#include "client/utils/json.h"
#include "client/utils/log.h"
#include "client/utils/utils.h"

#pragma region main

// sanitizes a filename to correctly be used to save a file
// removes every character & whitespaces except for
// alphabet, numbers, '-', '_' and  '.'
void files_sanitize_filename (char *filename) {

	if (filename) {
		for (int i = 0, j; filename[i] != '\0'; ++i) {
			while (
				!(filename[i] >= 'a' && filename[i] <= 'z') && !(filename[i] >= 'A' && filename[i] <= 'Z')	// alphabet
				&& !(filename[i] >= 48 && filename[i] <= 57)												// numbers
				&& !(filename[i] == '-') && !(filename[i] == '_') && !(filename[i] == '.')					// clean characters
				&& !(filename[i] == '\0')
			) {
				for (j = i; filename[j] != '\0'; ++j) {
					filename[j] = filename[j + 1];
				}

				filename[j] = '\0';
			}
		}

		c_string_remove_spaces (filename);
	}

}

// check if a directory already exists, and if not, creates it
// returns 0 on success, 1 on error
unsigned int files_create_dir (
	const char *dir_path, mode_t mode
) {

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
					client_log_error (
						"Failed to create dir %s!", dir_path
					);
				}
			} break;
			case 0: {
				client_log_warning (
					"Dir %s already exists!", dir_path
				);
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
					(void) memcpy (retval, ptr + 1, ext_len);
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
		images = dlist_init (str_delete, str_comparator);

		DIR *dp = opendir (dir);
		if (dp) {
			struct dirent *ep = NULL;
			String *file = NULL;
			while ((ep = readdir (dp)) != NULL) {
				if (strcmp (ep->d_name, ".") && strcmp (ep->d_name, "..")) {
					file = str_create ("%s/%s", dir, ep->d_name);

					(void) dlist_insert_after (
						images, dlist_end (images), file
					);
				}
			}

			(void) closedir (dp);
		}

		else {
			client_log_error ("Failed to open dir %s", dir);
		}
	}

	return images;

}

static String *file_get_line (
	FILE *file,
	char *buffer, const size_t buffer_size
) {

	String *str = NULL;

	if (!feof (file)) {
		if (fgets (buffer, buffer_size, file)) {
			c_string_remove_line_breaks (buffer);
			str = str_new (buffer);
		}
	}

	return str;

}

// reads each one of the file's lines into newly created strings
// and returns them inside a dlist
DoubleList *file_get_lines (
	const char *filename, const size_t buffer_size
) {

	DoubleList *lines = NULL;

	if (filename) {
		FILE *file = fopen (filename, "r");
		if (file) {
			lines = dlist_init (str_delete, str_comparator);

			char *buffer = (char *) calloc (buffer_size, sizeof (char));
			if (buffer) {
				String *line = NULL;
			
				while ((line = file_get_line (file, buffer, buffer_size))) {
					(void) dlist_insert_at_end_unsafe (lines, line);
				}

				free (buffer);
			}

			(void) fclose (file);
		}

		else {
			client_log_error ("Failed to open file: %s", filename);
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
FILE *file_open_as_file (
	const char *filename, const char *modes, struct stat *filestatus
) {

	FILE *fp = NULL;

	if (filename) {
		(void) memset (filestatus, 0, sizeof (struct stat));
		if (!stat (filename, filestatus))
			fp = fopen (filename, modes);

		else {
			#ifdef FILES_DEBUG
			client_log (
				LOG_TYPE_ERROR, LOG_TYPE_FILE,
				"File %s not found!", filename
			);
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
				#ifdef FILES_DEBUG
				client_log (
					LOG_TYPE_ERROR, LOG_TYPE_FILE,
					"Failed to read file (%s) contents!", filename
				);
				#endif

				free (file_contents);
			}

			(void) fclose (fp);
		}

		else {
			#ifdef FILES_DEBUG
			client_log (
				LOG_TYPE_ERROR, LOG_TYPE_FILE,
				"Unable to open file %s.", filename
			);
			#endif
		}
	}

	return file_contents;

}

// opens a file with the required flags
// returns fd on success, -1 on error
int file_open_as_fd (
	const char *filename, struct stat *filestatus, int flags
) {

	int retval = -1;

	if (filename) {
		(void) memset (filestatus, 0, sizeof (struct stat));
		if (!stat (filename, filestatus)) {
			retval = open (filename, flags);
		}
	}

	return retval;

}

json_value *file_json_parse (const char *filename) {

	json_value *value = NULL;

	if (filename) {
		size_t file_size = 0;
		char *file_contents = file_read (filename, &file_size);
		json_char *json = (json_char *) file_contents;
		value = json_parse (json, file_size);

		free (file_contents);
	}

	return value;

}

#pragma endregion

#pragma region send

// sends a first packet with file info
// returns 0 on success, 1 on error
static u8 file_send_header (
	Client *client, Connection *connection,
	const char *filename, size_t filelen
) {

	u8 retval = 1;

	Packet *packet = packet_new ();
	if (packet) {
		size_t packet_len = sizeof (PacketHeader) + sizeof (FileHeader);

		packet->packet = malloc (packet_len);
		packet->packet_size = packet_len;

		char *end = (char *) packet->packet;
		PacketHeader *header = (PacketHeader *) end;
		header->packet_type = PACKET_TYPE_REQUEST;
		header->packet_size = packet_len;

		header->request_type = REQUEST_PACKET_TYPE_SEND_FILE;

		end += sizeof (PacketHeader);

		FileHeader *file_header = (FileHeader *) end;
		(void) strncpy (file_header->filename, filename, DEFAULT_FILENAME_LEN);
		file_header->len = filelen;

		packet_set_network_values (packet, client, connection);

		retval = packet_send_unsafe (packet, 0, NULL, false);

		packet_delete (packet);
	}

	return retval;

}

static ssize_t file_send_actual (
	Client *client, Connection *connection,
	int file_fd, const char *actual_filename, size_t filelen
) {

	ssize_t retval = 0;

	(void) pthread_mutex_lock (connection->socket->write_mutex);

	// send a first packet with file info
	if (!file_send_header (
		client, connection,
		actual_filename, filelen
	)) {
		// send the actual file
		retval = sendfile (connection->socket->sock_fd, file_fd, NULL, filelen);
	}

	else {
		client_log (
			LOG_TYPE_ERROR, LOG_TYPE_FILE, 
			"file_send_actual () - failed to send file header"
		);
	}

	(void) pthread_mutex_unlock (connection->socket->write_mutex);

	return retval;

}

static int file_send_open (
	const char *filename, struct stat *filestatus,
	const char **actual_filename
) {

	int file_fd = -1;

	char *last = strrchr ((char *) filename, '/');
	*actual_filename = last ? last + 1 : NULL;
	if (actual_filename) {
		// try to open the file
		file_fd = file_open_as_fd (filename, filestatus, O_RDONLY);
		if (file_fd <= 0) {
			client_log (
				LOG_TYPE_ERROR, LOG_TYPE_FILE,
				"file_send () - Failed to open file %s", filename
			);
		}
	}

	else {
		client_log_error ("file_send () - failed to get actual filename");
	}

	return file_fd;

}

// opens a file and sends its contents
// first the FileHeader in a regular packet, then the file contents between sockets
// returns the number of bytes sent, or -1 on error
ssize_t file_send (
	Client *client, Connection *connection,
	const char *filename
) {

	ssize_t retval = -1;

	if (filename && connection) {
		const char *actual_filename = NULL;
		struct stat filestatus = { 0 };
		int file_fd = file_send_open (filename, &filestatus, &actual_filename);
		if (file_fd > 0) {
			retval = file_send_actual (
				client, connection,
				file_fd, actual_filename, filestatus.st_size
			);

			(void) close (file_fd);
		}
	}

	return retval;

}

// sends the file contents of the file referenced by a fd
// first the FileHeader in a regular packet, then the file contents between sockets
// returns the number of bytes sent, or -1 on error
ssize_t file_send_by_fd (
	Client *client, Connection *connection,
	int file_fd, const char *actual_filename, size_t filelen
) {

	ssize_t retval = -1;

	if (client && connection && actual_filename) {
		retval = file_send_actual (
			client, connection,
			file_fd, actual_filename, filelen
		);
	}

	return retval;

}

#pragma endregion

#pragma region receive

// move from socket to pipe buffer
static inline u8 file_receive_internal_receive (
	Connection *connection, int pipefd, int buff_size,
	ssize_t *received
) {

	u8 retval = 1;

	*received = splice (
		connection->socket->sock_fd, NULL,
		pipefd, NULL,
		buff_size,
		SPLICE_F_MOVE | SPLICE_F_MORE
	);

	switch (*received) {
		case -1: {
			#ifdef FILES_DEBUG
			perror ("file_receive_internal_receive () - splice () = -1");
			#endif
		} break;

		case 0: {
			#ifdef FILES_DEBUG
			perror ("file_receive_internal_receive () - splice () = 0");
			#endif
		} break;

		default: {
			#ifdef FILES_DEBUG
			client_log_debug (
				"file_receive_internal_receive () - spliced %ld bytes",
				*received
			);
			#endif

			retval = 0;
		} break;
	}

	return retval;

}

// move from pipe buffer to file
static inline u8 file_receive_internal_move (
	int pipefd, int file_fd, int buff_size,
	ssize_t *moved
) {

	u8 retval = 1;

	*moved = splice (
		pipefd, NULL,
		file_fd, NULL,
		buff_size,
		SPLICE_F_MOVE | SPLICE_F_MORE
	);

	switch (*moved) {
		case -1: {
			#ifdef FILES_DEBUG
			perror ("file_receive_internal_move () - splice () = -1");
			#endif
		} break;

		case 0: {
			#ifdef FILES_DEBUG
			perror ("file_receive_internal_move () - splice () = 0");
			#endif
		} break;

		default: {
			#ifdef FILES_DEBUG
			client_log_debug (
				"file_receive_internal_move () - spliced %ld bytes",
				*moved
			);
			#endif

			retval = 0;
		} break;
	}

	return retval;

}

static u8 file_receive_internal (
	Connection *connection, size_t filelen, int file_fd
) {

	u8 retval = 1;

	size_t buff_size = 4096;
	int pipefds[2] = { 0 };
	ssize_t received = 0;
	ssize_t moved = 0;
	if (!pipe (pipefds)) {
		size_t len = filelen;
		while (len > 0) {
			if (buff_size > len) buff_size = len;

			if (file_receive_internal_receive (
				connection, pipefds[1], buff_size, &received
			)) break;

			if (file_receive_internal_move (
				pipefds[0], file_fd, buff_size, &moved
			)) break;

			len -= buff_size;
		}

		(void) close (pipefds[0]);
    	(void) close (pipefds[1]);

		if (len <= 0) retval = 0;
	}

	return retval;

}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

// opens the file using an already created filename
// and use the fd to receive and save the file
u8 file_receive_actual (
	Client *client, Connection *connection,
	FileHeader *file_header,
	const char *file_data, size_t file_data_len,
	char **saved_filename
) {

	u8 retval = 1;

	int file_fd = open (*saved_filename, O_CREAT | O_WRONLY | O_TRUNC, 0777);
	if (file_fd > 0) {
		// ssize_t received = splice (
		// 	connection->socket->sock_fd, NULL,
		// 	file_fd, NULL,
		// 	file_header->len,
		// 	0
		// );

		// we received some part of the file when reading packets,
		// they should be the first ones to be saved into the file
		if (file_data && file_data_len) {
			ssize_t wrote = write (file_fd, file_data, file_data_len);
			if (wrote < 0) {
				client_log_error ("file_receive_actual () - write () has failed!");
				perror ("Error");
				client_log_line_break ();
			}

			else {
				#ifdef FILES_DEBUG
				client_log_debug (
					"\n\nwrote %ld of file_data_len %ld\n\n",
					wrote,
					file_data_len
				);
				#endif
			}
		}

		// there is still more data to be received
		if (file_data_len < file_header->len) {
			size_t real_filelen = file_header->len - file_data_len;
			#ifdef FILES_DEBUG
			client_log_debug (
				"\nfilelen: %ld - file data len %ld = %ld\n\n",
				file_header->len, file_data_len, real_filelen
			);
			#endif
			if (!file_receive_internal (
				connection,
				real_filelen,
				file_fd
			)) {
				#ifdef FILES_DEBUG
				client_log_success ("file_receive_internal () has finished");
				#endif

				retval = 0;
			}

			else {
				free (*saved_filename);
				*saved_filename = NULL;
			}
		}

		(void) close (file_fd);
	}

	else {
		client_log_error ("file_receive_actual () - failed to open file");

		free (*saved_filename);
		*saved_filename = NULL;
	}

	return retval;

}

#pragma GCC diagnostic pop

#pragma endregion