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

#include <stddef.h>
#include <stdio.h>
#include <sys/queue.h>

#include <rte_log.h>
#include <rte_common.h>
#include <rte_soc.h>

#include "eal_private.h"

struct soc_driver_list soc_driver_list =
	TAILQ_HEAD_INITIALIZER(soc_driver_list);
struct soc_device_list soc_device_list =
	TAILQ_HEAD_INITIALIZER(soc_device_list);

/** Pathname of SoC devices directory. */
#define SYSFS_SOC_DEVICES "/sys/bus/platform/devices"

const char *soc_get_sysfs_path(void)
{
	const char *path = NULL;

	path = getenv("SYSFS_SOC_DEVICES");
	if (path == NULL)
		return SYSFS_SOC_DEVICES;

	return path;
}

static int soc_id_match(const struct rte_soc_id *drv_id,
		const struct rte_soc_id *dev_id)
{
	int i;
	int j;

	RTE_VERIFY(drv_id != NULL);
	RTE_VERIFY(dev_id != NULL);

	for (i = 0; drv_id[i].compatible; ++i) {
		const char *drv_compat = drv_id[i].compatible;

		for (j = 0; dev_id[j].compatible; ++j) {
			const char *dev_compat = dev_id[j].compatible;

			if (!strcmp(drv_compat, dev_compat))
				return 1;
		}
	}

	return 0;
}

static int
rte_eal_soc_probe_one_driver(struct rte_soc_driver *dr,
		struct rte_soc_device *dev)
{
	int ret;

	if (!soc_id_match(dr->id_table, dev->id))
		return 1;

	RTE_LOG(DEBUG, EAL, "SoC device %s\n",
			dev->addr.name);
	RTE_LOG(DEBUG, EAL, "  probe driver %s\n", dr->name);

	dev->driver = dr;
	RTE_VERIFY(dr->devinit != NULL);
	return dr->devinit(dr, dev);
}

static int
soc_probe_all_drivers(struct rte_soc_device *dev)
{
	struct rte_soc_driver *dr = NULL;
	int rc = 0;

	if (dev == NULL)
		return -1;

	TAILQ_FOREACH(dr, &soc_driver_list, next) {
		rc = rte_eal_soc_probe_one_driver(dr, dev);
		if (rc < 0)
			/* negative value is an error */
			return -1;
		if (rc > 0)
			/* positive value means driver doesn't support it */
			continue;
		return 0;
	}
	return 1;
}

/* If the IDs match, call the devuninit() function of the driver. */
static int
rte_eal_soc_detach_dev(struct rte_soc_driver *dr,
		struct rte_soc_device *dev)
{
	if ((dr == NULL) || (dev == NULL))
		return -EINVAL;

	if (!soc_id_match(dr->id_table, dev->id))
		return 1;

	RTE_LOG(DEBUG, EAL, "SoC device %s\n",
			dev->addr.name);

	RTE_LOG(DEBUG, EAL, "  remove driver: %s\n", dr->name);

	if (dr->devuninit && (dr->devuninit(dev) < 0))
		return -1;	/* negative value is an error */

	/* clear driver structure */
	dev->driver = NULL;

	return 0;
}

/*
 * Call the devuninit() function of all registered drivers for the given
 * device if their IDs match.
 *
 * @return
 *       0 when successful
 *      -1 if deinitialization fails
 *       1 if no driver is found for this device.
 */
static int
soc_detach_all_drivers(struct rte_soc_device *dev)
{
	struct rte_soc_driver *dr = NULL;
	int rc = 0;

	if (dev == NULL)
		return -1;

	TAILQ_FOREACH(dr, &soc_driver_list, next) {
		rc = rte_eal_soc_detach_dev(dr, dev);
		if (rc < 0)
			/* negative value is an error */
			return -1;
		if (rc > 0)
			/* positive value means driver doesn't support it */
			continue;
		return 0;
	}
	return 1;
}

/*
 * Detach device specified by its SoC address.
 */
int
rte_eal_soc_detach(const struct rte_soc_addr *addr)
{
	struct rte_soc_device *dev = NULL;
	int ret = 0;

	if (addr == NULL)
		return -1;

	TAILQ_FOREACH(dev, &soc_device_list, next) {
		if (rte_eal_compare_soc_addr(&dev->addr, addr))
			continue;

		ret = soc_detach_all_drivers(dev);
		if (ret < 0)
			goto err_return;

		TAILQ_REMOVE(&soc_device_list, dev, next);
		return 0;
	}
	return -1;

err_return:
	RTE_LOG(WARNING, EAL, "Requested device %s cannot be used\n",
			dev->addr.name);
	return -1;
}

int
rte_eal_soc_probe_one(const struct rte_soc_addr *addr)
{
	struct rte_soc_device *dev = NULL;
	int ret = 0;

	if (addr == NULL)
		return -1;

	TAILQ_FOREACH(dev, &soc_device_list, next) {
		if (rte_eal_compare_soc_addr(&dev->addr, addr))
			continue;

		ret = soc_probe_all_drivers(dev);
		if (ret < 0)
			goto err_return;
		return 0;
	}
	return -1;

err_return:
	RTE_LOG(WARNING, EAL,
		"Requested device %s cannot be used\n", addr->name);
	return -1;
}

/*
 * Scan the SoC devices and call the devinit() function for all registered
 * drivers that have a matching entry in its id_table for discovered devices.
 */
int
rte_eal_soc_probe(void)
{
	struct rte_soc_device *dev = NULL;
	int probe_all = 0;
	int ret = 0;

	TAILQ_FOREACH(dev, &soc_device_list, next) {

		ret = soc_probe_all_drivers(dev);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Requested device %s"
				 " cannot be used\n", dev->addr.name);
	}

	return 0;
}

/* dump one device */
static int
soc_dump_one_device(FILE *f, struct rte_soc_device *dev)
{
	int i;

	fprintf(f, "%s", dev->addr.name);
	fprintf(f, " - fdt_path: %s\n",
			dev->addr.fdt_path? dev->addr.fdt_path : "(none)");

	for (i = 0; dev->id && dev->id[i].compatible; ++i)
		fprintf(f, "   %s\n", dev->id[i].compatible);

	return 0;
}

/* dump devices on the bus */
void
rte_eal_soc_dump(FILE *f)
{
	struct rte_soc_device *dev = NULL;

	TAILQ_FOREACH(dev, &soc_device_list, next) {
		soc_dump_one_device(f, dev);
	}
}

/* register a driver */
void
rte_eal_soc_register(struct rte_soc_driver *driver)
{
	TAILQ_INSERT_TAIL(&soc_driver_list, driver, next);
}

/* unregister a driver */
void
rte_eal_soc_unregister(struct rte_soc_driver *driver)
{
	TAILQ_REMOVE(&soc_driver_list, driver, next);
}
