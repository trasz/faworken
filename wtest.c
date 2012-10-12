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

int
main(void)
{
	struct w_window *root, *map_window, *character;
	struct map *map;
	int map_edge_len = 300;
	unsigned int x, y;

	root = w_init();
	map_window = w_window_new(root);
	w_window_resize(map_window, map_edge_len, map_edge_len);

	map = map_make(map_window);

	character = w_window_new(map_window);
	w_window_resize(character, 1, 1);
	map_find_empty_spot(map, &x, &y);
	w_window_move(character, x, y);
	w_window_move_cursor(character, 0, 0);
	w_window_putstr(character, 0, 0, "@");

	/*
	 * Move the map to the position where the character is in the center of the screen.
	 */
	w_window_move(map_window, -x + w_window_get_width(root) / 2, -y + w_window_get_height(root) / 2);

	w_window_bind(character, 'j', character_callback);
	w_window_bind(character, 'k', character_callback);
	w_window_bind(character, 'h', character_callback);
	w_window_bind(character, 'l', character_callback);
	w_window_bind(character, '?', character_callback);

	for (;;) {
		w_redraw(root);
		w_wait_for_key(character);
	}
	w_fini(root);
	return (0);
}
