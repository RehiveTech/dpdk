/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 RehiveTech. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of RehiveTech nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <rte_debug.h>
#include <rte_log.h>
#include <rte_byteorder.h>
#include <rte_fdt.h>

struct rte_fdt {
	char *path;
};

struct rte_fdt *rte_fdt_open(const char *path)
{
	struct rte_fdt *fdt;
	size_t pathlen;

	if (path == NULL) {
		path = "/proc/device-tree";
		pathlen = strlen("/proc/device-tree");
	} else {
		pathlen = strlen(path);
	}

	fdt = malloc(sizeof(*fdt) + pathlen + 1);
	if (fdt == NULL)
		return NULL;

	fdt->path = (char *) (fdt + 1);
	memcpy(fdt->path, path, pathlen);
	fdt->path[pathlen] = '\0';

	return fdt;
}

void rte_fdt_close(struct rte_fdt *fdt)
{
	RTE_VERIFY(fdt != NULL);

	fdt->path = NULL;
	free(fdt);
}

static int concat_and_abspath(char path[PATH_MAX],
		const char *p1, size_t p1len,
		const char *p2, size_t p2len)
{
	char *tmppath;

	tmppath = malloc(p1len + 1 + p2len + 1);
	if (tmppath == NULL) {
		RTE_LOG(ERR, EAL, "%s(): failed to malloc %zu B\n", __func__,
				p1len + 1 + p2len + 1);
		return -1;
	}

	memcpy(tmppath, p1, p1len);
	tmppath[p1len] = '/';
	memcpy(tmppath + p1len + 1, p2, p2len);
	tmppath[p1len + p2len + 1] = '\0';

	if (realpath(tmppath, path) == NULL) {
		long _e = errno;
		RTE_LOG(ERR, EAL, "%s(): realpath has failed for '%s'\n",
				__func__, tmppath);
		RTE_LOG(ERR, EAL, "reason: '%s'\n", strerror(_e));
		free(tmppath);
		return -2;
	}

	free(tmppath);
	return 0;
}

static int fdt_path_open(struct rte_fdt *fdt, const struct rte_fdt_path *base,
		const char *top)
{
	char *relpath = rte_fdt_path_tostr(base, top);
	char path[PATH_MAX];
	char root[PATH_MAX];
	int fd;

	RTE_VERIFY(fdt != NULL);
	RTE_VERIFY(relpath[0] == '/');

	if (relpath == NULL) {
		RTE_LOG(ERR, EAL, "%s(): failed to convert "
				"base path to string\n", __func__);
		errno = ENOMEM;
		return -1;
	}

	if (concat_and_abspath(path, fdt->path, strlen(fdt->path),
				relpath + 1, strlen(relpath + 1))) {
		RTE_LOG(ERR, EAL, "%s(): failed to derive absolute path from "
				"the root ('%s') and the FDT path ('%s')\n",
				__func__, fdt->path, relpath);
		free(relpath);
		errno = ENOMEM;
		return -2;
	}
	free(relpath); /* not needed anymore */

	/* ensure we have the fdt->path as a real and absolute path */
	if (realpath(fdt->path, root) == NULL) {
		long _e = errno;
		RTE_LOG(ERR, EAL, "%s(): realpath of '%s' has failed",
				__func__, fdt->path);
		errno = _e;
		return -3;
	}

	if (strstr(path, root) != path) {
		/* We are out of the fdt->path */
		RTE_LOG(ERR, EAL, "%s(): attempt to access out "
				"of the root path: '%s'\n", __func__, path);
		errno = EACCES;
		return -4;
	}

	if ((fd = open(path, O_RDONLY)) < 0) {
		RTE_LOG(ERR, EAL, "%s(): failed to open the FDT path '%s'\n",
				__func__, path);
		return -5;
	}

	return fd;
}

static ssize_t read_all(int fd, char *b, size_t bmax)
{
	size_t total = 0;

	while (total < bmax) {
		ssize_t rlen = read(fd, b + total, bmax - total);
		if (rlen < 0)
			return -1;

		total += rlen;
	}

	return total;
}

static ssize_t fdt_path_read(struct rte_fdt *fdt,
		const struct rte_fdt_path *base, const char *top,
		void *b, size_t bmax)
{
	int fd;
	struct stat st;
	size_t goal;
	ssize_t ret;

	if ((fd = fdt_path_open(fdt, base, top)) < 0)
		return -1;

	if (fstat(fd, &st) < 0) {
		close(fd);
		return -2;
	}

	goal = st.st_size;
	ret = read_all(fd, b, bmax > goal? goal : bmax);

	close(fd);
	return ret;
}

static ssize_t fdt_path_readx(struct rte_fdt *fdt,
		const struct rte_fdt_path *base, const char *top,
		void *v, size_t vmax, size_t vsize)
{
	size_t bmax = vmax * vsize;
	ssize_t ret;

	ret = fdt_path_read(fdt, base, top, v, bmax);
	if (ret < 0)
		return -1;

	return ret / vsize;
}

ssize_t rte_fdt_path_read(struct rte_fdt *fdt,
		const struct rte_fdt_path *base,
		const char *top, char *b, size_t blen)
{
	return fdt_path_read(fdt, base, top, b, blen);
}

ssize_t rte_fdt_path_read32(struct rte_fdt *fdt,
		const struct rte_fdt_path *base, const char *top,
		uint32_t *v, size_t vmax)
{
	ssize_t ret;
	ssize_t i;

	ret = fdt_path_readx(fdt, base, top, (void *) v, vmax, sizeof(*v));
	if (ret <= 0)
		return ret;

	for (i = 0; i < ret; ++i)
		v[i] = rte_be_to_cpu_32(v[i]);

	return ret;
}

ssize_t rte_fdt_path_read64(struct rte_fdt *fdt,
		const struct rte_fdt_path *base, const char *top,
		uint64_t *v, size_t vmax)
{
	ssize_t ret;
	ssize_t i;

	ret = fdt_path_readx(fdt, base, top, (void *) v, vmax, sizeof(*v));
	if (ret <= 0)
		return ret;

	for (i = 0; i < ret; ++i)
		v[i] = rte_be_to_cpu_64(v[i]);

	return ret;

}

ssize_t rte_fdt_path_reads(struct rte_fdt *fdt,
		const struct rte_fdt_path *base,
		const char *top, char **s)
{
	int fd;
	struct stat st;
	size_t goal;
	ssize_t ret;
	char *b;

	if ((fd = fdt_path_open(fdt, base, top)) < 0)
		return -1;

	if (fstat(fd, &st) < 0) {
		close(fd);
		return -2;
	}

	goal = st.st_size;

	b = malloc(goal + 1);
	if (b == NULL) {
		close(fd);
		return -3;
	}


	if ((ret = read_all(fd, b, goal)) < 0) {
		free(b);
		close(fd);
		return -4;
	}

	b[goal] = '\0';

	close(fd);
	*s = b;
	return ret;
}

int rte_fdt_path_walk(struct rte_fdt *fdt, const struct rte_fdt_path *base,
		int (*f)(struct rte_fdt *, const struct rte_fdt_path *,
			const char *, void *), void *context)
{
	int fd;
	DIR *dir;
	struct dirent entry;
	struct dirent *cur = NULL;
	int ret = 0;

	RTE_VERIFY(f != NULL);

	if ((fd = fdt_path_open(fdt, base, NULL)) < 0)
		return -1;

	dir = fdopendir(fd);
	if (dir == NULL) {
		close(fd);
		return -2;
	}

	while ((readdir_r(dir, &entry, &cur)) == 0 && cur != NULL) {
		if (!rte_fdt_path_is_valid(cur->d_name))
			continue;

		ret = f(fdt, base, cur->d_name, context);
		if (ret != 0)
			break;
	}

	closedir(dir); /* calls close() */
	return ret;
}
