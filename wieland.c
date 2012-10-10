#include <sys/queue.h>
#include <assert.h>
#include <err.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wieland.h"

struct w_window {
	TAILQ_ENTRY(w_window)	w_next;
	struct w_window		*w_parent;
	int			w_x;
	int			w_y;
	unsigned int		w_w;
	unsigned int		w_h;
	TAILQ_HEAD(, w_window)	w_windows;
	WINDOW			*w_curseswin;
	char			*w_data;
};

struct w_window	*
w_init(void)
{
	struct w_window *root;
	int maxx, maxy;

	root = w_window_new(NULL);
	root->w_curseswin = initscr();
	if (root->w_curseswin == NULL)
		err(1, "initscr");

	getmaxyx(root->w_curseswin, maxy, maxx);
	if (maxx < 1 || maxy < 1)
		errx(1, "getmaxyx returned nonsense values");
	w_window_resize(root, maxx - 1, maxy - 1);

	return (root);
}

void
w_fini(struct w_window *w)
{
	int error;

	error = endwin();
	if (error == ERR)
		warn("endwin");

	w_window_delete(w);
}

struct w_window	*
w_window_new(struct w_window *parent)
{
	struct w_window *w;

	w = calloc(1, sizeof(*w));
	if (w == NULL)
		err(1, "calloc");
	TAILQ_INIT(&w->w_windows);

	if (parent != NULL) {
		w->w_parent = parent;
		TAILQ_INSERT_TAIL(&parent->w_windows, w, w_next);
	}

	return (w);
}

void
w_window_delete(struct w_window *w)
{
	struct w_window *child, *tmpchild;

	TAILQ_FOREACH_SAFE(child, &w->w_windows, w_next, tmpchild)
		w_window_delete(child);

	free(w);
}

void
w_window_move(struct w_window *w, int x, int y)
{

	w->w_x = x;
	w->w_y = y;
}

void
w_window_resize(struct w_window *w, unsigned int width, unsigned int height)
{

	w->w_w = width;
	w->w_h = height;

#if 0
	fprintf(stderr, "resizing to <%d, %d>\n", width, height);
#endif

	free(w->w_data);
	w->w_data = calloc(1, w->w_w * w->w_h);
	if (w->w_data == NULL)
		err(1, "calloc");
	w_window_clear(w);
}

void	
w_window_clear(struct w_window *w)
{

	memset(w->w_data, ' ', w->w_w * w->w_h);
}

static void
w_data_put(struct w_window *w, int x, int y, char c)
{

	assert(x >= 0);
	assert(y >= 0);
	assert(x < w->w_w);
	assert(y < w->w_h);

#if 0
	fprintf(stderr, "putting '%c' at <%d, %d\n", c, x, y);
#endif

	w->w_data[w->w_w * y + x] = c;
}

static char
w_data_get(struct w_window *w, int x, int y)
{

	assert(x >= 0);
	assert(y >= 0);
	assert(x < w->w_w);
	assert(y < w->w_h);

	return (w->w_data[w->w_w * y + x]);
}

void
w_window_putstr(struct w_window *w, int x, int y, const char *str)
{
	int i, len;

	if (y < 0 || y >= w->w_h) {
#if 0
		fprintf(stderr, "skipping line at %d, not in <%d, %d>\n", y, 0, w->w_h);
#endif
		return;
	}

	len = strlen(str);
	for (i = 0; i < len; i++) {
		if (i + x < 0 || i + x >= w->w_w) {
#if 0
			fprintf(stderr, "skipping '%c'@<%d, %d>\n", str[i], i + x, y);
#endif
			continue;
		}
		w_data_put(w, x + i, y, str[i]);
	}
}

void
w_window_set_translucent_char(struct w_window *w, char c)
{
}

void
w_bind(struct w_window *w, const char *key, void (*callback)(struct w_window *w, const char *key))
{
}

static void
w_window_putscr(struct w_window *w, int x, int y, char c)
{
	int error;

#if 0
	fprintf(stderr, "'%c'@<%d, %d> ", c, x, y);
#endif

	if (w != NULL) {
		if (x < 0 || x >= w->w_w)
			return;
		if (y < 0 || y >= w->w_h)
			return;
		w_window_putscr(w->w_parent, w->w_x + x, w->w_y + y, c);
	} else {
		error = mvaddch(y, x, c);
		if (error == ERR)
			err(1, "mvaddch");
	}
}

static void
w_window_redraw(struct w_window *w)
{
	struct w_window *child;
	int x, y;
	char c;

	for (y = 0; y < w->w_h; y++) {
		for (x = 0; x < w->w_w; x++) {
			c = w_data_get(w, x, y);
#if 0
			if (c == 0)
				continue;
#endif
			w_window_putscr(w->w_parent, w->w_x + x, w->w_y + y, c);
		}
	}

	TAILQ_FOREACH(child, &w->w_windows, w_next)
		w_window_redraw(child);
}

void
w_redraw(struct w_window *w)
{
	int error;

	w_window_redraw(w);

	error = refresh();
	if (error == ERR)
		err(1, "refresh");
}

int
w_get_input_fd(struct w_window *w)
{

	return (42);
}

