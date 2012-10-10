#include <unistd.h>

#include "wieland.h"

int
main(void)
{
	struct w_window *root, *w;

	root = w_init();
#if 0
	w = w_window_new(root);
	w_window_resize(w, 10, 10);
	w_window_move(w, 10, 10);
	w_window_putstr(w, 0, 0, "*** test ***");
#else
	w_window_putstr(root, 3, 3, "*** test ***");
#endif
	w_redraw(root);
	sleep(3);
	w_fini(root);
	return (0);
}
