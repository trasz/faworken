#ifndef REMOTE_H
#define	REMOTE_H

#include <stdbool.h>

struct remote;

struct remote	*remote_new(int fd);
void		remote_delete(struct remote *r);
char		*remote_receive(struct remote *r);
char		*remote_receive_async(struct remote *r);
void		remote_send(struct remote *r, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#endif /* !REMOTE_H */
