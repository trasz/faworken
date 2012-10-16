#ifndef REMOTE_H
#define	REMOTE_H

#include <stdbool.h>

struct remote;

struct remote	*remote_new(int fd);
void		remote_delete(struct remote *r);
void		remote_send(struct remote *r, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void		remote_expect(struct remote *r, const char *word, int (*callback)(struct remote *r, char *str, char **uptr), char **uptr);
void		remote_process(struct remote *r);
void		remote_process_sync(struct remote *r);

#endif /* !REMOTE_H */
