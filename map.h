#ifndef MAP_H
#define	MAP_H

struct map;
struct actor;

struct map	*map_new(unsigned int w, unsigned int h);
struct actor	*map_actor_new(struct map *m);
void		map_actor_delete(struct actor *a);
unsigned int	map_get_width(struct map *m);
unsigned int	map_get_height(struct map *m);
char		map_get(struct map *m, unsigned int x, unsigned int y);
void		map_set(struct map *m, unsigned int x, unsigned int y, char c);
unsigned int	map_actor_get_x(struct actor *a);
unsigned int	map_actor_get_y(struct actor *a);
int		map_actor_move_by(struct actor *a, int dx, int dy);

#endif /* !MAP_H */
