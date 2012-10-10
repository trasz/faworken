#include <sys/queue.h>
#include <assert.h>
#include <err.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wieland.h"

struct w_binding {
	TAILQ_ENTRY(w_binding)	wb_next;
	int			wb_key;
	void			(*wb_callback)(struct w_window *w, int key);
};

struct w_window {
	TAILQ_ENTRY(w_window)	w_next;
	struct w_window		*w_parent;
	int			w_x;
	int			w_y;
	unsigned int		w_w;
	unsigned int		w_h;
	TAILQ_HEAD(, w_window)	w_windows;
	TAILQ_HEAD(, w_binding)	w_bindings;
	char			*w_data;
	struct w_window		*w_window_with_cursor;
	int			w_cursor_x;
	int			w_cursor_y;
};

struct w_window	*
w_init(void)
{
	struct w_window *root;
	int maxx, maxy, error;
	WINDOW *tmpwin;

	root = w_window_new(NULL);
	tmpwin = initscr();
	if (tmpwin == NULL)
		err(1, "initscr");

	getmaxyx(stdscr, maxy, maxx);
	if (maxx < 1 || maxy < 1)
		errx(1, "getmaxyx returned nonsense values");
	w_window_resize(root, maxx - 1, maxy - 1);

	error = noecho();
	if (error == ERR)
		errx(1, "noecho");

	error = keypad(stdscr, TRUE);
	if (error == ERR)
		errx(1, "keypad");

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
	TAILQ_INIT(&w->w_bindings);

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
	struct w_binding *wb, *tmpwb;

	TAILQ_FOREACH_SAFE(child, &w->w_windows, w_next, tmpchild)
		w_window_delete(child);
	TAILQ_FOREACH_SAFE(wb, &w->w_bindings, wb_next, tmpwb)
		free(wb);

	free(w);
}

void
w_window_move(struct w_window *w, int x, int y)
{

	w->w_x = x;
	w->w_y = y;
}

void
w_window_move_by(struct w_window *w, int x, int y)
{

	w->w_x += x;
	w->w_y += y;
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
w_window_move_cursor(struct w_window *w, int x, int y)
{
	struct w_window *root;

	for (root = w; root->w_parent != NULL; root = root->w_parent)
		continue;

	root->w_window_with_cursor = w;

	w->w_cursor_x = x;
	w->w_cursor_y = y;
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

static struct w_binding *
w_window_binding_find(struct w_window *w, int key)
{
	struct w_binding *wb;

	TAILQ_FOREACH(wb, &w->w_bindings, wb_next) {
		if (wb->wb_key == key)
			return (wb);
	}

	return (NULL);
}

void
w_window_bind(struct w_window *w, int key, void (*callback)(struct w_window *w, int key))
{
	struct w_binding *wb;

	wb = w_window_binding_find(w, key);
	if (wb != NULL) {
		wb->wb_callback = callback;
		return;
	}

	wb = calloc(1, sizeof(*wb));
	if (wb == NULL)
		err(1, "calloc");
	wb->wb_key = key;
	wb->wb_callback = callback;
	TAILQ_INSERT_TAIL(&w->w_bindings, wb, wb_next);
}

static void
w_window_draw(struct w_window *w, int x, int y, char c)
{
	int error;

	if (w != NULL) {
		if (x < 0 || x >= w->w_w)
			return;
		if (y < 0 || y >= w->w_h)
			return;
		w_window_draw(w->w_parent, w->w_x + x, w->w_y + y, c);
	} else {
		error = mvaddch(y, x, c);
		if (error == ERR)
			err(1, "mvaddch");
	}
}

static void
w_window_draw_cursor(struct w_window *w, int x, int y)
{
	int error;

	if (w != NULL) {
		if (x < 0 || x >= w->w_w)
			return;
		if (y < 0 || y >= w->w_h)
			return;
		w_window_draw_cursor(w->w_parent, w->w_x + x, w->w_y + y);
	} else {
		error = move(y, x);
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
			w_window_draw(w->w_parent, w->w_x + x, w->w_y + y, c);
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

	if (w->w_window_with_cursor != NULL)
		w_window_draw_cursor(w->w_window_with_cursor, w->w_window_with_cursor->w_cursor_x, w->w_window_with_cursor->w_cursor_y);

	error = refresh();
	if (error == ERR)
		err(1, "refresh");
}

void
w_wait_for_key(struct w_window *w)
{
	struct w_binding *wb;
	int key;

	key = getch();
	for (; w != NULL; w = w->w_parent) {
		wb = w_window_binding_find(w, key);
		if (wb != NULL) {
			wb->wb_callback(w, key);
			return;
		}
	}
}

int
w_get_input_fd(struct w_window *w)
{

	return (42);
}

struct w_window *
w_window_get_parent(struct w_window *w)
{

	return (w->w_parent);
}

struct w_window *
w_window_get_root(struct w_window *w)
{
	struct w_window *root;

	for (root = w; root->w_parent != NULL; root = root->w_parent)
		continue;

	return (root);
}

int
w_window_get_x(struct w_window *w)
{

	return (w->w_x);
}

int
w_window_get_y(struct w_window *w)
{

	return (w->w_y);
}

int
w_window_get_width(struct w_window *w)
{

	return (w->w_w);
}

int
w_window_get_height(struct w_window *w)
{

	return (w->w_h);
}
