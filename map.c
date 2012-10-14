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
	char		*m_data;
};

struct actor {
	struct map	*a_map;
	unsigned int	a_x;
	unsigned int	a_y;
};

void
map_set(struct map *m, unsigned int x, unsigned int y, char c)
{

	if (x >= m->m_width)
		return;
	if (y >= m->m_height)
		return;

	m->m_data[m->m_width * y + x] = c;
}

char
map_get(struct map *m, unsigned int x, unsigned int y)
{

	if (x >= m->m_width)
		return ('\0');
	if (y >= m->m_height)
		return ('\0');

	return (m->m_data[m->m_width * y + x]);
}

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
				c = map_get(m, cx, cy);
				if (c == '#')
					m->m_number_of_empty_cells++;
				map_set(m, cx, cy, ' ');
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
			c = map_get(m, x, y);
			if (c == ' ')
				break;
		}

		vx = vy = 0;
		dir = rand() % 4;

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
			c = map_get(m, x, y);
			assert(c != '\0');
			if (c == ' ') {
				if (rand() % 100 > 95)
					break;
			} else {
				m->m_number_of_empty_cells++;
				map_set(m, x, y, ' ');
			}
		}
	}
}

static void
map_make_walls(struct map *m)
{
	unsigned int x, y;
	char c, prevc;

	for (x = 1; x < m->m_width - 1; x++) {
		prevc = '#';
		for (y = 1; y < m->m_height - 1; y++) {
			c = map_get(m, x, y);
			assert(c != '\0');
			if ((prevc == ' ' && c == '#') ||
			    (prevc == '|' && c == '#'))
				map_set(m, x, y, '-');
			prevc = c;
			continue;
		}
		prevc = '#';
		for (y = m->m_height - 1; y > 0; y--) {
			c = map_get(m, x, y);
			assert(c != '\0');
			if ((prevc == ' ' && c == '#') ||
			    (prevc == '|' && c == '#'))
				map_set(m, x, y, '-');
			prevc = c;
			continue;
		}
	}

	for (y = 1; y < m->m_height - 1; y++) {
		prevc = '#';
		for (x = 1; x < m->m_width - 1; x++) {
			c = map_get(m, x, y);
			assert(c != '\0');
			if ((prevc == ' ' && c == '#') ||
			    (prevc == '-' && c == '#'))
				map_set(m, x, y, '|');
			prevc = c;
			continue;
		}
		prevc = '#';
		for (x = m->m_width - 1; x > 0; x--) {
			c = map_get(m, x, y);
			assert(c != '\0');
			if ((prevc == ' ' && c == '#') ||
			    (prevc == '-' && c == '#'))
				map_set(m, x, y, '|');
			prevc = c;
			continue;
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
			c = map_get(m, x, y);
			assert(c != '\0');
			if (c == ' ') {
				if (cells > 0 && cells < 3) {
					map_set(m, x - 2, y, ' ');
					map_set(m, x - 1, y, ' ');
				}
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
			c = map_get(m, x, y);
			assert(c != '\0');
			if (c == ' ') {
				if (cells > 0 && cells < 3) {
					map_set(m, x, y - 1, ' ');
					map_set(m, x, y - 2, ' ');
				}
				cells = 0;
				continue;
			} else {
				cells++;
			}
		}
	}
}

static void
map_make_border(struct map *m)
{
	unsigned int x, y;

	/*
	 * Make sure there is no empty space at the border.
	 */
	for (y = 0; y < m->m_height; y++) {
		map_set(m, 0, y, '#');
		map_set(m, 1, y, '#');
		map_set(m, m->m_width - 1, y, '#');
		map_set(m, m->m_width - 2, y, '#');
	}
	for (x = 0; x < m->m_width; x++) {
		map_set(m, x, 0, '#');
		map_set(m, x, 1, '#');
		map_set(m, x, m->m_height - 1, '#');
		map_set(m, x, m->m_height - 2, '#');
	}
}

struct map *
map_new(unsigned int w, unsigned int h)
{
	struct map *m;
	unsigned int x, y;

	m = calloc(1, sizeof(*m));
	if (m == NULL)
		err(1, "calloc");
	m->m_width = w;
	m->m_height = h;
	m->m_number_of_cells = m->m_width * m->m_height;
	m->m_data = calloc(1, m->m_width * m->m_height);
	if (m->m_data == NULL)
		err(1, "calloc");

	sranddev();

	/*
	 * Fill the map with solid rock.
	 */
	for (y = 0; y < m->m_height; y++) {
		for (x = 0; x < m->m_width; x++)
			map_set(m, x, y, '#');
	}

	map_make_caves(m);
	map_make_tunnels(m);
	map_make_border(m);
	map_remove_thin_walls(m);
	map_make_walls(m);

	return (m);
}

unsigned int
map_get_width(struct map *m)
{

	return (m->m_width);
}

unsigned int
map_get_height(struct map *m)
{

	return (m->m_height);
}

static void
map_find_empty_spot(struct map *m, unsigned int *xp, unsigned int *yp)
{
	unsigned int x, y;
	char c;

	for (;;) {
		x = rand() % m->m_width;
		y = rand() % m->m_height;

		c = map_get(m, x, y);
		assert(c != '\0');

		if (c == ' ') {
			*xp = x;
			*yp = y;
			break;
		}
	}
}

struct actor *
map_actor_new(struct map *m)
{
	struct actor *a;

	a = calloc(1, sizeof(*a));
	if (a == NULL)
		err(1, "calloc");

	a->a_map = m;
	map_find_empty_spot(m, &a->a_x, &a->a_y);
	return (a);
}

unsigned int
map_actor_get_x(struct actor *a)
{

	return (a->a_x);
}

unsigned int
map_actor_get_y(struct actor *a)
{

	return (a->a_y);
}

int
map_actor_move_by(struct actor *a, int dx, int dy)
{
	char c;

	c = map_get(a->a_map, a->a_x + dx, a->a_y + dy);
	assert(c != '\0');
	if (c != ' ')
		return (1);

	a->a_x += dx;
	a->a_y += dy;

	return (0);
}
