#ifndef _CLIENT_NETWORK_H_
#define _CLIENT_NETWORK_H_

#include <stdbool.h>

#include <unistd.h>
#include <fcntl.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "client/config.h"

#define IP_TO_STR_LEN       16
#define IPV6_TO_STR_LEN     46

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Protocol {

	PROTOCOL_TCP = IPPROTO_TCP,
	PROTOCOL_UDP = IPPROTO_UDP

} Protocol;

#define SOCK_SIZEOF_MEMBER(type, member) (sizeof(((type *) NULL)->member))

// gets the ip related to the hostname
// returns a newly allocated c string if found
// returns NULL on error or not found
CLIENT_PUBLIC char *network_hostname_to_ip (
	const char *hostname
);

// enable/disable blocking on a socket
// true on success, false if there was an error
CLIENT_PUBLIC bool sock_set_blocking (
	int32_t fd, bool blocking
);

CLIENT_PUBLIC unsigned int sock_ip_to_string_actual (
	const struct sockaddr *address,
	char *ip_buffer
);

CLIENT_PUBLIC char *sock_ip_to_string (
	const struct sockaddr *address
);

CLIENT_PUBLIC bool sock_ip_equal (
	const struct sockaddr *a, const struct sockaddr *b
);

CLIENT_PUBLIC in_port_t sock_ip_port (
	const struct sockaddr *address
);

// sets a timeout (in seconds) for a socket
// the socket will still block until the timeout is completed
// if no data was read, a EAGAIN error is returned
// returns 0 on success, 1 on error
CLIENT_PUBLIC int sock_set_timeout (
	int sock_fd, time_t timeout
);

// sets the socket's reusable options
// this should avoid errors when binding sockets
// returns 0 on success, 1 on any error
CLIENT_PUBLIC int sock_set_reusable (int sock_fd);

#ifdef __cplusplus
}
#endif

#endif