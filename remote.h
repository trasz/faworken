#ifndef REMOTE_H
#define	REMOTE_H

#include <stdbool.h>

struct remote;

struct remote	*remote_new(int fd);
void		remote_delete(struct remote *r);
char		*remote_receive(struct remote *r);
void		remote_send(struct remote *r, const char *str);
bool		remote_disconnected(struct remote *r);

#endif /* !REMOTE_H */
