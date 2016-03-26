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

#include <rte_common.h>
#include <rte_fdt.h>

#ifdef RTE_EXEC_ENV_LINUXAPP
static int test_fdt_open_close(void)
{
	struct rte_fdt *fdt;

	fdt = rte_fdt_open("linux-fdt/xgene1");
	TEST_ASSERT_NOT_NULL(fdt, "failed to open linux-fdt/xgene1");
	rte_fdt_close(fdt);
	return 0;
}
#else
static int test_fdt_open_close(void)
{
	printf("The %s is not implemented for this platform\n", __func__);
	return 0;
}
#endif

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

static int test_fdt_path_read_common(struct rte_fdt *fdt)
{
	struct rte_fdt_path *path = NULL;
	ssize_t len;
	char *str;
	uint32_t u32;

	/* reads string from /model */
	TEST_ASSERT_SUCCESS(rte_fdt_path_parse(&path, "/model"),
			"failed to parse '/model'");
	len = rte_fdt_path_reads(fdt, path, NULL, &str);
	TEST_ASSERT_EQUAL(len, 25,
			"unexpected length (%zd) of '/model'", len);
	TEST_ASSERT_SUCCESS(strcmp(str, "APM X-Gene Mustang board"),
			"unexpected content of '/model': '%s'", str);
	free(str);
	rte_fdt_path_free(path);

	/* reads string from /compatible */
	TEST_ASSERT_SUCCESS(rte_fdt_path_parse(&path, "/compatible"),
			"failed to parse '/compatible'");
	len = rte_fdt_path_reads(fdt, path, NULL, &str);
	TEST_ASSERT_EQUAL(len, 28,
			"unexpected length (%zd) of '/compatible'", len);
	TEST_ASSERT_SUCCESS(strcmp(str, "apm,mustang\0apm,xgene-storm"),
			"unexpected content(1) of '/compatible': '%s'", str);
	TEST_ASSERT_SUCCESS(strcmp(str + 12, "apm,xgene-storm"),
			"unexpected content(2) of '/compatible': '%s'", str);
	free(str);
	rte_fdt_path_free(path);

	/* reads string from /#address-cells */
	TEST_ASSERT_SUCCESS(rte_fdt_path_parse(&path, "/#address-cells"),
			"failed to parse '/#address-cells'");
	len = rte_fdt_path_read32(fdt, path, NULL, &u32, 1);
	TEST_ASSERT_EQUAL(len, 1, "failed to read "
			"'/#address-cells': %zd", len);
	TEST_ASSERT_EQUAL(u32, 2, "unexpected value of "
			"'/#address-cells': %zu", (size_t) u32);
	rte_fdt_path_free(path);

	/* reads string from /#size-cells */
	TEST_ASSERT_SUCCESS(rte_fdt_path_parse(&path, "/#size-cells"),
			"failed to parse '/#size-cells'");
	len = rte_fdt_path_read32(fdt, path, NULL, &u32, 1);
	TEST_ASSERT_EQUAL(len, 1, "failed to read "
			"'/#size-cells': %zd", len);
	TEST_ASSERT_EQUAL(u32, 2, "unexpected value of "
			"'/#size-cells': %zu", (size_t) u32);
	rte_fdt_path_free(path);

	return 0;
}

static int test_fdt_xgene1_ethernet(struct rte_fdt *fdt)
{
	struct rte_fdt_path *base;
	ssize_t len;
	uint64_t reg[6];
	char mac[6];
	ssize_t i;
	const char e17020000_mac[] = {
		0x00, 0x11, 0x3a, 0x8a, 0x5a, 0x78
	};

	TEST_ASSERT_SUCCESS(rte_fdt_path_parse(&base,
				"/soc/ethernet@17020000"),
			"failed to parse '/soc/ethernet@17020000'");
	len = rte_fdt_path_read64(fdt, base, "reg", reg, 6);
	TEST_ASSERT_EQUAL(len, 6, "unexpected length of 'reg': %zd", len);

	TEST_ASSERT_EQUAL(reg[0], 0x17020000, "unexpected value of "
			"reg[0]: %zx", (size_t) reg[0]);
	TEST_ASSERT_EQUAL(reg[1], 0x00000030, "unexpected value of "
			"reg[1]: %zx", (size_t) reg[1]);
	TEST_ASSERT_EQUAL(reg[2], 0x17020000, "unexpected value of "
			"reg[2]: %zx", (size_t) reg[2]);
	TEST_ASSERT_EQUAL(reg[3], 0x00010000, "unexpected value of "
			"reg[3]: %zx", (size_t) reg[3]);
	TEST_ASSERT_EQUAL(reg[4], 0x17020000, "unexpected value of "
			"reg[4]: %zx", (size_t) reg[4]);
	TEST_ASSERT_EQUAL(reg[5], 0x00000020, "unexpected value of "
			"reg[5]: %zx", (size_t) reg[5]);

	len = rte_fdt_path_read(fdt, base, "local-mac-address", mac, 6);
	TEST_ASSERT_EQUAL(len, 6, "unexpected length of "
			"'local-mac-address': %zd", len);
	for (i = 0; i < len; ++i) {
		int v = mac[i];
		int exp = e17020000_mac[i];
		TEST_ASSERT_EQUAL(v, exp, "unexpected mac[%zu]: %x\n", i, v);
	}

	rte_fdt_path_free(base);
	return 0;
}

#ifdef RTE_EXEC_ENV_LINUXAPP
static int test_fdt_path_read(void)
{
	int ret;
	struct rte_fdt *fdt;

	fdt = rte_fdt_open("linux-fdt/xgene1");
	TEST_ASSERT_NOT_NULL(fdt, "failed to open linux-fdt/xgene1");

	ret = test_fdt_path_read_common(fdt);
	if (ret)
		goto fail;

	ret = test_fdt_xgene1_ethernet(fdt);
	if (ret)
		goto fail;

fail:
	rte_fdt_close(fdt);
	return ret;
}
#else
static int test_fdt_path_read(void)
{
	printf("The %s is not implemented for this platform\n", __func__);
	return 0;
}
#endif

#ifdef RTE_EXEC_ENV_LINUXAPP
static int test_walk(__rte_unused struct rte_fdt *fdt,
		__rte_unused const struct rte_fdt_path *path,
		const char *top, void *context)
{
	struct {
		int seen;
		const char *name;
	} *expect = context;
	int i;

	for (i = 0; i < 5; ++i) {
		if (!strcmp(top, expect[i].name)) {
			expect[i].seen += 1;
			return 0;
		}
	}

	printf("unexpected top: '%s'\n", top);
	return 2; /* stop walking, unexpected top */
}

static int test_fdt_path_walk(void)
{
	int ret;
	struct rte_fdt *fdt;
	struct {
		int seen;
		const char *name;
	} expect[] = {
		{ 0, "#address-cells" },
		{ 0, "compatible" },
		{ 0, "model" },
		{ 0, "#size-cells" },
		{ 0, "soc" },
	};
	int i;

	fdt = rte_fdt_open("linux-fdt/xgene1");
	TEST_ASSERT_NOT_NULL(fdt, "failed to open linux-fdt/xgene1");

	ret = rte_fdt_path_walk(fdt, NULL, test_walk, expect);
	TEST_ASSERT_SUCCESS(ret, "walk has failed: %d", ret);

	for (i = 0; i < 5; ++i) {
		TEST_ASSERT_EQUAL(expect[i].seen, 1, "unexpected value of "
				"seen for '%s' (%u)", expect[i].name, i);
	}

	rte_fdt_close(fdt);
	return 0;
}
#else
static int test_fdt_path_walk(void)
{
	printf("The %s is not implemented for this platform\n", __func__);
	return 0;
}
#endif

static int test_fdt(void)
{
	if (test_fdt_open_close())
		return -1;

	if (test_fdt_path_is_valid())
		return -1;

	if (test_fdt_path_push_pop())
		return -1;

	if (test_fdt_path_parse())
		return -1;

	if (test_fdt_path_read())
		return -1;

	if (test_fdt_path_walk())
		return -1;

	return 0;
}

static struct test_command fdt_cmd = {
	.command = "fdt_autotest",
	.callback = test_fdt,
};
REGISTER_TEST_COMMAND(fdt_cmd);
