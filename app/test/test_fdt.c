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
#include <stdio.h>

#include "test.h"

#include <rte_fdt.h>

static int test_fdt_path_is_valid(void)
{
	TEST_ASSERT(rte_fdt_path_is_valid("name"), "'name' must be valid");
	TEST_ASSERT(rte_fdt_path_is_valid("comp@00ffabcd"),
			"'comp@00ffabcd' must be valid");
	TEST_ASSERT(rte_fdt_path_is_valid("#address-cells"),
			"'#address-cells' must be valid");
	TEST_ASSERT(rte_fdt_path_is_valid("#size-cells"),
			"'#size-cells' must be valid");
	TEST_ASSERT(!rte_fdt_path_is_valid(NULL), "NULL must not be valid");
	TEST_ASSERT(!rte_fdt_path_is_valid("."), "'.' must not be valid");
	TEST_ASSERT(!rte_fdt_path_is_valid(".."), "'..' must not be valid");
	TEST_ASSERT(!rte_fdt_path_is_valid("/"), "'/' must not be valid");
	TEST_ASSERT(!rte_fdt_path_is_valid("/name"),
			"'/name' must not be valid");
	TEST_ASSERT(!rte_fdt_path_is_valid("base/name"),
			"'base/name' must not be valid");
	TEST_ASSERT(!rte_fdt_path_is_valid("base/"),
			"'base/' must not be valid");
	return 0;
}

static int test_fdt_path_push_pop(void)
{
	struct rte_fdt_path *path = NULL;

	path = rte_fdt_path_pushs(path, "amba");
	TEST_ASSERT_NOT_NULL(path, "path must not be NULL");
	TEST_ASSERT(!strcmp(path->name, "amba"), "name must be 'amba'");
	TEST_ASSERT_NULL(path->base, "base must be NULL");
	TEST_ASSERT_NULL(path->top, "top must be NULL");

	path = rte_fdt_path_pushs(path, "ethernet@ffc00000");
	TEST_ASSERT_NOT_NULL(path, "path must not be NULL");
	TEST_ASSERT(!strcmp(path->name, "ethernet@ffc00000"),
			"name must be 'ethernet@ffc00000'");
	TEST_ASSERT_NOT_NULL(path->base, "base must not be NULL");
	TEST_ASSERT_NULL(path->top, "top must be NULL");
	TEST_ASSERT_EQUAL(path->base->top, path,
			"base must point to current top");

	path = rte_fdt_path_pushs(path, "name");
	TEST_ASSERT_NOT_NULL(path, "path must not be NULL");
	TEST_ASSERT(!strcmp(path->name, "name"),
			"name must be 'name'");
	TEST_ASSERT_NOT_NULL(path->base, "base must not be NULL");
	TEST_ASSERT_NULL(path->top, "top must be NULL");
	TEST_ASSERT_EQUAL(path->base->top, path,
			"base must point to current top");

	path = rte_fdt_path_pop(path);
	TEST_ASSERT_NOT_NULL(path, "path must not be NULL");
	TEST_ASSERT(!strcmp(path->name, "ethernet@ffc00000"),
			"name must be 'ethernet@ffc00000'");
	TEST_ASSERT_NULL(path->top, "top must be NULL");

	path = rte_fdt_path_pushs(path, "compatible");
	TEST_ASSERT_NOT_NULL(path, "path must not be NULL");
	TEST_ASSERT(!strcmp(path->name, "compatible"),
			"name must be 'compatible'");
	TEST_ASSERT_NOT_NULL(path->base, "base must not be NULL");
	TEST_ASSERT_NULL(path->top, "top must be NULL");
	TEST_ASSERT_EQUAL(path->base->top, path,
			"base must point to current top");

	path = rte_fdt_path_pop(path);
	TEST_ASSERT_NOT_NULL(path, "path must not be NULL");
	TEST_ASSERT(!strcmp(path->name, "ethernet@ffc00000"),
			"name must be 'ethernet@ffc00000'");
	TEST_ASSERT_NULL(path->top, "top must be NULL");

	path = rte_fdt_path_pop(path);
	TEST_ASSERT_NOT_NULL(path, "path must not be NULL");
	TEST_ASSERT(!strcmp(path->name, "amba"),
			"name must be 'amba'");
	TEST_ASSERT_NULL(path->top, "top must be NULL");

	path = rte_fdt_path_pop(path);
	TEST_ASSERT_NULL(path, "path must be NULL");

	return 0;
}

static int fdt_path_equal(const struct rte_fdt_path *a,
		const struct rte_fdt_path *b)
{
	while (a != NULL && b != NULL) {
		if (strcmp(a->name, b->name))
			return 0;

		a = a->base;
		b = b->base;
	}

	if (a != b) /* both must be NULL */
		return 0;

	return 1;
}

static int test_fdt_path_parse(void)
{
	const char *p0  = "/";
	const char *p1  = "/amba";
	const char *p2  = "/amba/ethernet@ffc00000";
	const char *p3  = "/amba/ethernet@ffc00000/compatible";
	const char *p4  = "/amba/./xxx";
	const char *p5  = "/amba/./xxx/.";
	const char *p6  = "/amba/../xxx/..";
	const char *p7  = "..";
	const char *p8  = "/..";
	const char *p9  = "";
	const char *p10 = NULL;

	struct rte_fdt_path *test = NULL;
	struct rte_fdt_path *tmp;

	// parse p0

	TEST_ASSERT_EQUAL(rte_fdt_path_parse(&tmp, p0), 0,
			"failed to parse p0");
	TEST_ASSERT_NULL(tmp, "tmp must be NULL");

	// parse p1

	test = rte_fdt_path_pushs(test, "amba");
	TEST_ASSERT_NOT_NULL(test, "push failed for 'amba'");
	TEST_ASSERT_EQUAL(rte_fdt_path_parse(&tmp, p1), 0,
			"failed to parse p1");
	TEST_ASSERT_NOT_NULL(tmp, "tmp must not be NULL");
	TEST_ASSERT(fdt_path_equal(test, tmp),
			"parsed p1 does not match the constructed path");
	tmp = rte_fdt_path_free(tmp);

	// parse p2

	test = rte_fdt_path_pushs(test, "ethernet@ffc00000");
	TEST_ASSERT_NOT_NULL(test, "push failed for 'ethernet@ffc00000'");
	TEST_ASSERT_EQUAL(rte_fdt_path_parse(&tmp, p2), 0,
			"failed to parse p2");
	TEST_ASSERT_NOT_NULL(tmp, "tmp must not be NULL");
	TEST_ASSERT(fdt_path_equal(test, tmp),
			"parsed p2 does not match the constructed path");
	tmp = rte_fdt_path_free(tmp);

	// parse p3

	test = rte_fdt_path_pushs(test, "compatible");
	TEST_ASSERT_NOT_NULL(test, "push failed for 'compatible'");
	TEST_ASSERT_EQUAL(rte_fdt_path_parse(&tmp, p3), 0,
			"failed to parse p3");
	TEST_ASSERT_NOT_NULL(tmp, "tmp must not be NULL");
	TEST_ASSERT(fdt_path_equal(test, tmp),
			"parsed p3 does not match the constructed path");
	tmp = rte_fdt_path_free(tmp);

	// parse p4

	test = rte_fdt_path_pop(test); /* pop compatible */
	test = rte_fdt_path_pop(test); /* pop ethernet@ffc00000 */

	test = rte_fdt_path_pushs(test, "xxx");
	TEST_ASSERT_NOT_NULL(test, "push failed for 'xxx'");
	TEST_ASSERT_EQUAL(rte_fdt_path_parse(&tmp, p4), 0,
			"failed to parse p4");
	TEST_ASSERT_NOT_NULL(tmp, "tmp must not be NULL");
	TEST_ASSERT(fdt_path_equal(test, tmp),
			"parsed p4 does not match the constructed path");
	tmp = rte_fdt_path_free(tmp);

	// parse p5

	TEST_ASSERT_EQUAL(rte_fdt_path_parse(&tmp, p5), 0,
			"failed to parse p5");
	TEST_ASSERT_NOT_NULL(tmp, "tmp must not be NULL");
	TEST_ASSERT(fdt_path_equal(test, tmp),
			"parsed p5 does not match the constructed path");
	tmp = rte_fdt_path_free(tmp);

	// parse p6

	test = rte_fdt_path_free(test);

	TEST_ASSERT_EQUAL(rte_fdt_path_parse(&tmp, p6), 0,
			"failed to parse p6");
	TEST_ASSERT_NULL(tmp, "tmp must be NULL");

	// parse p7

	TEST_ASSERT_NOT_EQUAL(rte_fdt_path_parse(&tmp, p7), 0,
			"parse p7 must fail");
	// parse p8

	TEST_ASSERT_EQUAL(rte_fdt_path_parse(&tmp, p8), 0,
			"failed to parse p8");
	TEST_ASSERT_NULL(tmp, "tmp must be NULL");

	// parse p9

	TEST_ASSERT_NOT_EQUAL(rte_fdt_path_parse(&tmp, p9), 0,
			"parse p9 must fail");

	// parse p10

	TEST_ASSERT_NOT_EQUAL(rte_fdt_path_parse(&tmp, p10), 0,
			"parse p10 must fail");

	return 0;
}

static int test_fdt(void)
{
	if (test_fdt_path_is_valid())
		return -1;

	if (test_fdt_path_push_pop())
		return -1;

	if (test_fdt_path_parse())
		return -1;

	return 0;
}

static struct test_command fdt_cmd = {
	.command = "fdt_autotest",
	.callback = test_fdt,
};
REGISTER_TEST_COMMAND(fdt_cmd);
