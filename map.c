#include <assert.h>
#include <err.h>
#include <stdlib.h>

#include "window.h"
#include "map.h"

struct map {
	unsigned int	m_width;
	unsigned int	m_height;
	unsigned int	m_number_of_cells;
	unsigned int	m_number_of_empty_cells;
	struct window	*m_window;
};

static void
map_make_caves(struct map *m)
{
	unsigned int x, y, cx, cy, rx, ry;
	char c;

	while (m->m_number_of_empty_cells < m->m_number_of_cells / 3) {
		x = rand() % m->m_width;
		y = rand() % m->m_height;

		/*
		 * XXX: Gaussian distribution.
		 */
		ry = rand() % 8 + 2;
		rx = rand() % 8 + 2;

		for (cy = y - ry; cy < y + ry; cy++) {
			for (cx = x - rx; cx < x + rx; cx++) {
				c = window_get(m->m_window, cx, cy);
				if (c == '#')
					m->m_number_of_empty_cells++;
				window_putstr(m->m_window, cx, cy, " ");
			}
		}
	}
}

static void
map_make_tunnels(struct map *m)
{
	unsigned int x, y; 
	int vx, vy, dir;
	char c;

	while (m->m_number_of_empty_cells / 3 < m->m_number_of_cells / 7) {
		/*
		 * Find a random empty cell.
		 */
		for (;;) {
			x = rand() % m->m_width;
			y = rand() % m->m_height;
			c = window_get(m->m_window, x, y);
			if (c == ' ')
				break;
		}

		vx = vy = 0;

		for (;;) {
			/*
			 * If we approach the edge of the map - make a turn.
			 */
			if (x + vx <= 1 || x + vx >= m->m_width - 2 || y + vy <= 1 || y + vy >= m->m_height - 2)
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
			if (x + vx <= 0 || x + vx >= m->m_width - 1 || y + vy <= 0 || y + vy >= m->m_height - 1)
				break;

			/*
			 * Perhaps end here, if it joins some other corridor.
			 */
			c = window_get(m->m_window, x, y);
			assert(c != '\0');
			if (c == ' ') {
				if (rand() % 100 > 95)
					break;
			} else {
				m->m_number_of_empty_cells++;
				window_putstr(m->m_window, x, y, " ");
			}
		}
	}
}

static void
map_make_walls(struct map *m)
{
	unsigned int x, y;
	char c, c2;

	for (y = 1; y < m->m_height - 1; y++) {
		for (x = 1; x < m->m_width - 1; x++) {
			c = window_get(m->m_window, x, y);
			assert(c != '\0');
			if (c == ' ')
				continue;
			c2 = window_get(m->m_window, x - 1, y);
			assert(c2 != '\0');
			if (c2 == ' ') {
				window_putstr(m->m_window, x, y, "|");
				continue;
			}

			c2 = window_get(m->m_window, x + 1, y);
			assert(c2 != '\0');
			if (c2 == ' ') {
				window_putstr(m->m_window, x, y, "|");
				continue;
			}
		}
	}

	for (y = 1; y < m->m_height - 1; y++) {
		for (x = 1; x < m->m_width - 1; x++) {
			c = window_get(m->m_window, x, y);
			assert(c != '\0');
			if (c == ' ')
				continue;

			c2 = window_get(m->m_window, x, y - 1);
			assert(c2 != '\0');
			if (c2 == ' ') {
				window_putstr(m->m_window, x, y, "-");
				continue;
			}

			c2 = window_get(m->m_window, x, y + 1);
			assert(c2 != '\0');
			if (c2 == ' ') {
				window_putstr(m->m_window, x, y, "-");
				continue;
			}
		}
	}
}

/*
 * The point of this is to remove walls thinner than three cells, which
 * would end up looking like those:
 *
 *   || 				|
 *   ||               -------------	|
 *   ||               -------------
 *
 */
static void
map_remove_thin_walls(struct map *m)
{
	unsigned int x, y;
	char c;

	/*
	 * Horizontal pass.
	 */
	for (y = 0; y < m->m_height; y++) {
		int cells = 0;
		for (x = 3; x < m->m_width; x++) {
			c = window_get(m->m_window, x, y);
			assert(c != '\0');
			if (c == ' ') {
				if (cells > 0 && cells < 3)
					window_putstr(m->m_window, x - 2, y, "  ");
				cells = 0;
				continue;
			} else {
				cells++;
			}
		}
	}

	/*
	 * Vertical pass.
	 */
	for (x = 0; x < m->m_width; x++) {
		int cells = 0;
		for (y = 3; y < m->m_height; y++) {
			c = window_get(m->m_window, x, y);
			assert(c != '\0');
			if (c == ' ') {
				if (cells > 0 && cells < 3) {
					window_putstr(m->m_window, x, y - 1, " ");
					window_putstr(m->m_window, x, y - 2, " ");
				}
				cells = 0;
				continue;
			} else {
				cells++;
			}
		}
	}
}

struct map *
map_make(struct window *map_window)
{
	struct map *m;
	unsigned int x, y;

	m = calloc(1, sizeof(*m));
	if (m == NULL)
		err(1, "calloc");
	m->m_width = window_get_width(map_window);
	m->m_height = window_get_height(map_window);
	m->m_number_of_cells = m->m_width * m->m_height;
	m->m_window = map_window;

	sranddev();

	/*
	 * Fill the map with solid rock.
	 */
	for (y = 0; y < m->m_height; y++) {
		for (x = 0; x < m->m_width; x++) {
			window_putstr(m->m_window, x, y, "#");
		}
	}

	map_make_caves(m);
	map_make_tunnels(m);

	/*
	 * Make sure there is no empty space at the border.
	 */
	for (y = 0; y < m->m_height; y++) {
		window_putstr(m->m_window, 0, y, "##");
		window_putstr(m->m_window, m->m_width - 2, y, "##");
	}
	for (x = 0; x < m->m_width; x++) {
		window_putstr(m->m_window, x, 0, "#");
		window_putstr(m->m_window, x, 1, "#");
		window_putstr(m->m_window, x, m->m_height - 1, "#");
		window_putstr(m->m_window, x, m->m_height - 2, "#");
	}

	map_remove_thin_walls(m);
	map_make_walls(m);

	return (m);
}

void
map_find_empty_spot(struct map *m, unsigned int *xp, unsigned int *yp)
{
	unsigned int x, y;
	char c;

	for (;;) {
		x = rand() % m->m_width;
		y = rand() % m->m_height;

		c = window_get(m->m_window, x, y);
		assert(c != '\0');

		if (c == ' ') {
			*xp = x;
			*yp = y;
			break;
		}
	}
}

