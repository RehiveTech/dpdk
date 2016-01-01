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

#ifndef _RTE_SOC_H_
#define _RTE_SOC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/errno.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_interrupts.h>

TAILQ_HEAD(soc_device_list, rte_soc_device);
TAILQ_HEAD(soc_driver_list, rte_soc_driver);

extern struct soc_device_list soc_device_list;
extern struct soc_driver_list soc_driver_list;

/* Path to detect platform devices (in architecture-specific bus systems). */
#define SYSFS_SOC_DEVICES "/sys/bus/platform/devices"
/* Flat Device Tree location in the system. */
#define FDT_ROOT "/proc/device-tree"

struct rte_soc_resource {
	uint64_t phys_addr; /**< Physical address, 0 if no resource. */
	uint64_t len;       /**< Length of the resource. */
	void *addr;         /**< Virtual address, NULL when not mapped. */
};

/** Maximum number of SoC resources. */
#define SOC_MAX_RESOURCE 6

struct rte_soc_id {
	char **compatible; /**< List of compatible strings. */
};

struct rte_soc_addr {
	char devtree_path[PATH_MAX]; /** Path to identify the device in FDT. */
};

enum rte_soc_kernel_driver {
	RTE_SOC_KDRV_UNKNOWN = 0,
	RTE_SOC_KDRV_NONE
};

/**
 * A structure describing a SoC device. A SoC device is connected via some
 * architecture-specific bus without auto-discovery features. Currently, this
 * covers devices detected by reading the device-tree provided by the OS.
 */
struct rte_soc_device {
	TAILQ_ENTRY(rte_soc_device) next;   /**< Next probed SoC device. */
	struct rte_soc_addr addr;           /**< Device-tree location. */
	struct rte_soc_id id;               /**< Device identification. */
	struct rte_soc_resource mem_resource[SOC_MAX_RESOURCE]; /**< SoC memory resource. */
	struct rte_intr_handle intr_handle; /**< Interrupt handle. */
	struct rte_soc_driver *driver;      /**< Associated driver. */
	int numa_node;                      /**< NUMA node connection. */
	struct rte_devargs *devargs;        /**< Device user arguments. */
	enum rte_soc_kernel_driver kdrv;    /**< Kernel driver passthrough. */
};

struct rte_soc_driver;

/**
 * Initialization function for the driver called during SoC probing.
 */
typedef int (soc_devinit_t)(struct rte_soc_driver *, struct rte_soc_device *);

/**
 * Uninitialization function for the driver called during SoC hotplugging.
 */
typedef int (soc_devuninit_t)(struct rte_soc_device *);

struct rte_soc_driver {
	TAILQ_ENTRY(rte_soc_driver) next;  /**< Next in list. */
	const char *name;                  /**< Driver name. */
	soc_devinit_t *devinit;            /**< Device init. function. */
	soc_devuninit_t *devuninit;        /**< Device uninit. function. */
	const struct rte_soc_id *id_table; /**< ID table, NULL terminated. */
	uint32_t drv_flags;                /**< Flags for handling of device. */
};

struct soc_map {
	void *addr;
	char *path;
	uint64_t offset;
	uint64_t size;
	uint64_t phaddr;
};

struct mapped_soc_resource {
	TAILQ_ENTRY(mapped_soc_resource) next;
	struct rte_soc_addr soc_addr;
	char path[PATH_MAX];
	int nb_maps;
	struct soc_map maps[SOC_MAX_RESOURCE];
};

TAILQ_HEAD(mapped_soc_res_list, mapped_soc_resource);

/**
 * Compare two SoC device address.
 * @return
 *	0 on addr == addr2
 *	Positive on addr > addr2
 *	Negative on addr < addr2
 */
static inline int
rte_eal_compare_soc_addr(const struct rte_soc_addr *addr,
			 const struct rte_soc_addr *addr2)
{
	if ((addr == NULL) || (addr2 == NULL))
		return -1;

	return strcmp(addr->devtree_path, addr2->devtree_path);
}

/**
 * Scan the architecture-specific buses for the SoC devices, and the devices
 * in the devices list.
 *
 * @return
 * 	0 on success
 */
int rte_eal_soc_scan(void);

/**
 * Probe SoC devices for registered drivers.
 *
 * Call probe() function for all registered drivers that have a matching entry
 * in its id_table for discovered devices.
 *
 * @return
 *	0 on success
 */
int rte_eal_soc_probe(void);

void *soc_map_resource(void *requested_addr, int fd, off_t offset,
		size_t size, int additional_flags);
void soc_unmap_resource(void *requested_addr, size_t size);

/**
 * Probe the single SoC device.
 *
 * Find the SoC device specified by the SoC address, then call the probe()
 * function for the registered driver that has a matching entry in its id_table.
 *
 * @return
 *	0 on success
 */
int rte_eal_soc_probe_one(const struct rte_soc_addr *addr);

/**
 * Close the single SoC device.
 *
 * Find the SoC device specified by the SoC address, then call devuninit()
 * function for the registered driver.
 */
int rte_eal_soc_detach(const struct rte_soc_addr *addr);

void rte_eal_soc_dump(FILE *f);

void rte_eal_soc_register(struct rte_soc_driver *driver);
void rte_eal_soc_unregister(struct rte_soc_driver *driver);

#ifdef __cplusplus
}
#endif

#endif
