#include <sys/queue.h>
#include <assert.h>
#include <err.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "window.h"

struct w_binding {
	TAILQ_ENTRY(w_binding)	wb_next;
	int			wb_key;
	void			(*wb_callback)(struct window *w, int key);
};

struct window {
	TAILQ_ENTRY(window)	w_next;
	struct window		*w_parent;
	int			w_x;
	int			w_y;
	unsigned int		w_w;
	unsigned int		w_h;
	TAILQ_HEAD(, window)	w_windows;
	TAILQ_HEAD(, w_binding)	w_bindings;
	char			*w_data;
	struct window		*w_window_with_cursor;
	int			w_cursor_x;
	int			w_cursor_y;
	void			*w_uptr;

	char			*w_frame_title;
};

static void	window_redraw_frame(struct window *w);

struct window	*
window_init(void)
{
	struct window *root;
	int maxx, maxy, error;
	WINDOW *tmpwin;

	root = window_new(NULL);
	tmpwin = initscr();
	if (tmpwin == NULL)
		err(1, "initscr");

	getmaxyx(stdscr, maxy, maxx);
	if (maxx < 1 || maxy < 1)
		errx(1, "getmaxyx returned nonsense values");
	window_resize(root, maxx - 1, maxy - 1);

	error = noecho();
	if (error == ERR)
		errx(1, "noecho");

	error = keypad(stdscr, TRUE);
	if (error == ERR)
		errx(1, "keypad");

	return (root);
}

void
window_fini(struct window *w)
{
	int error;

	error = endwin();
	if (error == ERR)
		warn("endwin");

	window_delete(w);
}

struct window	*
window_new(struct window *parent)
{
	struct window *w;

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
window_delete(struct window *w)
{
	struct window *child, *tmpchild;
	struct w_binding *wb, *tmpwb;

	TAILQ_FOREACH_SAFE(child, &w->w_windows, w_next, tmpchild)
		window_delete(child);
	TAILQ_FOREACH_SAFE(wb, &w->w_bindings, wb_next, tmpwb)
		free(wb);

	free(w->w_frame_title);
	free(w);
}

static bool
window_is_framed(const struct window *w)
{
	if (w->w_parent != NULL && w->w_parent->w_frame_title != NULL)
		return (true);
	return (false);
}

void
window_move(struct window *w, int x, int y)
{
	if (window_is_framed(w)) {
		window_move(w->w_parent, x, y);
	} else {
		w->w_x = x;
		w->w_y = y;
	}
}

void
window_move_by(struct window *w, int x, int y)
{
	if (window_is_framed(w)) {
		window_move_by(w->w_parent, x, y);
	} else {
		w->w_x += x;
		w->w_y += y;
	}
}

void
window_resize(struct window *w, unsigned int width, unsigned int height)
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
	window_clear(w);

	if (window_is_framed(w)) {
		window_resize(w->w_parent, width + 2, height + 2);
		window_redraw_frame(w->w_parent);
	}
}

void
window_move_cursor(struct window *w, int x, int y)
{
	struct window *root;

	for (root = w; root->w_parent != NULL; root = root->w_parent)
		continue;

	root->w_window_with_cursor = w;

	w->w_cursor_x = x;
	w->w_cursor_y = y;
}

void	
window_clear(struct window *w)
{

	memset(w->w_data, ' ', w->w_w * w->w_h);
}

static void
w_data_put(struct window *w, int x, int y, char c)
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
w_data_get(struct window *w, int x, int y)
{

	assert(x >= 0);
	assert(y >= 0);
	assert(x < w->w_w);
	assert(y < w->w_h);

	return (w->w_data[w->w_w * y + x]);
}

void
window_putstr(struct window *w, int x, int y, const char *str)
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

char
window_get(struct window *w, int x, int y)
{

	if (y < 0 || y >= w->w_h)
		return ('\0');

	if (x < 0 || x >= w->w_w)
		return ('\0');

	return (w_data_get(w, x, y));
}

void
window_set_translucent_char(struct window *w, char c)
{
}

static struct w_binding *
window_binding_find(struct window *w, int key)
{
	struct w_binding *wb;

	TAILQ_FOREACH(wb, &w->w_bindings, wb_next) {
		if (wb->wb_key == key)
			return (wb);
	}

	return (NULL);
}

void
window_bind(struct window *w, int key, void (*callback)(struct window *w, int key))
{
	struct w_binding *wb;

	wb = window_binding_find(w, key);
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
window_draw(struct window *w, int x, int y, char c)
{
	int error;

	if (w != NULL) {
		if (x < 0 || x >= w->w_w)
			return;
		if (y < 0 || y >= w->w_h)
			return;
		window_draw(w->w_parent, w->w_x + x, w->w_y + y, c);
	} else {
		error = mvaddch(y, x, c);
		if (error == ERR)
			err(1, "mvaddch");
	}
}

static void
window_draw_cursor(struct window *w, int x, int y)
{
	int error;

	if (w != NULL) {
		if (x < 0 || x >= w->w_w)
			return;
		if (y < 0 || y >= w->w_h)
			return;
		window_draw_cursor(w->w_parent, w->w_x + x, w->w_y + y);
	} else {
		error = move(y, x);
		if (error == ERR)
			err(1, "mvaddch");
	}
}

static void
window_redraw_internal(struct window *w)
{
	struct window *child;
	int x, y;
	char c;

	for (y = 0; y < w->w_h; y++) {
		for (x = 0; x < w->w_w; x++) {
			c = w_data_get(w, x, y);
#if 0
			if (c == 0)
				continue;
#endif
			window_draw(w->w_parent, w->w_x + x, w->w_y + y, c);
		}
	}

	TAILQ_FOREACH(child, &w->w_windows, w_next)
		window_redraw_internal(child);
}

void
window_redraw(struct window *w)
{
	int error;

	window_redraw_internal(w);

	if (w->w_window_with_cursor != NULL)
		window_draw_cursor(w->w_window_with_cursor, w->w_window_with_cursor->w_cursor_x, w->w_window_with_cursor->w_cursor_y);

	error = refresh();
	if (error == ERR)
		err(1, "refresh");
}

void
window_check_input_fd(struct window *w)
{
	struct w_binding *wb;
	int key;

	key = getch();
	if (key == ERR) {
		/*
		 * No input.
		 */
		return;
	}
	for (; w != NULL; w = w->w_parent) {
		wb = window_binding_find(w, key);
		if (wb != NULL) {
			wb->wb_callback(w, key);
			return;
		}
	}
}

int
window_get_input_fd(struct window *w)
{

	return (fileno(stdin));
}

struct window *
window_get_parent(struct window *w)
{

	return (w->w_parent);
}

struct window *
window_get_root(struct window *w)
{
	struct window *root;

	for (root = w; root->w_parent != NULL; root = root->w_parent)
		continue;

	return (root);
}

int
window_get_x(struct window *w)
{
	if (window_is_framed(w))
		return (window_get_x(w->w_parent));

	return (w->w_x);
}

int
window_get_y(struct window *w)
{
	if (window_is_framed(w))
		return (window_get_y(w->w_parent));

	return (w->w_y);
}

int
window_get_width(struct window *w)
{

	return (w->w_w);
}

int
window_get_height(struct window *w)
{

	return (w->w_h);
}

struct window	*
window_framed_new(struct window *parent, const char *title)
{
	struct window *frame, *w;

	frame = window_new(parent);
	frame->w_frame_title = strdup(title);
	if (frame->w_frame_title == NULL)
		err(1, "strdup");
	w = window_new(frame);
	w->w_x = 1;
	w->w_y = 1;
	return (w);
}

static void
window_redraw_frame(struct window *w)
{
	int x, y;

	assert(w->w_frame_title != NULL);

	if (w->w_h == 0 || w->w_w == 0)
		return;

	/*
	 * Frame.
	 */
	for (x = 1; x < w->w_w - 1; x++)
		window_putstr(w, x, 0, "-");
	for (y = 1; y < w->w_w - 1; y++) {
		window_putstr(w, 0, y, "|");
		window_putstr(w, w->w_w - 1, y, "|");
	}
	for (x = 1; x < w->w_w - 1; x++)
		window_putstr(w, x, w->w_h - 1, "-");
	window_putstr(w, 0, 0, "+");
	window_putstr(w, w->w_w - 1, 0, "+");
	window_putstr(w, w->w_w - 1, w->w_h - 1, "+");
	window_putstr(w, 0, w->w_h - 1, "+");

	/*
	 * Title.
	 */
	window_putstr(w, 1, 0, "[ ");
	window_putstr(w, 1 + strlen("[ "), 0, w->w_frame_title);
	window_putstr(w, 1 + strlen("[ ") + strlen(w->w_frame_title), 0, " ]");
}

void
window_set_uptr(struct window *w, void *uptr)
{

	w->w_uptr = uptr;
}

void *
window_uptr(struct window *w)
{

	return (w->w_uptr);
}
