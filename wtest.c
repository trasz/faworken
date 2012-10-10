#include <assert.h>
#include <curses.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wieland.h"

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

static void
make_caves(struct w_window *map, unsigned int width, unsigned int height, unsigned int *number_of_cellsp, unsigned int *number_of_empty_cellsp)
{
	int x, y, cx, cy, rx, ry;
	char c;

	while (*number_of_empty_cellsp < *number_of_cellsp / 3) {
		x = rand() % width;
		y = rand() % height;

		/*
		 * XXX: Gaussian distribution.
		 */
		ry = rand() % 8 + 2;
		rx = rand() % 8 + 2;

		for (cy = y - ry; cy < y + ry; cy++) {
			for (cx = x - rx; cx < x + rx; cx++) {
				c = w_window_get(map, cx, cy);
				if (c == '#')
					(*number_of_empty_cellsp)++;
				w_window_putstr(map, cx, cy, " ");
			}
		}
	}
}

static void
make_tunnels(struct w_window *map, unsigned int width, unsigned int height, unsigned int *number_of_cellsp, unsigned int *number_of_empty_cellsp)
{
	int x, y, vx, vy, dir;
	char c;

	while (*number_of_empty_cellsp / 3 < *number_of_cellsp / 7) {
		/*
		 * Find a random empty cell.
		 */
		for (;;) {
			x = rand() % width;
			y = rand() % height;
			c = w_window_get(map, x, y);
			if (c == ' ')
				break;
		}

		vx = vy = 0;

		for (;;) {
			/*
			 * If we approach the edge of the map - make a turn.
			 */
			if (x + vx <= 1 || x + vx >= width - 2 || y + vy <= 1 || y + vy >= height - 2)
				vx = vy = 0;

			/*
			 * Perhaps make a turn.
			 */
			if ((vx == 0 && vy == 0) || rand() % 100 > 90) {
				/*
				 * Change 'dir' by +1/-1.  This is to make sure
				 * we never turn around 180 degrees in place.
				 */
				if (rand() % 2 == 1)
					dir++;
				else
					dir--;

				if (dir > 3)
					dir = 0;
				else if (dir < 0)
					dir = 3;

				switch (dir) {
				case 0:
					vx = 0;
					vy = -1;
					break;
				case 1:
					vx = 1;
					vy = 0;
					break;
				case 2:
					vx = 0;
					vy = 1;
					break;
				case 3:
					vx = -1;
					vy = 0;
					break;
				default:
					assert(!"meh");
				}
			}
			assert(vx != 0 || vy != 0);

			x += vx;
			y += vy;

			/*
			 * For some reason we've approached the edge.  Don't go any further.
			 */
			if (x + vx <= 0 || x + vx >= width - 1 || y + vy <= 0 || y + vy >= height - 1)
				break;

			/*
			 * Perhaps end here, if it joins some other corridor.
			 */
			c = w_window_get(map, x, y);
			assert(c != '\0');
			if (c == ' ') {
				if (rand() % 100 > 95)
					break;
			} else {
				(*number_of_empty_cellsp)++;
				w_window_putstr(map, x, y, " ");
			}
		}
	}
}

static void
make_map(struct w_window *map, unsigned int width, unsigned int height)
{
	unsigned int x, y;
	unsigned int number_of_cells, number_of_empty_cells;

	sranddev();

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			w_window_putstr(map, x, y, "#");
		}
	}

	number_of_cells = width * height;
	number_of_empty_cells = 0;

	make_caves(map, width, height, &number_of_cells, &number_of_empty_cells);
	make_tunnels(map, width, height, &number_of_cells, &number_of_empty_cells);
}

int
main(void)
{
	struct w_window *root, *map, *character;
	int map_edge_len = 300;

	root = w_init();

	map = w_window_new(root);
	w_window_resize(map, map_edge_len, map_edge_len);
	w_window_move(map, -map_edge_len / 3, -map_edge_len / 3);
	make_map(map, map_edge_len, map_edge_len);

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
