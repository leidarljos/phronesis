/* SPDX-License-Identifier: MIT */
/*
 * phronesis-pack-image PACK.janet OUT.jimage
 *
 * Build-time tool: compile one entry pack (with its lib/ helpers) and write
 * the image the library loads in place of compiling the sources.
 */
#include "policy_janet.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	char pack[PATH_MAX];
	char root[PATH_MAX];
	char *slash;

	if (argc != 3) {
		fprintf(stderr, "usage: phronesis-pack-image PACK.janet OUT.jimage\n");
		return 2;
	}
	if (!realpath(argv[1], pack)) {
		perror(argv[1]);
		return 1;
	}
	memcpy(root, pack, sizeof(root));
	slash = strrchr(root, '/');
	if (!slash || slash == root)
		return 1;
	*slash = '\0';
	/* Open the sources under the pack's own directory. */
	if (setenv("PHRONESIS_PACK_ROOT", root, 1) != 0)
		return 1;
	if (phronesis_pack_image_write(pack, argv[2]) != 0) {
		fprintf(stderr, "phronesis-pack-image: %s: no image\n", pack);
		return 1;
	}
	return 0;
}
