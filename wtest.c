#include <assert.h>
#include <curses.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "win.h"
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
center_map(struct w_window *character)
{
	struct w_window *root, *map;
	unsigned int screen_width, screen_height, x_char, y_char;
	int x_off, y_off;

	map = w_window_get_parent(character);
	root = w_window_get_root(character);
	screen_width = w_window_get_width(root);
	screen_height = w_window_get_height(root);

	x_char = w_window_get_x(character);
	y_char = w_window_get_y(character);

	/*
	 * Move the map to the position where the character is in the center of the screen.
	 */
	w_window_move(map, -x_char + w_window_get_width(root) / 2, -y_char + w_window_get_height(root) / 2);

	/*
	 * Now move the map so that it covers the whole screen, even if that means the character
	 * won't be in the center any more.
	 */
	x_off = -w_window_get_x(map);
	if (x_off < 0)
		w_window_move_by(map, x_off, 0);

	x_off = -w_window_get_x(map) - w_window_get_width(map) + screen_width;
	if (x_off > 0)
		w_window_move_by(map, x_off, 0);

	y_off = -w_window_get_y(map);
	if (y_off < 0)
		w_window_move_by(map, 0, y_off);

	y_off = -w_window_get_y(map) - w_window_get_height(map) + screen_height;
	if (y_off > 0)
		w_window_move_by(map, 0, y_off);
}

static void
scroll_map(struct w_window *character)
{
	struct w_window *root, *map;
	unsigned int screen_width, screen_height;
	unsigned int x_margin, y_margin;

	map = w_window_get_parent(character);
	root = w_window_get_root(character);
	screen_width = w_window_get_width(root);
	screen_height = w_window_get_height(root);

	x_margin = screen_width / 4;
	if (x_margin > 10)
		x_margin = 10;

	y_margin = screen_height / 4;
	if (y_margin > 10)
		y_margin = 10;

	if (w_window_get_x(map) < 0 &&
	    w_window_get_x(character) < -w_window_get_x(map) + x_margin)
		w_window_move_by(map, 1, 0);
	else if (w_window_get_x(map) + w_window_get_width(map) > screen_width &&
	    w_window_get_x(character) > -w_window_get_x(map) + screen_width - x_margin)
		w_window_move_by(map, -1, 0);
	if (w_window_get_y(map) < 0 &&
	    w_window_get_y(character) < -w_window_get_y(map) + y_margin)
		w_window_move_by(map, 0, 1);
	else if (w_window_get_y(map) + w_window_get_height(map) > screen_height &&
	    w_window_get_y(character) > -w_window_get_y(map) + screen_height - y_margin)
		w_window_move_by(map, 0, -1);
}

static void
character_callback(struct w_window *w, int key)
{

	switch (key) {
	case 'h':
	case KEY_LEFT:
		w_window_move_by(w, -1, 0);
		break;
	case 'l':
	case KEY_RIGHT:
		w_window_move_by(w, 1, 0);
		break;
	case 'k':
	case KEY_UP:
		w_window_move_by(w, 0, -1);
		break;
	case 'j':
	case KEY_DOWN:
		w_window_move_by(w, 0, 1);
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

	center_map(character);

	w_window_bind(character, 'j', character_callback);
	w_window_bind(character, KEY_DOWN, character_callback);
	w_window_bind(character, 'k', character_callback);
	w_window_bind(character, KEY_UP, character_callback);
	w_window_bind(character, 'h', character_callback);
	w_window_bind(character, KEY_LEFT, character_callback);
	w_window_bind(character, 'l', character_callback);
	w_window_bind(character, KEY_RIGHT, character_callback);
	w_window_bind(character, '?', character_callback);

	for (;;) {
		w_redraw(root);
		w_wait_for_key(character);
	}
	w_fini(root);
	return (0);
}
