#include <curses.h>
#include <err.h>
#include <string.h>
#include <unistd.h>

#include "wieland.h"

static void
move_callback(struct w_window *w, int key)
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

int
main(void)
{
	struct w_window *root, *w;

	root = w_init();
	w = w_window_new(root);
	w_window_resize(w, 20, 3);
	w_window_move(w, 10, 10);
	w_window_putstr(w, 0, 0, "*** test ***");
	w_window_bind(w, KEY_LEFT, move_callback);
	w_window_bind(w, KEY_RIGHT, move_callback);
	w_window_bind(w, KEY_UP, move_callback);
	w_window_bind(w, KEY_DOWN, move_callback);
	for (;;) {
		w_redraw(root);
		w_wait_for_key(w);
	}
	w_fini(root);
	return (0);
}
