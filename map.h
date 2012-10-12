#ifndef MAP_H
#define	MAP_H

struct map;

struct map	*map_make(struct w_window *w);
void		map_find_empty_spot(struct map *m, unsigned int *x, unsigned int *y);

#endif /* !MAP_H */
