#include <assert.h>
#include <curses.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "win.h"
#include "map.h"

static void
help_callback(struct window *w, int key)
{
	switch (key) {
	case 'q':
		window_delete(w);
		break;
	default:
		errx(1, "unknown key %d", key);
	}
}

static void
help(struct window *character)
{
	struct window *root, *w;
	int help_width = 20, help_height = 20;

	root = window_get_root(character);
	w = window_framed_new(root, "help");
	window_resize(w, help_width, help_height);
	window_move(w, (window_get_width(root) - help_width) / 2, (window_get_height(root) - help_height) / 2);
	window_putstr(w, 0, 0, "first line");
	window_putstr(w, 0, help_height - 1, "last line");
	window_bind(w, 'q', help_callback);
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

	switch (key) {
	case 'h':
	case KEY_LEFT:
		window_move_by(w, -1, 0);
		break;
	case 'l':
	case KEY_RIGHT:
		window_move_by(w, 1, 0);
		break;
	case 'k':
	case KEY_UP:
		window_move_by(w, 0, -1);
		break;
	case 'j':
	case KEY_DOWN:
		window_move_by(w, 0, 1);
		break;
	case '?':
		help(w);
		break;
	default:
		errx(1, "unknown key %d", key);
	}

	scroll_map(w);
}

int
main(void)
{
	struct window *root, *map_window, *character;
	struct map *map;
	int map_edge_len = 300;
	unsigned int x, y;

	root = w_init();
	map_window = window_new(root);
	window_resize(map_window, map_edge_len, map_edge_len);

	map = map_make(map_window);

	character = window_new(map_window);
	window_resize(character, 1, 1);
	map_find_empty_spot(map, &x, &y);
	window_move(character, x, y);
	window_move_cursor(character, 0, 0);
	window_putstr(character, 0, 0, "@");

	center_map(character);

	window_bind(character, 'j', character_callback);
	window_bind(character, KEY_DOWN, character_callback);
	window_bind(character, 'k', character_callback);
	window_bind(character, KEY_UP, character_callback);
	window_bind(character, 'h', character_callback);
	window_bind(character, KEY_LEFT, character_callback);
	window_bind(character, 'l', character_callback);
	window_bind(character, KEY_RIGHT, character_callback);
	window_bind(character, '?', character_callback);

	for (;;) {
		w_redraw(root);
		w_wait_for_key(character);
	}
	w_fini(root);
	return (0);
}
