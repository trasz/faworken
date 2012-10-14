#include <sys/ioctl.h>
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct remote {
	int			r_fd;
	size_t			r_returned;
	size_t			r_buffered;
	size_t			r_buf_size;
	char			*r_buf;
	bool			r_disconnected;
};

struct remote *
remote_new(int fd)
{
	struct remote *r;

	r = calloc(1, sizeof(*r));
	if (r == NULL)
		err(1, "calloc");

	r->r_buf_size = 1024;
	r->r_buf = calloc(1, r->r_buf_size);
	if (r->r_buf == NULL)
		err(1, "calloc");
	r->r_fd = fd;

	return (r);
}

void
remote_delete(struct remote *r)
{

	close(r->r_fd);
	free(r->r_buf);
	free(r);
}

void
remote_send(struct remote *r, const char *msg)
{
	ssize_t len;

	/*
	 * XXX: Make it nonblocking.
	 */

	len = write(r->r_fd, msg, strlen(msg) + 1);
	if (len < 0) {
		warn("write");
		r->r_disconnected = true;
	}
}

char *
remote_receive(struct remote *r)
{
	ssize_t len;
	int bytes, error, i;

	/*
	 * Discard data returned the previous time.
	 */
	if (r->r_returned > 0) {
		memmove(r->r_buf, r->r_buf + r->r_returned, r->r_buffered - r->r_returned);
		r->r_buffered -= r->r_returned;
		r->r_returned = 0;
	}

	/*
	 * Receive as much as we can without blocking.
	 */
	error = ioctl(r->r_fd, FIONREAD, &bytes);
	if (error != 0 || bytes < 0)
		err(1, "FIONREAD");

	if (bytes > 0) {
		if (bytes > r->r_buf_size - r->r_buffered)
			bytes = r->r_buf_size - r->r_buffered;
		if (bytes <= 0) {
			warnx("client overflow\n");
			r->r_disconnected = true;
			return (NULL);
		}

		len = read(r->r_fd, r->r_buf + r->r_buffered, bytes);
		if (len != bytes)
			err(1, "short read\n");
		r->r_buffered += len;
	}

	/*
	 * Look for a newline.
	 */
	for (i = 0; i < r->r_buffered; i++) {
		if (r->r_buf[i] != '\n' && r->r_buf[i] != '\r')
			continue;

		/*
		 * Found a newline.  Terminate the string and return it.
		 */
		r->r_buf[i] = '\0';
		r->r_returned = i + 1; /* +1, because i is an offset, and r_returned is a counter. */
		return (r->r_buf);
	}

	/*
	 * No newline, thus no command to be returned.
	 */
	return (NULL);
}

bool
remote_disconnected(struct remote *r)
{

	return (r->r_disconnected);
}
