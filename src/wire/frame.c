/* SPDX-License-Identifier: Apache-2.0 */
#include "wire/frame.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_full(int fd, void *buf, size_t n)
{
	uint8_t *p = buf;
	size_t got = 0;

	while (got < n) {
		ssize_t r = read(fd, p + got, n - got);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			return -errno;
		}
		if (r == 0)
			return -ECONNRESET;
		got += (size_t)r;
	}
	return 0;
}

static int write_full(int fd, const void *buf, size_t n)
{
	const uint8_t *p = buf;
	size_t put = 0;

	while (put < n) {
		ssize_t w = write(fd, p + put, n - put);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			return -errno;
		}
		if (w == 0)
			return -EIO;
		put += (size_t)w;
	}
	return 0;
}

int gkpp_read_frame(int fd, uint8_t **body, size_t *body_len)
{
	uint8_t hdr[GKPP_HEADER_LEN];
	uint32_t blen;
	uint8_t *buf;
	int rc;

	if (!body || !body_len)
		return -EINVAL;
	*body = NULL;
	*body_len = 0;

	rc = read_full(fd, hdr, sizeof(hdr));
	if (rc != 0)
		return rc;
	if (memcmp(hdr, GKPP_MAGIC, 4) != 0)
		return -EPROTO;
	if (hdr[4] != GKPP_FRAME_VERSION)
		return -EPROTO;
	if (hdr[5] != 0)
		return -EPROTO; /* packed not implemented */
	if (hdr[6] != 0 || hdr[7] != 0)
		return -EPROTO;

	blen = (uint32_t)hdr[8] | ((uint32_t)hdr[9] << 8) |
	       ((uint32_t)hdr[10] << 16) | ((uint32_t)hdr[11] << 24);
	if (blen == 0 || blen > GKPP_MAX_BODY)
		return -EPROTO;

	buf = malloc(blen);
	if (!buf)
		return -ENOMEM;
	rc = read_full(fd, buf, blen);
	if (rc != 0) {
		free(buf);
		return rc;
	}
	*body = buf;
	*body_len = blen;
	return 0;
}

int gkpp_write_frame(int fd, const uint8_t *body, size_t body_len)
{
	uint8_t hdr[GKPP_HEADER_LEN];
	uint32_t blen;
	int rc;

	if (!body || body_len == 0 || body_len > GKPP_MAX_BODY)
		return -EINVAL;
	blen = (uint32_t)body_len;
	memcpy(hdr, GKPP_MAGIC, 4);
	hdr[4] = GKPP_FRAME_VERSION;
	hdr[5] = 0;
	hdr[6] = 0;
	hdr[7] = 0;
	hdr[8] = (uint8_t)(blen & 0xff);
	hdr[9] = (uint8_t)((blen >> 8) & 0xff);
	hdr[10] = (uint8_t)((blen >> 16) & 0xff);
	hdr[11] = (uint8_t)((blen >> 24) & 0xff);

	rc = write_full(fd, hdr, sizeof(hdr));
	if (rc != 0)
		return rc;
	return write_full(fd, body, body_len);
}
