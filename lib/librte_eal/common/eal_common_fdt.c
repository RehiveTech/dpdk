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

#include <string.h>
#include <errno.h>
#include <limits.h>

#include <rte_fdt.h>
#include <rte_debug.h>

bool rte_fdt_path_is_valid(const char *name)
{
	size_t i;
	size_t len = 0;

	if (name == NULL)
		return 0;

	len = strlen(name);
	if (len == 0)
		return 0;

	if (!strcmp(name, "."))
		return 0;
	if (!strcmp(name, ".."))
		return 0;

	for (i = 0; i < len; ++i) {
		if (strchr("/\\", name[i]))
			return 0;
	}

	return 1;
}

struct rte_fdt_path *rte_fdt_path_pushs(struct rte_fdt_path *base,
		const char *top)
{
	struct rte_fdt_path *path;
	size_t toplen;

	RTE_VERIFY(top != NULL);
	RTE_VERIFY(rte_fdt_path_is_valid(top));

	toplen = strlen(top);

	path = malloc(sizeof(*path) + toplen + 1);
	if (path == NULL)
		return NULL;

	path->name = (char *) (path + 1);
	memcpy(path->name, top, toplen);
	path->name[toplen] = '\0';

	path->base = base;
	if (base != NULL)
		base->top = path;

	path->top = NULL;
	return path;
}

struct rte_fdt_path *rte_fdt_path_pop(struct rte_fdt_path *path)
{
	struct rte_fdt_path *base;
	RTE_VERIFY(path != NULL);

	base = path->base;
	free(path);

	if (base != NULL)
		base->top = NULL;

	return base;
}

struct rte_fdt_path *rte_fdt_path_dup(const struct rte_fdt_path *path)
{
	struct rte_fdt_path *copy = NULL;
	struct rte_fdt_path *tmp = NULL;
	const struct rte_fdt_path *cur = path;

	if (cur == NULL)
		return NULL;

	while (cur->base != NULL)
		cur = cur->base;

	/* copy all but the top most path component */
	while (cur != path) {
		tmp = rte_fdt_path_pushs(copy, cur->name);
		if (tmp == NULL) {
			rte_fdt_path_free(copy);
			return NULL;
		}

		copy = tmp;
		cur = cur->top;
	}

	/* copy the top most path component */
	tmp = rte_fdt_path_pushs(copy, path->name);
	if (tmp == NULL) {
		rte_fdt_path_free(copy);
		return NULL;
	}

	copy = tmp;
	return copy;
}

struct rte_fdt_path *rte_fdt_path_free(struct rte_fdt_path *path)
{
	while (path != NULL)
		path = rte_fdt_path_pop(path);

	return NULL;
}

int rte_fdt_path_parse(struct rte_fdt_path **p, const char *path)
{
	const char *cur;
	const char *end;
	size_t pathlen;
	struct rte_fdt_path *base = NULL;
	struct rte_fdt_path *tmp = NULL;
	char name[PATH_MAX];

	if (path == NULL) {
		errno = EINVAL;
		return -1;
	}

	pathlen = strlen(path);
	if (pathlen == 0) {
		errno = EINVAL;
		return -2;
	}

	cur = path;

	if (cur[0] != '/') {
		errno = EINVAL;
		return -3;
	}

	/* root "/" */
	if (cur[1] == '\0') {
		*p = NULL;
		return 0;
	}
	cur += 1;

	do {
		end = strchr(cur, '/');
		if (end == NULL)
			end = path + pathlen;

		if (end - cur == 0)
			break; /* strip the ending '/' */

		RTE_VERIFY(end >= cur);
		RTE_VERIFY(end - cur < PATH_MAX);
		memcpy(name, cur, end - cur);
		name[end - cur] = '\0';

		if (!strcmp(name, "."))
			goto next_cur;

		if (!strcmp(name, "..")) {
			if (base)
				base = rte_fdt_path_pop(base);
			goto next_cur;
		}

		if (!rte_fdt_path_is_valid(name))
			goto name_invalid;

		tmp = rte_fdt_path_pushs(base, name);
		if (tmp == NULL)
			goto push_failed;

		base = tmp;
next_cur:
		if (*end == '\0')
			break;

		cur = end + 1;
	} while(end != '\0');

	*p = base;
	return 0;

name_invalid:
	errno = EINVAL;
	return -4;
push_failed:
	rte_fdt_path_free(base);
	return -5;
}

/**
 * Compute the length of the base using the delimiter '/'. An optional new top
 * can be specified. If the top is NULL, only the base is examined. The NUL
 * character is included.
 */
static size_t fdt_path_length(const struct rte_fdt_path *base, const char *top)
{
	size_t len = 0;

	if (base == NULL && top == NULL)
		return strlen("/") + 1;

	while (base != NULL) {
		RTE_VERIFY(base->name != NULL);
		/* '/' + <name> */
		len += 1 + strlen(base->name);
		base = base->base;
	}

	if (top != NULL) {
		/* '/' + <name> */
		len += 1 + strlen(top);
	}

	return len + 1; /* append NUL */
}

char *rte_fdt_path_tostr(const struct rte_fdt_path *base, const char *top)
{
	const size_t len = fdt_path_length(base, top);
	const struct rte_fdt_path *cur = base;
	char *s;
	char *p;

	if (base == NULL && top == NULL)
		return strdup("/");

	s = malloc(len);
	if (s == NULL)
		return NULL;

	if (base == NULL /* && top != NULL */) {
		memcpy(s + 1, top, len - 2);
		s[0] = '/';
		s[len] = '\0';
		return s;
	}

	/* find the bottom, the root component of the path */
	while (cur->base != NULL)
		cur = cur->base;

	p = s;

	/* copy the base from the bottom into the target string */
	while (cur != NULL) {
		size_t curlen = strlen(cur->name);

		RTE_VERIFY(p - s + curlen + 1 < len);

		p[0] = '/';
		memcpy(p + 1, cur->name, curlen);
		p += curlen + 1;

		if (cur == base)
			break;

		cur = cur->top;
	}

	/* append top if exists */
	if (top != NULL) {
		size_t toplen = strlen(top);

		RTE_VERIFY(p - s + toplen + 1 < len);

		p[0] = '/';
		memcpy(p + 1, top, toplen);
		p += toplen + 1;
	}

	RTE_VERIFY(p - s + 1 == (off_t) len);
	p[0] = '\0';

	return s;
}
