#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <err.h>
#include <stdarg.h>
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
};

struct remote *
remote_new(int fd)
{
	struct remote *r;
	int error, flag;

	r = calloc(1, sizeof(*r));
	if (r == NULL)
		err(1, "calloc");

	r->r_buf_size = 1024;
	r->r_buf = calloc(1, r->r_buf_size);
	if (r->r_buf == NULL)
		err(1, "calloc");
	r->r_fd = fd;

	flag = 1;
	error = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)); 
	if (error != 0)
		err(1, "TCP_NODELAY");

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
remote_send(struct remote *r, const char *fmt, ...)
{
	va_list args;
	char *msg;
	int msglen;
	ssize_t len;

	/*
	 * XXX: Make it nonblocking.
	 */
	va_start(args, fmt);
	msglen = vasprintf(&msg, fmt, args);
	va_end(args);
	if (msglen <= 0)
		err(1, "vasprintf");
	len = write(r->r_fd, msg, msglen + 1);
	if (len < 0)
		warn("write");
	free(msg);
}

char *
remote_receive_internal(struct remote *r)
{
	ssize_t len;
	int bytes, error, i;

	/*
	 * Discard data returned the previous time.
	 */
	if (r->r_returned > 0) {
#if 0
		fprintf(stderr, "discarding %zd bytes, leaving %ld\n", r->r_returned, r->r_buffered - r->r_returned);
#endif
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
			return (NULL);
		}

#if 0
		fprintf(stderr, "receiving %d bytes\n", bytes);
#endif
		len = read(r->r_fd, r->r_buf + r->r_buffered, bytes);
		if (len != bytes)
			err(1, "short read\n");
		r->r_buffered += len;
	}

	/*
	 * Look for a newline.
	 */
	for (i = 0; i < r->r_buffered; i++) {
		if (r->r_buf[i] != '\n' && r->r_buf[i] != '\r' && r->r_buf[i] != '\0')
			continue;

		/*
		 * Found a newline.  Terminate the string and return it.
		 */
		r->r_buf[i] = '\0';
		r->r_returned = i + 1; /* +1, because i is an offset, and r_returned is a counter. */
#if 0
		fprintf(stderr, "returning '%s', %zd bytes\n", r->r_buf, r->r_returned);
#endif
		return (r->r_buf);
	}

	/*
	 * No newline, thus no command to be returned.
	 */
#if 0
	fprintf(stderr, "no newline, returning NULL\n");
#endif
	return (NULL);
}

char *
remote_receive(struct remote *r)
{
	char buf[1], *str;

	for (;;) {
		/*
		 * Check if the socket is still connected and wait for some data.
		 */
		if (recv(r->r_fd, buf, sizeof(buf), MSG_WAITALL | MSG_PEEK) <= 0)
			return (NULL);

		/*
		 * Skip empty commands.
		 */
		for (;;) {
			str = remote_receive_internal(r);
			if (str == NULL)
				break;
			if (str[0] == '\0')
				continue;
			break;
		}
		if (str != NULL)
			return (str);
	}
}

char *
remote_receive_async(struct remote *r)
{
	char *str;

	for (;;) {
		str = remote_receive_internal(r);
		if (str == NULL)
			return (str);
		if (str[0] == '\0')
			continue;
		return (str);
	}
}
