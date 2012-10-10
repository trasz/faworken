#include <assert.h>
#include <curses.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wieland.h"
#include "map.h"

static void
help_callback(struct w_window *w, int key)
{
	switch (key) {
	case 'q':
		w_window_delete(w);
		break;
	default:
		errx(1, "unknown key %d", key);
	}
}

static void
help(struct w_window *character)
{
	struct w_window *root, *w;
	int help_width = 20, help_height = 20;

	root = w_window_get_root(character);
	w = w_window_framed_new(root, "help");
	w_window_resize(w, help_width, help_height);
	w_window_move(w, (w_window_get_width(root) - help_width) / 2, (w_window_get_height(root) - help_height) / 2);
	w_window_putstr(w, 0, 0, "first line");
	w_window_putstr(w, 0, help_height - 1, "last line");
	w_window_bind(w, 'q', help_callback);
}

static void
character_callback(struct w_window *w, int key)
{
	switch (key) {
	case 'h':
		w_window_move_by(w, -1, 0);
		break;
	case 'l':
		w_window_move_by(w, 1, 0);
		break;
	case 'k':
		w_window_move_by(w, 0, -1);
		break;
	case 'j':
		w_window_move_by(w, 0, 1);
		break;
	case '?':
		help(w);
		break;
	default:
		errx(1, "unknown key %d", key);
	}
}

static void
map_callback(struct w_window *w, int key)
{
	switch (key) {
	case KEY_LEFT:
		w_window_move_by(w, -1, 0);
		break;
	case KEY_RIGHT:
		w_window_move_by(w, 1, 0);
		break;
	case KEY_UP:
		w_window_move_by(w, 0, -1);
		break;
	case KEY_DOWN:
		w_window_move_by(w, 0, 1);
		break;
	default:
		errx(1, "unknown key %d", key);
	}
}

int
main(void)
{
	struct w_window *root, *map, *character;
	int map_edge_len = 300;

	root = w_init();

	map = map_make(root, map_edge_len, map_edge_len);
	w_window_move(map, -map_edge_len / 3, -map_edge_len / 3);

	character = w_window_new(map);
	w_window_resize(character, 1, 1);
	w_window_move(character, map_edge_len / 2, map_edge_len / 2);
	w_window_move_cursor(character, 0, 0);
	w_window_putstr(character, 0, 0, "@");

	w_window_bind(character, 'j', character_callback);
	w_window_bind(character, 'k', character_callback);
	w_window_bind(character, 'h', character_callback);
	w_window_bind(character, 'l', character_callback);
	w_window_bind(character, '?', character_callback);

	w_window_bind(map, KEY_LEFT, map_callback);
	w_window_bind(map, KEY_RIGHT, map_callback);
	w_window_bind(map, KEY_UP, map_callback);
	w_window_bind(map, KEY_DOWN, map_callback);

	for (;;) {
		w_redraw(root);
		w_wait_for_key(character);
	}
	w_fini(root);
	return (0);
}
