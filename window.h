#ifndef WINDOW_H
#define	WINDOW_H

struct window;

struct window	*window_init(void);
void		window_fini(struct window *w);
void		window_redraw(struct window *w);
void		window_check_input_fd(struct window *w);
int		window_get_input_fd(struct window *w);

struct window	*window_new(struct window *parent);
void		window_delete(struct window *w);

void		window_move(struct window *w, int x, int y);
void		window_move_by(struct window *w, int x, int y);
void		window_resize(struct window *w, unsigned int width, unsigned int height);
void		window_move_cursor(struct window *w, int x, int y);

void		window_clear(struct window *w);
void		window_putstr(struct window *w, int x, int y, const char *str);
char		window_get(struct window *w, int x, int y);
void		window_set_translucent_char(struct window *w, char c);

void		window_bind(struct window *w, int key, void (*callback)(struct window *w, int key));

struct window	*window_get_parent(struct window *w);
struct window	*window_get_root(struct window *w);
int		window_get_x(struct window *w);
int		window_get_y(struct window *w);
int		window_get_width(struct window *w);
int		window_get_height(struct window *w);

struct window	*window_framed_new(struct window *parent, const char *title);

void		window_set_uptr(struct window *w, void *uptr);
void		*window_uptr(struct window *w);

#endif /* !WINDOW_H */
