#ifndef WIELAND_H
#define	WIELAND_H

struct w_window;

struct w_window	*w_init(void);
void		w_fini(struct w_window *w);
void		w_redraw(struct w_window *w);
void		w_wait_for_key(struct w_window *w);
int		w_get_input_fd(struct w_window *w);

struct w_window	*w_window_new(struct w_window *parent);
void		w_window_delete(struct w_window *w);

void		w_window_move(struct w_window *w, int x, int y);
void		w_window_move_by(struct w_window *w, int x, int y);
void		w_window_resize(struct w_window *w, unsigned int width, unsigned int height);
void		w_window_move_cursor(struct w_window *w, int x, int y);

void		w_window_clear(struct w_window *w);
void		w_window_putstr(struct w_window *w, int x, int y, const char *str);
char		w_window_get(struct w_window *w, int x, int y);
void		w_window_set_translucent_char(struct w_window *w, char c);

void		w_window_bind(struct w_window *w, int key, void (*callback)(struct w_window *w, int key));

struct w_window	*w_window_get_parent(struct w_window *w);
struct w_window	*w_window_get_root(struct w_window *w);
int		w_window_get_x(struct w_window *w);
int		w_window_get_y(struct w_window *w);
int		w_window_get_width(struct w_window *w);
int		w_window_get_height(struct w_window *w);

struct w_window	*w_window_framed_new(struct w_window *parent, const char *title);

#endif /* !WIELAND_H */
