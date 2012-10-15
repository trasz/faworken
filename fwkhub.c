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

struct action {
	TAILQ_ENTRY(action)	a_next;
	const char		*a_name;
	void			(*a_handler)(struct client *c, char *name);
};

static TAILQ_HEAD(, client)	clients;
static TAILQ_HEAD(, action)	actions;
static struct map		*map;

static void
action_add(const char *name, void (*handler)(struct client *c, char *name))
{
	struct action *a;

	a = calloc(1, sizeof(*a));
	if (a == NULL)
		err(1, "calloc");

	a->a_name = name;
	a->a_handler = handler;
	TAILQ_INSERT_TAIL(&actions, a, a_next);
}

static struct action *
action_find(const char *name)
{
	struct action *a;

	TAILQ_FOREACH(a, &actions, a_next) {
		if (strcmp(name, a->a_name) == 0)
			return (a);
	}

	return (NULL);
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
client_send(struct client *c, const char *msg)
{

	remote_send(c->c_remote, msg);
}

static int
client_execute(struct client *c, char *cmd)
{
	struct action *a;
	char *name;

#if 0
	fprintf(stderr, "'%s'\n", cmd);
#endif

	name = strdup(cmd);
	if (name == NULL)
		err(1, "strdup");
	name = strsep(&name, " ");
	a = action_find(name);
	free(name);
	if (a == NULL) {
		/*
		 * Not a real action, since we need to exit the loop
		 * when this happens.
		 */
		if (strcmp(name, "bye") == 0) {
			client_send(c, "ok, see you next time\r\n");
			client_remove(c);
			return (1);
		}

		client_send(c, "sorry, unknown action\r\n");
		return (0);
	}

	a->a_handler(c, cmd);
	return (0);
}

static void
client_receive(struct client *c)
{
	char *cmd;
	int error;

	for (;;) {
		cmd = remote_receive_async(c->c_remote);
		if (cmd == NULL)
			break;
		error = client_execute(c, cmd);
		if (error != 0)
			break;
	}
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

static void
action_whereami(struct client *c, char *cmd)
{
	char *str;

	asprintf(&str, "ok, %d %d\r\n", map_actor_get_x(c->c_actor), map_actor_get_y(c->c_actor));
	if (str == NULL)
		err(1, "asprintf");
	client_send(c, str);
	free(str);
}

static void
action_move(struct client *c, char *cmd)
{
	int error;

	if (strcmp(cmd, "north") == 0)
		error = map_actor_move_by(c->c_actor, 0, -1);
	else if (strcmp(cmd, "south") == 0)
		error = map_actor_move_by(c->c_actor, 0, 1);
	else if (strcmp(cmd, "west") == 0)
		error = map_actor_move_by(c->c_actor, -1, 0);
	else if (strcmp(cmd, "east") == 0)
		error = map_actor_move_by(c->c_actor, 1, 0);
	else {
		client_send(c, "sorry, no idea where's that\r\n");
		return;
	}

	if (error == 0)
		client_send(c, "ok\r\n");
	else
		client_send(c, "sorry, can't go that way\r\n");
}

static void
action_map_get_size(struct client *c, char *cmd)
{
	char *str;

	asprintf(&str, "ok, %d %d\r\n", map_get_width(map), map_get_height(map));
	if (str == NULL)
		err(1, "asprintf");
	client_send(c, str);
	free(str);
}

static void
action_map_get(struct client *c, char *cmd)
{
	unsigned int x, y;
	int assigned;
	char ch, *str;

	assigned = sscanf(cmd, "map-get %d %d", &x, &y);
	if (assigned != 2) {
		client_send(c, "sorry, invalid usage; should be 'map-get x y'\r\n");
		return;
	}
	if (x > map_get_width(map)) {
		client_send(c, "sorry, too large x\r\n");
		return;
	}
	if (y > map_get_height(map)) {
		client_send(c, "sorry, too large y\r\n");
		return;
	}
	ch = map_get(map, x, y);
	assert(ch != '\0');
	asprintf(&str, "ok, '%c'\r\n", ch);
	if (str == NULL)
		err(1, "asprintf");
	client_send(c, str);
	free(str);
}

static void
action_map_set(struct client *c, char *cmd)
{
	unsigned int x, y;
	int assigned;
	char ch;

	assigned = sscanf(cmd, "map-set %d %d %c", &x, &y, &ch);
	if (assigned != 3) {
		client_send(c, "sorry, invalid usage; should be 'map-set x y ch'\r\n");
		return;
	}
	if (x > map_get_width(map)) {
		client_send(c, "sorry, too large x\r\n");
		return;
	}
	if (y > map_get_height(map)) {
		client_send(c, "sorry, too large y\r\n");
		return;
	}
	map_set(map, x, y, ch);
	client_send(c, "ok\r\n");
}

static void
action_map_get_line(struct client *c, char *cmd)
{
	unsigned int x, y, width;
	int assigned;
	char *str, *line;

	assigned = sscanf(cmd, "map-get-line %d", &y);
	if (assigned != 1) {
		client_send(c, "sorry, invalid usage; should be 'map-get-line y'\r\n");
		return;
	}
	if (y > map_get_height(map)) {
		client_send(c, "sorry, too large y\r\n");
		return;
	}
	width = map_get_width(map);
	line = calloc(1, width + 1);
	if (line == NULL)
		err(1, "calloc");
	for (x = 0; x < width; x++)
		line[x] = map_get(map, x, y);
	asprintf(&str, "ok, %s\r\n", line);
	free(line);
	if (str == NULL)
		err(1, "asprintf");
	client_send(c, str);
	free(str);
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
	TAILQ_INIT(&actions);

	action_add("whereami", action_whereami);
	action_add("north", action_move);
	action_add("south", action_move);
	action_add("east", action_move);
	action_add("west", action_move);
	action_add("map-get-size", action_map_get_size);
	action_add("map-get", action_map_get);
	action_add("map-get-line", action_map_get_line);
	action_add("map-set", action_map_set);

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
