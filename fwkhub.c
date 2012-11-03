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

struct client_actor {
	TAILQ_ENTRY(client_actor)	ca_next;
	unsigned int			ca_id;
	struct actor			*ca_actor;
	struct client			*ca_client;
	char				ca_char;
	char				*ca_name;
};

struct client {
	TAILQ_ENTRY(client)		c_next;
	struct remote			*c_remote;
	int				c_fd;
};

static TAILQ_HEAD(, client)		clients;
static TAILQ_HEAD(, client_actor)	actors;
static struct map			*map;

static unsigned int
client_actor_allocate_id(void)
{
	struct client_actor *ca;
	unsigned int max_id = 0;

	TAILQ_FOREACH(ca, &actors, ca_next) {
		if (ca->ca_id > max_id)
			max_id = ca->ca_id;
	}

	return (max_id + 1);
}

static unsigned int
client_actor_add(struct client *c, char ch, const char *name)
{
	struct client_actor *ca;

	ca = calloc(1, sizeof(*ca));
	if (ca == NULL)
		err(1, "calloc");

	ca->ca_id = client_actor_allocate_id();
	ca->ca_actor = map_actor_new(map);
	ca->ca_client = c;
	ca->ca_char = ch;
	ca->ca_name = strdup(name);
	if (ca->ca_name == NULL)
		err(1, "strdup");

	TAILQ_INSERT_TAIL(&actors, ca, ca_next);

	return (ca->ca_id);
}

static void
client_actor_remove(struct client_actor *ca)
{

	TAILQ_REMOVE(&actors, ca, ca_next);
	map_actor_delete(ca->ca_actor);
	free(ca->ca_name);
	free(ca);
}

static struct client_actor *
client_actor_find(struct client *c, unsigned int id)
{
	struct client_actor *ca;

	TAILQ_FOREACH(ca, &actors, ca_next) {
		if (ca->ca_client != c)
			continue;
		if (ca->ca_id != id)
			continue;
		return (ca);
	}

	return (NULL);
}

static int
action_actor_new(struct remote *r, char *str, char **uptr)
{
	struct client *c;
	unsigned int actor_id;
	int assigned;
	char ch;
	char name[32];

	c = (struct client *)uptr;

	assigned = sscanf(str, "actor-new '%c' %31s", &ch, name);
	if (assigned != 2) {
		remote_send(r, "sorry, invalid usage; should be 'actor-new 'X' name'\r\n");
		return (0);
	}

	actor_id = client_actor_add(c, ch, name);
	remote_send(r, "ok, your ID is %d\r\n", actor_id);
	return (0);
}

static int
action_actor_locate(struct remote *r, char *str, char **uptr)
{
	struct client *c;
	struct client_actor *ca;
	unsigned int actor_id;
	int assigned;

	c = (struct client *)uptr;

	assigned = sscanf(str, "actor-locate %d", &actor_id);
	if (assigned != 1) {
		remote_send(r, "sorry, invalid usage; should be 'actor-locate actor-id'\r\n");
		return (0);
	}

	ca = client_actor_find(c, actor_id);
	if (ca == NULL) {
		remote_send(r, "sorry, invalid actor-id\r\n");
		return (0);
	}

	remote_send(r, "ok, %d %d\r\n", map_actor_get_x(ca->ca_actor), map_actor_get_y(ca->ca_actor));
	return (0);
}

static int
action_actor_move(struct remote *r, char *str, char **uptr)
{
	struct client *c;
	struct client_actor *ca;
	unsigned int actor_id;
	int assigned, error;
	char direction[10];

	c = (struct client *)uptr;

	assigned = sscanf(str, "actor-move %d %9s", &actor_id, direction);
	if (assigned != 2) {
		remote_send(r, "sorry, invalid usage; should be 'actor-move actor-id north|south|east|west'\r\n");
		return (0);
	}

	ca = client_actor_find(c, actor_id);
	if (ca == NULL) {
		remote_send(r, "sorry, invalid actor-id\r\n");
		return (0);
	}

	if (strcmp(direction, "north") == 0)
		error = map_actor_move_by(ca->ca_actor, 0, -1);
	else if (strcmp(direction, "south") == 0)
		error = map_actor_move_by(ca->ca_actor, 0, 1);
	else if (strcmp(direction, "west") == 0)
		error = map_actor_move_by(ca->ca_actor, -1, 0);
	else if (strcmp(direction, "east") == 0)
		error = map_actor_move_by(ca->ca_actor, 1, 0);
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
	TAILQ_INSERT_TAIL(&clients, c, c_next);

	remote_expect(c->c_remote, "actor-new", action_actor_new, (char **)c);
	remote_expect(c->c_remote, "actor-locate", action_actor_locate, (char **)c);
	remote_expect(c->c_remote, "actor-move", action_actor_move, (char **)c);
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
	struct client_actor *ca, *tmpca;

	TAILQ_FOREACH_SAFE(ca, &actors, ca_next, tmpca) {
		if (ca->ca_client != c)
			continue;
		client_actor_remove(ca);
	}

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
	TAILQ_INIT(&actors);

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
