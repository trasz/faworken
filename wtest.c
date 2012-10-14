#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <curses.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "window.h"
#include "remote.h"

#define	FAWORKEN_PORT	1981

struct remote		*hub;

static void
server_map_get_size(unsigned int *width, unsigned int *height)
{
	char *reply;
	int assigned;

	remote_send(hub, "map-get-size\r\n");

	for (;;) {
		if (remote_disconnected(hub))
			errx(1, "lost connection to the hub");
		reply = remote_receive(hub);
		if (reply != NULL)
			break;
	}

	assigned = sscanf(reply, "ok, %d %d", width, height);
	if (assigned != 2)
		errx(1, "invalid reply to map-get-size: %s", reply);
}

static char
server_map_get(unsigned int x, unsigned int y)
{
	char *query, *reply;
	int assigned;
	char ch;

	asprintf(&query, "map-get %d %d\r\n", x, y);
	if (query == NULL)
		err(1, "asprintf");
	remote_send(hub, query);
	free(query);

	for (;;) {
		if (remote_disconnected(hub))
			errx(1, "lost connection to the hub");
		reply = remote_receive(hub);
		if (reply != NULL)
			break;
	}

	assigned = sscanf(reply, "ok, '%c'", &ch);
	if (assigned != 1)
		errx(1, "invalid reply to map-get: %s", reply);

	return (ch);
}

static int
server_move(const char *dir)
{
	char *query, *reply;

	asprintf(&query, "%s\r\n", dir);
	if (query == NULL)
		err(1, "asprintf");
	remote_send(hub, query);
	free(query);

	for (;;) {
		if (remote_disconnected(hub))
			errx(1, "lost connection to the hub");
		reply = remote_receive(hub);
		if (reply != NULL)
			break;
	}

	if (strncmp(reply, "ok", strlen("ok")) == 0)
		return (0);
	return (1);
}

static void
server_whereami(unsigned int *x, unsigned int *y)
{
	char *reply;
	int assigned;

	remote_send(hub, "whereami\r\n");

	for (;;) {
		if (remote_disconnected(hub))
			errx(1, "lost connection to the hub");
		reply = remote_receive(hub);
		if (reply != NULL)
			break;
	}

	assigned = sscanf(reply, "ok, %d %d", x, y);
	if (assigned != 2)
		errx(1, "invalid reply to whereami: %s", reply);
}

static struct window *
prepare_map_window(struct window *root)
{
	struct window *w;
	unsigned int x, y, width, height;
	char c, str[2];

	w = window_new(root);
	server_map_get_size(&width, &height);
	window_resize(w, width, height);

	str[1] = '\0';

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			c = server_map_get(x, y);
			str[0] = c;
			window_putstr(w, x, y, str);
		}
	}

	return (w);
}

static void
center_map(struct window *character)
{
	struct window *root, *map;
	unsigned int screen_width, screen_height, x_char, y_char;
	int x_off, y_off;

	map = window_get_parent(character);
	root = window_get_root(character);
	screen_width = window_get_width(root);
	screen_height = window_get_height(root);

	x_char = window_get_x(character);
	y_char = window_get_y(character);

	/*
	 * Move the map to the position where the character is in the center of the screen.
	 */
	window_move(map, -x_char + window_get_width(root) / 2, -y_char + window_get_height(root) / 2);

	/*
	 * Now move the map so that it covers the whole screen, even if that means the character
	 * won't be in the center any more.
	 */
	x_off = -window_get_x(map);
	if (x_off < 0)
		window_move_by(map, x_off, 0);

	x_off = -window_get_x(map) - window_get_width(map) + screen_width;
	if (x_off > 0)
		window_move_by(map, x_off, 0);

	y_off = -window_get_y(map);
	if (y_off < 0)
		window_move_by(map, 0, y_off);

	y_off = -window_get_y(map) - window_get_height(map) + screen_height;
	if (y_off > 0)
		window_move_by(map, 0, y_off);
}

static void
scroll_map(struct window *character)
{
	struct window *root, *map;
	unsigned int screen_width, screen_height;
	unsigned int x_margin, y_margin;

	map = window_get_parent(character);
	root = window_get_root(character);
	screen_width = window_get_width(root);
	screen_height = window_get_height(root);

	x_margin = screen_width / 4;
	if (x_margin > 10)
		x_margin = 10;

	y_margin = screen_height / 4;
	if (y_margin > 10)
		y_margin = 10;

	if (window_get_x(map) < 0 &&
	    window_get_x(character) < -window_get_x(map) + x_margin)
		window_move_by(map, 1, 0);
	else if (window_get_x(map) + window_get_width(map) > screen_width &&
	    window_get_x(character) > -window_get_x(map) + screen_width - x_margin)
		window_move_by(map, -1, 0);
	if (window_get_y(map) < 0 &&
	    window_get_y(character) < -window_get_y(map) + y_margin)
		window_move_by(map, 0, 1);
	else if (window_get_y(map) + window_get_height(map) > screen_height &&
	    window_get_y(character) > -window_get_y(map) + screen_height - y_margin)
		window_move_by(map, 0, -1);
}

static void
character_callback(struct window *w, int key)
{
	int error, x = 0, y = 0;

	switch (key) {
	case 'h':
	case KEY_LEFT:
		error = server_move("west");
		x = -1;
		break;
	case 'l':
	case KEY_RIGHT:
		error = server_move("east");
		x = 1;
		break;
	case 'k':
	case KEY_UP:
		error = server_move("north");
		y = -1;
		break;
	case 'j':
	case KEY_DOWN:
		error = server_move("south");
		y = 1;
		break;
	default:
		errx(1, "unknown key %d", key);
	}

	if (error != 0)
		return;

	window_move_by(w, x, y);
	scroll_map(w);
}

static struct window *
prepare_character_window(struct window *map_window)
{
	struct window *w;
	unsigned int x, y;

	server_whereami(&x, &y);

	w = window_new(map_window);
	window_resize(w, 1, 1);
	window_move(w, x, y);
	window_move_cursor(w, 0, 0);
	window_putstr(w, 0, 0, "@");

	center_map(w);

	window_bind(w, 'j', character_callback);
	window_bind(w, KEY_DOWN, character_callback);
	window_bind(w, 'k', character_callback);
	window_bind(w, KEY_UP, character_callback);
	window_bind(w, 'h', character_callback);
	window_bind(w, KEY_LEFT, character_callback);
	window_bind(w, 'l', character_callback);
	window_bind(w, KEY_RIGHT, character_callback);
	window_bind(w, '?', character_callback);

	return (w);
}

static int
connect_to(const char *ip, int port)
{
	struct sockaddr_in sin;
	int sock, error;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		err(1, "socket");

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = INADDR_ANY;

	if (inet_pton(AF_INET, ip, &sin.sin_addr) != 1)
		err(1, "inet_pton");

	error = connect(sock, (struct sockaddr *)&sin, sizeof(sin));
	if (error != 0)
		err(1, "connect");

	return (sock);
}

static int
invalid_ip(const char *ip)
{
	struct sockaddr_in sin;

	if (inet_pton(AF_INET, ip, &sin.sin_addr) != 1)
		return (1);

	return (0);
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

	printf("usage: fwk hub-ip [hub-port]\n");
	exit(0);
}

int
main(int argc, char **argv)
{
	struct window *root, *map_window, *character;
	int input_fd, hub_fd, hub_port, error, nfds;
	const char *hub_ip;
	fd_set fdset;

	if (argc < 2 || argc > 3)
		usage();

	/*
	 * XXX: Rewrite using getaddrinfo(3).
	 */
	hub_ip = argv[1];
	if (invalid_ip(hub_ip))
		errx(1, "invalid ip address");
	if (argc == 3) {
		hub_port = atoi(argv[2]);
		if (hub_port <= 0 || hub_port > 65535)
			errx(1, "invalid port number");
	} else
		hub_port = FAWORKEN_PORT;

	hub_fd = connect_to(hub_ip, hub_port);
	hub = remote_new(hub_fd);

	root = window_init();

	map_window = prepare_map_window(root);
	character = prepare_character_window(map_window);

	input_fd = window_get_input_fd(root);
	window_redraw(root);

	for (;;) {
		FD_ZERO(&fdset);
		nfds = 0;
		nfds = fd_add(input_fd, &fdset, nfds);
		nfds = fd_add(hub_fd, &fdset, nfds);
		error = select(nfds + 1, &fdset, NULL, NULL, NULL);
		if (error <= 0)
			err(1, "select");

		if (FD_ISSET(input_fd, &fdset)) {
			window_check_input_fd(character);
			window_redraw(root);
			continue;
		}
		if (FD_ISSET(hub_fd, &fdset)) {
			continue;
		}
		err(1, "select returned unknown fd");
	}

	window_fini(root);
	return (0);
}
