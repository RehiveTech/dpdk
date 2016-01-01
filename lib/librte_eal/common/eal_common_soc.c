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

#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include <sys/mman.h>

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_soc.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_devargs.h>

#include "eal_private.h"

struct soc_driver_list soc_driver_list;
struct soc_device_list soc_device_list;

static struct rte_devargs *soc_devargs_lookup(struct rte_soc_device *dev)
{
	struct rte_devargs *devargs;

	TAILQ_FOREACH(devargs, &devargs_list, next) {
		if (devargs->type != RTE_DEVTYPE_BLACKLISTED_SOC
			&& devargs->type != RTE_DEVTYPE_WHITELISTED_SOC)
			continue;

		if (!rte_eal_compare_soc_addr(&dev->addr, &devargs->soc.addr))
			return devargs;
	}

	return NULL;
}

/* TODO: generalize and merge with PCI version */
void *
soc_map_resource(void *requested_addr, int fd, off_t offset, size_t size,
		int additional_flags)
{
	void *mapaddr;

	mapaddr = mmap(requested_addr, size, PROT_READ | PROT_WRITE,
			MAP_SHARED | additional_flags, fd, offset);
	if (mapaddr == MAP_FAILED) {
		RTE_LOG(ERR, EAL, "%s(): cannot mmap(%d, %p, 0x%lx, 0x%lx): %s (%p)\n",
			__func__, fd, requested_addr,
			(unsigned long)size, (unsigned long)offset,
			strerror(errno), mapaddr);
	} else
		RTE_LOG(DEBUG, EAL, "  SoC memory mapped at %p\n", mapaddr);

	return mapaddr;
}

/* TODO: generalize and merge with PCI version */
void
soc_unmap_resource(void *requested_addr, size_t size)
{
	if (requested_addr == NULL)
		return;

	if (munmap(requested_addr, size)) {
		RTE_LOG(ERR, EAL, "%s(): cannot munmap(%p, 0x%lx): %s\n",
			__func__, requested_addr, (unsigned long)size,
			strerror(errno));
	} else
		RTE_LOG(DEBUG, EAL, "  SoC memory unmapped at %p\n",
				requested_addr);
}

static int
rte_eal_soc_id_match(const struct rte_soc_id *dr, const struct rte_soc_id *dev)
{
	int i;

	if (dr == NULL || dev == NULL)
		return 0;

	if (dr->compatible == NULL || dev->compatible == NULL)
		return 0;

	for (i = 0; dr->compatible[i]; ++i) {
		int j;

		for (j = 0; dev->compatible[j]; ++j) {
			if (!strcmp(dr->compatible[i], dev->compatible[j]))
				return 1;
		}
	}

	return 0;
}

static int
rte_eal_soc_probe_one_driver(struct rte_soc_driver *dr, struct rte_soc_device *dev)
{
	int ret;
	const struct rte_soc_id *id_table;

	if (dr == NULL || dev == NULL)
		return -EINVAL;

	for (id_table = dr->id_table; id_table->compatible; ++id_table) {
		if (!rte_eal_soc_id_match(id_table, &dev->id))
			continue;

		RTE_LOG(DEBUG, EAL, "SoC device %s on NUMA socket %i\n",
				dev->addr.devtree_path, dev->numa_node);
		RTE_LOG(DEBUG, EAL, "  probe driver: %s\n", dr->name);

		if (dev->devargs != NULL
			&& dev->devargs->type == RTE_DEVTYPE_BLACKLISTED_SOC) {
			RTE_LOG(DEBUG, EAL, "  Device is blacklisted, skipping\n");
			return 1;
		}

		ret = soc_map_device(dev);
		if (ret != 0)
			return ret;

		dev->driver = dr;
		return dr->devinit(dr, dev);
	}

	return 1;
}

static int
rte_eal_soc_detach_dev(struct rte_soc_driver *dr, struct rte_soc_device *dev)
{
	const struct rte_soc_id *id_table;

	if (dr == NULL || dev == NULL)
		return -EINVAL;

	for (id_table = dr->id_table; id_table->compatible; ++id_table) {
		if (!rte_eal_soc_id_match(id_table, &dev->id))
			continue;

		RTE_LOG(DEBUG, EAL, "SoC device %s on NUMA socket %i\n",
				dev->addr.devtree_path, dev->numa_node);

		RTE_LOG(DEBUG, EAL, "  remove driver: %s %s\n",
				dev->addr.devtree_path, dr->name);

		if (dr->devuninit && (dr->devuninit(dev) < 0))
			return -1; /* error */

		dev->driver = NULL;
		soc_unmap_device(dev);

		return 0;
	}

	return 1;
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
			return -1; /* error */
		if (rc > 0)
			continue; /* no driver match */

		return 0;
	}

	return 1;
}

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
			return -1; /* error */
		if (rc > 0)
			continue; /* not driver match */
		return 0;
	}

	return 1;
}

int
rte_eal_soc_probe_one(const struct rte_soc_addr *addr)
{
	struct rte_soc_device *dev = NULL;
	int rc = 0;

	if (addr == NULL)
		return -1;

	TAILQ_FOREACH(dev, &soc_device_list, next) {
		if (rte_eal_compare_soc_addr(&dev->addr, addr))
			continue;

		rc = soc_probe_all_drivers(dev);
		if (rc < 0)
			goto err_return;

		return 0;
	}

	return -1;

err_return:
	RTE_LOG(WARNING, EAL, "Failed to probe device %s\n",
			dev->addr.devtree_path);
	return -1;
}

int
rte_eal_soc_detach(const struct rte_soc_addr *addr)
{
	struct rte_soc_device *dev = NULL;
	int rc = 0;

	if (addr == NULL)
		return -1;

	TAILQ_FOREACH(dev, &soc_device_list, next) {
		if (rte_eal_compare_soc_addr(&dev->addr, addr))
			continue;

		rc = soc_detach_all_drivers(dev);
		if (rc < 0)
			goto err_return;

		TAILQ_REMOVE(&soc_device_list, dev, next);
		return 0;
	}

	return -1;

err_return:
	RTE_LOG(WARNING, EAL, "Failed to detach device %s\n",
			dev->addr.devtree_path);
	return -1;
}

int
rte_eal_soc_probe(void)
{
	struct rte_soc_device *dev = NULL;
	struct rte_devargs *devargs;
	int probe_all = 0;
	int rc = 0;

	if (rte_eal_devargs_type_count(RTE_DEVTYPE_WHITELISTED_SOC) == 0)
		probe_all = 1;

	TAILQ_FOREACH(dev, &soc_device_list, next) {
		devargs = soc_devargs_lookup(dev);
		if (devargs != NULL)
			dev->devargs = devargs;

		/* probe _all_ or _only whitelisted_ devices */
		if (probe_all)
			rc = soc_probe_all_drivers(dev);
		else if (devargs != NULL
			&& devargs->type == RTE_DEVTYPE_WHITELISTED_SOC)
			rc = soc_probe_all_drivers(dev);
		if (rc < 0)
			rte_exit(EXIT_FAILURE,
				"Requested device %s cannot be used",
				dev->addr.devtree_path);
	}

	return 0;
}

#define SIZEOF_MEM_RESOURCE_ARRAY(r) (sizeof(r) / sizeof(r[0]))

static int
soc_dump_one_device(FILE *f, struct rte_soc_device *dev)
{
	int i;

	fprintf(f, "%s\n", dev->addr.devtree_path);

	for (i = 0; dev->id.compatible && dev->id.compatible[i]; ++i)
		fprintf(f, "   %s\n", dev->id.compatible[i]);
	for (i = 0; i != SIZEOF_MEM_RESOURCE_ARRAY(dev->mem_resource); ++i) {
		fprintf(f, "   %16.16"PRIx64" %16.16"PRIx64"\n",
			dev->mem_resource[i].phys_addr,
			dev->mem_resource[i].len);
	}

	return 0;
}

void
rte_eal_soc_dump(FILE *f)
{
	struct rte_soc_device *dev = NULL;

	TAILQ_FOREACH(dev, &soc_device_list, next)
		soc_dump_one_device(f, dev);
}

void
rte_eal_soc_register(struct rte_soc_driver *dr)
{
	TAILQ_INSERT_TAIL(&soc_driver_list, dr, next);
}

void
rte_eal_soc_unregister(struct rte_soc_driver *dr)
{
	TAILQ_REMOVE(&soc_driver_list, dr, next);
}
