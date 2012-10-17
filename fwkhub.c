#include <sys/types.h>
#include <sys/queue.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "map.h"
#include "remote.h"

#define	FAWORKEN_PORT		1981

struct client {
	TAILQ_ENTRY(client)	c_next;
	struct remote		*c_remote;
	struct actor		*c_actor;
	int			c_fd;
};

static TAILQ_HEAD(, client)	clients;
static struct map		*map;

static int
action_whereami(struct remote *r, char *str, char **uptr)
{
	struct client *c;

	c = (struct client *)uptr;

	remote_send(r, "ok, %d %d\r\n", map_actor_get_x(c->c_actor), map_actor_get_y(c->c_actor));
	return (0);
}

static int
action_move(struct remote *r, char *str, char **uptr)
{
	int error;
	struct client *c;

	c = (struct client *)uptr;

	if (strcmp(str, "north") == 0)
		error = map_actor_move_by(c->c_actor, 0, -1);
	else if (strcmp(str, "south") == 0)
		error = map_actor_move_by(c->c_actor, 0, 1);
	else if (strcmp(str, "west") == 0)
		error = map_actor_move_by(c->c_actor, -1, 0);
	else if (strcmp(str, "east") == 0)
		error = map_actor_move_by(c->c_actor, 1, 0);
	else {
		remote_send(r, "sorry, no idea where's that\r\n");
		return (0);
	}

	if (error == 0)
		remote_send(r, "ok\r\n");
	else
		remote_send(r, "sorry, can't go that way\r\n");
	return (0);
}

static int
action_map_get_size(struct remote *r, char *str, char **uptr)
{

	remote_send(r, "ok, %d %d\r\n", map_get_width(map), map_get_height(map));
	return (0);
}

static int
action_map_get(struct remote *r, char *str, char **uptr)
{
	unsigned int x, y;
	int assigned;
	char ch;

	assigned = sscanf(str, "map-get %d %d", &x, &y);
	if (assigned != 2) {
		remote_send(r, "sorry, invalid usage; should be 'map-get x y'\r\n");
		return (0);
	}
	if (x > map_get_width(map)) {
		remote_send(r, "sorry, too large x\r\n");
		return (0);
	}
	if (y > map_get_height(map)) {
		remote_send(r, "sorry, too large y\r\n");
		return (0);
	}
	ch = map_get(map, x, y);
	assert(ch != '\0');
	remote_send(r, "ok, '%c'\r\n", ch);
	return (0);
}

static int
action_map_set(struct remote *r, char *str, char **uptr)
{
	unsigned int x, y;
	int assigned;
	char ch;

	assigned = sscanf(str, "map-set %d %d %c", &x, &y, &ch);
	if (assigned != 3) {
		remote_send(r, "sorry, invalid usage; should be 'map-set x y ch'\r\n");
		return (0);
	}
	if (x > map_get_width(map)) {
		remote_send(r, "sorry, too large x\r\n");
		return (0);
	}
	if (y > map_get_height(map)) {
		remote_send(r, "sorry, too large y\r\n");
		return (0);
	}
	map_set(map, x, y, ch);
	remote_send(r, "ok\r\n");
	return (0);
}

static int
action_map_get_line(struct remote *r, char *str, char **uptr)
{
	unsigned int x, y, width;
	int assigned;
	char *line;

	assigned = sscanf(str, "map-get-line %d", &y);
	if (assigned != 1) {
		remote_send(r, "sorry, invalid usage; should be 'map-get-line y'\r\n");
		return (0);
	}
	if (y > map_get_height(map)) {
		remote_send(r, "sorry, too large y\r\n");
		return (0);
	}
	width = map_get_width(map);
	line = calloc(1, width + 1);
	if (line == NULL)
		err(1, "calloc");
	for (x = 0; x < width; x++)
		line[x] = map_get(map, x, y);
	remote_send(r, "ok, %s\r\n", line);
	free(line);
	return (0);
}

static int
action_bye(struct remote *r, char *str, char **uptr)
{
	struct client *c;

	c = (struct client *)uptr;

	remote_send(r, "ok, see you next time\r\n");
	/*
	 * Close the socket, so that the select() will notice
	 * and delete the client.
	 */
#if 0
	/*
	 * XXX: Nope, doesn't work.
	 */
	close(c->c_fd);
#endif
	return (0);
}

static int
action_say(struct remote *r, char *str, char **uptr)
{
	struct client *c;

	TAILQ_FOREACH(c, &clients, c_next) {
		/*
		 * XXX: Validate the string somehow.
		 */
		remote_send(c->c_remote, "%s\r\n", str);
	}
	return (0);
}

static int
action_unknown(struct remote *r, char *str, char **uptr)
{

	remote_send(r, "sorry, no idea what you mean\r\n");
	return (0);
}

static void
client_add(int fd)
{
	struct client *c;

	c = calloc(1, sizeof(*c));
	if (c == NULL)
		err(1, "calloc");

	c->c_fd = fd;
	c->c_remote = remote_new(fd);
	c->c_actor = map_actor_new(map);
	TAILQ_INSERT_TAIL(&clients, c, c_next);

	remote_expect(c->c_remote, "whereami", action_whereami, (char **)c);
	remote_expect(c->c_remote, "north", action_move, (char **)c);
	remote_expect(c->c_remote, "south", action_move, (char **)c);
	remote_expect(c->c_remote, "east", action_move, (char **)c);
	remote_expect(c->c_remote, "west", action_move, (char **)c);
	remote_expect(c->c_remote, "map-get-size", action_map_get_size, (char **)c);
	remote_expect(c->c_remote, "map-get", action_map_get, (char **)c);
	remote_expect(c->c_remote, "map-get-line", action_map_get_line, (char **)c);
	remote_expect(c->c_remote, "map-set", action_map_set, (char **)c);
	remote_expect(c->c_remote, "bye", action_bye, (char **)c);
	remote_expect(c->c_remote, "say", action_say, (char **)c);
	remote_expect(c->c_remote, "", action_unknown, (char **)c);
}

static void
client_remove(struct client *c)
{

	TAILQ_REMOVE(&clients, c, c_next);
	remote_delete(c->c_remote);
	free(c);
}

static struct client *
client_find_by_fd(int fd)
{
	struct client *c;

	TAILQ_FOREACH(c, &clients, c_next) {
		if (c->c_fd == fd)
			return (c);
	}

	return (NULL);
}

static void
client_receive(struct client *c)
{

	remote_process(c->c_remote);
}

static int
listen_on(int port)
{
	struct sockaddr_in sin;
	int sock, error;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		err(1, "socket");

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = INADDR_ANY;

	error = bind(sock, (struct sockaddr *)&sin, sizeof(sin));
	if (error != 0)
		err(1, "bind");

	error = listen(sock, 42);
	if (error != 0)
		err(1, "listen");

	return (sock);
}

static int
fd_add(int fd, fd_set *fdset, int nfds)
{
	FD_SET(fd, fdset);
	if (fd > nfds)
		nfds = fd;
	return (nfds);
}

static void
usage(void)
{

	printf("usage: fwkhub\n");
	exit(0);
}

int
main(int argc, char **argv)
{
	fd_set fdset;
	int error, i, nfds, client_fd, listening_socket;
	int map_edge_len = 300;
	struct client *client;
	char buf[1];

	if (argc != 1)
		usage();

	TAILQ_INIT(&clients);

	map = map_new(map_edge_len, map_edge_len);

	listening_socket = listen_on(FAWORKEN_PORT);

#if 0
	fprintf(stderr, "listening for clients on port %d\n", FAWORKEN_PORT);
#endif

	for (;;) {
		FD_ZERO(&fdset);
		nfds = 0;
		nfds = fd_add(listening_socket, &fdset, nfds);
		TAILQ_FOREACH(client, &clients, c_next)
			nfds = fd_add(client->c_fd, &fdset, nfds);
		error = select(nfds + 1, &fdset, NULL, NULL, NULL);
		if (error <= 0)
			err(1, "select");

		if (FD_ISSET(listening_socket, &fdset)) {
			client_fd = accept(listening_socket, NULL, 0);
			if (client_fd < 0)
				err(1, "accept");
#if 0
			fprintf(stderr, "fd %d: got new client\n", client_fd);
#endif
			client_add(client_fd);
			continue;
		}

		for (i = 0; i < nfds + 1; i++) {
			if (!FD_ISSET(i, &fdset))
				continue;

			client = client_find_by_fd(i);
			if (client != NULL) {
				/*
				 * Check if the socket is still connected.
				 */
				if (recv(i, buf, sizeof(buf), MSG_DONTWAIT | MSG_PEEK) <= 0) {
					client_remove(client);
					break;
				}
				client_receive(client);
				break;
			}
			errx(1, "unknown fd %d", i);
		}
		if (i == nfds + 1)
			errx(1, "fd not found");
	}

	return (0);
}
