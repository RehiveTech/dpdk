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

#ifndef _RTE_FDT_H_
#define _RTE_FDT_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

/**
 * @file
 *
 * Flat Device Tree access
 *
 * Common API to access FDT. Multiple FDT backends can be implemented for
 * different platforms (Linux, BSD).
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Representation of an FDT.
 */
struct rte_fdt;

/**
 * Open the FDT specified by the given path. The path is platform dependent.
 * For Linux, the path is usually "/proc/device-tree".
 */
struct rte_fdt *rte_fdt_open(const char *path);

/**
 * Release all resources used by the given FDT.
 */
void rte_fdt_close(struct rte_fdt *fdt);

/**
 * Representation of a path in the FDT.
 * It is a linked-list of path components.
 *
 * "amba" <- "eth0" <- "name"
 * |----- base -----|- top -|
 */
struct rte_fdt_path {
	char *name;
	struct rte_fdt_path *top;  /**< points to the front to "my top" */
	struct rte_fdt_path *base; /**< pointes to the back to "my base" */
};

/**
 * Returns true if the given name is a valid name to be the rte_fdt_path
 * component. A path component must not contain '/', '\\'.
 * It MUST NOT be neither of: '.', '..', '' (empty).
 */
bool rte_fdt_path_is_valid(const char *name);

/**
 * Push a new component at the top of the path. Return the new top.
 * The base can be NULL if there is no base yet.
 * It returns NULL on allocation failure (the base pointer remains valid).
 */
struct rte_fdt_path *rte_fdt_path_pushs(struct rte_fdt_path *base,
		const char *top);

/**
 * Drop the top of the path. Returns the base. If there is no base,
 * it returns NULL.
 */
struct rte_fdt_path *rte_fdt_path_pop(struct rte_fdt_path *path);

/**
 * Duplicate the given path. Useful to preserve a deep copy of a path.
 */
struct rte_fdt_path *rte_fdt_path_dup(const struct rte_fdt_path *path);

/**
 * Free the given path. Returns NULL.
 */
struct rte_fdt_path *rte_fdt_path_free(struct rte_fdt_path *path);

/**
 * Parse the given path into the rte_fdt_path representation.
 */
int rte_fdt_path_parse(struct rte_fdt_path **p, const char *path);

/**
 * Construct the string representation of the given base and top using the
 * delimiter '/'. The top can be omitted by giving NULL. The returned string
 * should be freed by the standard free().
 */
char *rte_fdt_path_tostr(const struct rte_fdt_path *base, const char *top);

/**
 * Read raw contents of the given path.
 * Returns the length of the read data (excluding the appended NUL).
 */
ssize_t rte_fdt_path_read(struct rte_fdt *fdt,
		const struct rte_fdt_path *base,
		const char *top, char *b, size_t blen);

/**
 * Read up to vmax values, each 4 bytes long. Returns the number of read
 * values or a negative in case of an error. If zero is returned, the path
 * does not exist.
 */
ssize_t rte_fdt_path_read32(struct rte_fdt *fdt,
		const struct rte_fdt_path *base, const char *top,
		uint32_t *v, size_t vmax);

/**
 * Read up to vmax values, each 8 bytes long. Returns the number of read
 * values or a negative in case of an error. If zero is returned, the path
 * does not exist.
 */
ssize_t rte_fdt_path_read64(struct rte_fdt *fdt,
		const struct rte_fdt_path *base, const char *top,
		uint64_t *v, size_t vmax);

/**
 * Read contents of the given path. The result is always NUL terminated.
 * The buffer s is to be freed by the standard free() function.
 * Returns the length of the read data (excluding the appended NUL).
 */
ssize_t rte_fdt_path_reads(struct rte_fdt *fdt,
		const struct rte_fdt_path *base,
		const char *top, char **s);

/**
 * Walk the current base for all available tops. For each top, the function f
 * is called with the base, top and the context as its arguments. Returns
 * * 0 on success,
 * * 1 on success with forced stop,
 * * a positive value (defined by the given f function),
 * * a negative value on a failure (unrelated to the f function).
 *
 * The function f should return:
 *
 * * 0 on success (to continue the walking),
 * * 1 on success (to stop the walking),
 * * any other (positive) value on failure (stops walking and returns
 *   the value).
 */
int rte_fdt_path_walk(struct rte_fdt *fdt, const struct rte_fdt_path *base,
		int (*f)(struct rte_fdt *, const struct rte_fdt_path *,
			const char *, void *), void *context);

#ifdef __cplusplus
}
#endif

#endif
