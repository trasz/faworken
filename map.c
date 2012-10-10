#include <assert.h>
#include <err.h>
#include <stdlib.h>

#include "wieland.h"
#include "map.h"

struct map {
	unsigned int	m_width;
	unsigned int	m_height;
	unsigned int	m_number_of_cells;
	unsigned int	m_number_of_empty_cells;
	struct w_window	*m_window;
};

static void
map_make_caves(struct map *m)
{
	int x, y, cx, cy, rx, ry;
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
				c = w_window_get(m->m_window, cx, cy);
				if (c == '#')
					m->m_number_of_empty_cells++;
				w_window_putstr(m->m_window, cx, cy, " ");
			}
		}
	}
}

static void
map_make_tunnels(struct map *m)
{
	int x, y, vx, vy, dir;
	char c;

	while (m->m_number_of_empty_cells / 3 < m->m_number_of_cells / 7) {
		/*
		 * Find a random empty cell.
		 */
		for (;;) {
			x = rand() % m->m_width;
			y = rand() % m->m_height;
			c = w_window_get(m->m_window, x, y);
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
			c = w_window_get(m->m_window, x, y);
			assert(c != '\0');
			if (c == ' ') {
				if (rand() % 100 > 95)
					break;
			} else {
				m->m_number_of_empty_cells++;
				w_window_putstr(m->m_window, x, y, " ");
			}
		}
	}
}

struct w_window *
map_make(struct w_window *parent, unsigned int width, unsigned int height)
{
	struct map *m;
	struct w_window *w;
	unsigned int x, y;

	m = calloc(1, sizeof(*m));
	if (m == NULL)
		err(1, "calloc");
	m->m_width = width;
	m->m_height = height;
	m->m_number_of_cells = width * height;
	m->m_window = w_window_new(parent);
	w_window_resize(m->m_window, width, height);

	sranddev();

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			w_window_putstr(m->m_window, x, y, "#");
		}
	}

	map_make_caves(m);
	map_make_tunnels(m);

	w = m->m_window;
	free(m);

	return (w);
}
