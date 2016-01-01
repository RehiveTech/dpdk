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
 *     * Neither the name of Intel Corporation nor the names of its
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
#include <stdint.h>
#include <sys/queue.h>

#include <rte_interrupts.h>
#include <rte_soc.h>
#include <rte_ethdev.h>
#include <rte_devargs.h>

#include "test.h"

#define NUM_MAX_DRIVERS 256

static unsigned int soc_dev_init_count;
static unsigned int soc_dev_uninit_count;

static int test_driver0_init(struct rte_soc_driver *dr,
		struct rte_soc_device *dev)
{
	printf("Initialize %s\n", dr->name);
	printf("Device: %s\n", dev->addr.devtree_path);

	soc_dev_init_count += 1;
	return 0;
}

static int test_driver0_uninit(struct rte_soc_device *dev)
{
	if (dev->driver == NULL) {
		printf("Uninitialize device %s without any driver\n",
			dev->addr.devtree_path);
	} else {
		printf("Uninitialize device %s bound to %s\n",
			dev->addr.devtree_path, dev->driver->name);
	}

	soc_dev_uninit_count += 1;
	return 0;
}

static const char *test_driver0_compatible[] = {
	"dpdk,test-device",
	NULL
};

static const struct rte_soc_id test_driver0_id_table[] = {
	{
		.compatible = test_driver0_compatible
	},
	{
		.compatible = NULL
	}
};

static struct rte_soc_driver test_driver0 = {
	.name = "test_driver0",
	.devinit = test_driver0_init,
	.devuninit = test_driver0_uninit,
	.id_table = test_driver0_id_table,
	.drv_flags = 0
};

static void
rte_eal_soc_detach_all(void)
{
	struct rte_soc_device *dev = NULL;

	TAILQ_FOREACH(dev, &soc_device_list, next) {
		int rc = rte_eal_soc_detach(&dev->addr);
		if (rc != 0) {
			rte_exit(EXIT_FAILURE, "Failed to detach device %s\n",
				dev->addr.devtree_path);
		}
	}
}

static int
blacklist_all_devices(void)
{
	struct rte_soc_device *dev = NULL;
	int i = 0;

	TAILQ_FOREACH(dev, &soc_device_list, next) {
		int rc = rte_eal_devargs_add(RTE_DEVTYPE_BLACKLISTED_SOC,
			dev->addr.devtree_path);

		if (rc < 0) {
			printf("Failed to blacklist device %s\n",
				dev->addr.devtree_path);
			return -1;
		}

		i += 1;
	}

	printf("%u devices have been blacklisted\n", i);
	return 0;
}

static void free_devargs_list(void)
{
	struct rte_devargs *devargs;

	while (!TAILQ_EMPTY(&devargs_list)) {
		devargs = TAILQ_FIRST(&devargs_list);
		TAILQ_REMOVE(&devargs_list, devargs, next);
		if (devargs->args)
			free(devargs->args);

		free(devargs);
	}
}

int test_soc_run = 0; /* value checked by the multiprocess test */

static int
test_soc(void)
{
	struct rte_devargs_list save_devargs_list;
	struct rte_soc_driver *dr = NULL;
	struct rte_soc_driver *save_soc_driver_list[NUM_MAX_DRIVERS];
	int num_drivers = 0;
	int i;
	int rc = 0;

	if (TAILQ_EMPTY(&soc_device_list)) {
		printf("There are no SoC devices detected\n");
		return -1;
	}

	printf("Dump all devices\n");
	rte_eal_soc_dump(stdout);

	TAILQ_FOREACH(dr, &soc_driver_list, next) {
		rte_eal_soc_unregister(dr);
		save_soc_driver_list[num_drivers++] = dr;
	}

	rte_eal_soc_register(&test_driver0);

	soc_dev_init_count = 0;
	soc_dev_uninit_count = 0;

	printf("Probe SoC devices\n");
	rte_eal_soc_probe();

	if (soc_dev_init_count == 0) {
		printf("No SoC device detected\n");
		rc = -1;
		goto failed;
	}

	rte_eal_soc_detach_all();

	if (soc_dev_init_count != soc_dev_uninit_count) {
		printf("Detached %u out of %u devices\n",
			soc_dev_uninit_count, soc_dev_init_count);
		rc = -1;
		goto failed;
	}

	if (rte_eal_soc_scan()) {
		printf("Failed to scan for SoC devices\n");
		rc = -1;
		goto failed;
	}

	if (TAILQ_EMPTY(&soc_device_list)) {
		printf("There are no SoC devices detected\n");
		rc = -1;
		goto failed;
	}

	save_devargs_list = devargs_list;
	TAILQ_INIT(&devargs_list);

	if (blacklist_all_devices()) {
		free_devargs_list();
		devargs_list = save_devargs_list;
		rc = -1;
		goto failed_devargs_restore;
	}

	soc_dev_init_count = 0;
	soc_dev_uninit_count = 0;

	printf("Probe SoC devices while all are blacklisted\n");
	rte_eal_soc_probe();

	if (soc_dev_init_count != 0) {
		printf("%u devices where probed while blacklisted\n",
			soc_dev_init_count);
		rc = -1;
	}

failed_devargs_restore:
	free_devargs_list();
	devargs_list = save_devargs_list;

failed:
	rte_eal_soc_unregister(&test_driver0);

	test_soc_run = 1;

	for (i = 0; i < num_drivers; ++i)
		rte_eal_soc_register(save_soc_driver_list[i]);

	return rc;
}

static struct test_command soc_cmd = {
	.command = "soc_autotest",
	.callback = test_soc,
};
REGISTER_TEST_COMMAND(soc_cmd);
