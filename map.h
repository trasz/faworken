#ifndef MAP_H
#define	MAP_H

struct map;
struct actor;

struct map	*map_make(struct window *w);
struct actor	*map_actor_new(struct map *m);
unsigned int	map_actor_get_x(struct actor *a);
unsigned int	map_actor_get_y(struct actor *a);
int		map_actor_move_by(struct actor *a, int dx, int dy);

#endif /* !MAP_H */
