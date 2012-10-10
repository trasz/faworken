#ifndef WIELAND_H
#define	WIELAND_H

struct w_window;

struct w_window	*w_init(void);
void		w_fini(struct w_window *w);

struct w_window	*w_window_new(struct w_window *parent);
void		w_window_delete(struct w_window *w);

void		w_window_move(struct w_window *w, int x, int y);
void		w_window_resize(struct w_window *w, unsigned int width, unsigned int height);

void		w_window_clear(struct w_window *w);
void		w_window_putstr(struct w_window *w, int x, int y, const char *str);
void		w_window_set_translucent_char(struct w_window *w, char c);

void		w_bind(struct w_window *w, const char *key, void (*callback)(struct w_window *w, const char *key));
void		w_redraw(struct w_window *w);
int		w_get_input_fd(struct w_window *w);

#endif /* !WIELAND_H */
