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

/**
 * @file
 *
 * RTE SoC Interface
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>

#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_dev.h>
#include <rte_interrupts.h>

TAILQ_HEAD(soc_driver_list, rte_soc_driver); /**< SoC drivers in D-linked Q. */
TAILQ_HEAD(soc_device_list, rte_soc_device); /**< SoC devices in D-linked Q. */

extern struct soc_driver_list soc_driver_list; /**< Global list of SoC drivers. */
extern struct soc_device_list soc_device_list; /**< Global list of SoC devices. */

/** Return SoC scan path of the sysfs root. */
const char *soc_get_sysfs_path(void);

#define SOC_MAX_RESOURCE 6

struct rte_soc_resource {
	uint64_t phys_addr;
	uint64_t len;
	void *addr;
};

struct rte_soc_id {
	union {
		/* workaround -Werror=discarded-qualifiers,cast-equal */
		char *_compatible;      /**< OF compatible specification */
		const char *compatible; /**< OF compatible specification */
	};
};

struct rte_soc_addr {
	char *name;     /**< name used in sysfs */
	char *fdt_path; /**< path to the associated node in FDT */
};

struct rte_devargs;

/**
 * A structure describing a SoC device.
 */
struct rte_soc_device {
	TAILQ_ENTRY(rte_soc_device) next;   /**< Next probed SoC device */
	struct rte_soc_addr addr;           /**< SoC device Location */
	struct rte_soc_id *id;              /**< SoC device ID list */
	struct rte_soc_resource mem_resource[SOC_MAX_RESOURCE];
	struct rte_intr_handle intr_handle; /**< Interrupt handle */
	struct rte_soc_driver *driver;      /**< Associated driver */
	int numa_node;                      /**< NUMA node connection */
	int is_dma_coherent;                /**< DMA coherent device */
	struct rte_devargs *devargs;        /**< Device user arguments */
	enum rte_kernel_driver kdrv;        /**< Kernel driver */
};

struct rte_soc_driver;

/**
 * Initialization function for the driver called during SoC probing.
 */
typedef int (soc_devinit_t)(struct rte_soc_driver *, struct rte_soc_device *);

/**
 * Uninitialization function for the driver called during hotplugging.
 */
typedef int (soc_devuninit_t)(struct rte_soc_device *);

/**
 * A structure describing a SoC driver.
 */
struct rte_soc_driver {
	TAILQ_ENTRY(rte_soc_driver) next;  /**< Next in list */
	const char *name;                  /**< Driver name */
	soc_devinit_t *devinit;            /**< Device initialization */
	soc_devuninit_t *devuninit;        /**< Device uninitialization */
	const struct rte_soc_id *id_table; /**< ID table, NULL terminated */
	uint32_t drv_flags;                /**< Control handling of device */
};

/** Device needs to map its resources by EAL */
#define RTE_SOC_DRV_NEED_MAPPING 0x0001
/** Device needs to be unbound event if no module is provieded */
#define RTE_SOC_DRV_FORCE_UNBIND 0x0004
/** Device driver supports link state interrupt */
#define RTE_SOC_DRV_INTR_LSC	 0x0008
/** Device driver supports detaching capability */
#define RTE_SOC_DRV_DETACHABLE	 0x0010
/** Device driver accepts DMA non-coherent devices */
#define RTE_SOC_DRV_ACCEPT_NONCC 0x0020

/**
 * A structure describing a SoC mapping.
 */
struct soc_map {
	void *addr;
	char *path;
	uint64_t offset;
	uint64_t size;
	uint64_t phaddr;
};

/**
 * A structure describing a mapped SoC resource.
 * For multi-process we need to reproduce all SoC mappings in secondary
 * processes, so save them in a tailq.
 */
struct mapped_soc_resource {
	TAILQ_ENTRY(mapped_soc_resource) next;

	struct rte_soc_addr soc_addr;
	char path[PATH_MAX];
	int nb_maps;
	struct soc_map maps[SOC_MAX_RESOURCE];
};

/** mapped SoC resource list */
TAILQ_HEAD(mapped_soc_res_list, mapped_soc_resource);

/**
 * Utility function to write a SoC device name, this device name can later be
 * used to retrieve the corresponding rte_soc_addr using above functions.
 *
 * @param addr
 *	The SoC address
 * @param output
 *	The output buffer string
 * @param size
 *	The output buffer size
 * @return
 *  0 on success, negative on error.
 */
static inline void
rte_eal_soc_device_name(const struct rte_soc_addr *addr,
		    char *output, size_t size)
{
	int ret;
	RTE_VERIFY(addr != NULL);
	RTE_VERIFY(size >= strlen(addr->name));
	ret = snprintf(output, size, "%s", addr->name);
	RTE_VERIFY(ret >= 0);
}

static inline int
rte_eal_compare_soc_addr(const struct rte_soc_addr *a0,
                         const struct rte_soc_addr *a1)
{
	if (a0 == NULL || a1 == NULL)
		return -1;

	RTE_VERIFY(a0->name != NULL);
	RTE_VERIFY(a1->name != NULL);

	return strcmp(a0->name, a1->name);
}

/**
 * Parse a specification of a soc device. The specification must differentiate
 * a SoC device specification from the PCI bus and virtual devices. We assume
 * a SoC specification starts with "soc:". The function allocates the name
 * entry of the given addr.
 *
 * @return
 *      -  0 on success
 *      -  1 when not a SoC spec
 *      - -1 on failure
 */
static inline int
rte_eal_parse_soc_spec(const char *spec, struct rte_soc_addr *addr)
{
	if (strstr(spec, "soc:") == spec) {
		addr->name = strdup(spec + 4);
		if (addr->name == NULL)
			return -1;
		return 0;
	}

	return 1;
}

/**
 * Scan for new SoC devices.
 */
int rte_eal_soc_scan(void);

/**
 * Probe SoC devices for registered drivers.
 */
int rte_eal_soc_probe(void);

/**
 * Probe the single SoC device.
 */
int rte_eal_soc_probe_one(const struct rte_soc_addr *addr);

/**
 * Close the single SoC device.
 *
 * Scan the SoC devices and find the SoC device specified by the SoC
 * address, then call the devuninit() function for registered driver
 * that has a matching entry in its id_table for discovered device.
 *
 * @param addr
 *	The SoC address to close.
 * @return
 *   - 0 on success.
 *   - Negative on error.
 */
int rte_eal_soc_detach(const struct rte_soc_addr *addr);

/**
 * Map SoC device resources into userspace.
 *
 * This is called by the EAL if (drv_flags & RTE_SOC_DRV_NEED_MAPPING).
 */
int rte_eal_soc_map_device(struct rte_soc_device *dev);

/**
 * Unmap the device resources.
 */
void rte_eal_soc_unmap_device(struct rte_soc_device *dev);

/**
 * Dump discovered SoC devices.
 */
void rte_eal_soc_dump(FILE *f);

/**
 * Register a SoC driver.
 */
void rte_eal_soc_register(struct rte_soc_driver *driver);

#define RTE_EAL_SOC_REGISTER(name) \
RTE_INIT(socinitfn_ ##name); \
static void socinitfn_ ##name(void) \
{ \
	rte_eal_soc_register(&name.soc_drv); \
}

/**
 * Unregister a SoC driver.
 */
void rte_eal_soc_unregister(struct rte_soc_driver *driver);

#endif
