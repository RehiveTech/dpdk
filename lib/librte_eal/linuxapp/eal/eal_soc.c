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
#include <dirent.h>
#include <sys/stat.h>

#include <rte_log.h>
#include <rte_pci.h>
#include <rte_eal_memconfig.h>
#include <rte_malloc.h>
#include <rte_devargs.h>
#include <rte_memcpy.h>

#include "eal_filesystem.h"
#include "eal_private.h"
#include "eal_pci_init.h"

int
soc_map_device(struct rte_soc_device *dev)
{
	int rc = -1;

	switch (dev->kdrv) {
	case RTE_SOC_KDRV_NONE:
		rc = 0;
		break;
	default:
		RTE_LOG(DEBUG, EAL,
			"  Not managed by a supported kernel driver, skipped\n");
		rc = 1;
		break;
	}

	return rc;
}

void
soc_unmap_device(struct rte_soc_device *dev)
{
	switch (dev->kdrv) {
	case RTE_SOC_KDRV_NONE:
		break;
	default:
		RTE_LOG(DEBUG, EAL,
			"  Not managed by a supported kernel driver, skipped\n");
		break;
	}
}

static char *
linecpy(char *dst, const char *line, size_t max)
{
	size_t len = 0;

	while (line[len] && line[len] != '\n')
		len += 1;

	return (char *) memcpy(dst, line, len > max? max : len);
}

static char *
linedup(const char *line)
{
	size_t len = 0;
	char *s;

	while (line[len] && line[len] != '\n')
		len += 1;

	s = malloc(len + 1);
	if (s == NULL)
		return NULL;

	memcpy(s, line, len);
	s[len] = '\0';
	return s;
}

static const char *
uevent_find_entry(const char *start, const char *end,
		const char *prefix, const char *context)
{
	const size_t len = strlen(prefix);
	const char *p = start;

	while (strncmp(prefix, p, len)) {
		while (p < end && *p != '\n') {
			p += 1;
		}

		if (p >= end) {
			RTE_LOG(WARNING, EAL,
				"%s(): missing uevent entry %s (%s)\n",
				__func__, prefix, context);
			return NULL;
		}
		else {
			p += 1; /* skip end-of-line */
		}
	}

	if (p + len < end)
		return p + len;
	else {
		RTE_LOG(WARNING, EAL,
			"%s(): missing value for uevent entry %s (%s)\n",
			__func__, prefix, context);
		return NULL;
	}
}

static int
soc_device_from_uevent(struct rte_soc_device *dev, const char *uevent)
{
	FILE *f;
	struct stat st;
	char *buf;
	char *end;
	char *err;
	const char *entry;
	unsigned long i;
	unsigned long count;

	if ((f = fopen(uevent, "r")) == NULL) {
		RTE_LOG(ERR, EAL, "%s(): cannot open sysfs file uevent\n",
			__func__);
		return -1;
	}

	if (fstat(fileno(f), &st) < 0) {
		RTE_LOG(ERR, EAL, "%s(): cannot stat sysfs file uevent (%s)\n",
			__func__, strerror(errno));
		goto fail_fclose;
	}

	if (st.st_size <= 0) {
		RTE_LOG(ERR, EAL, "%s(): sysfs file uevent seems to be empty\n",
			__func__);
		goto fail_fclose_skip;
	}

	buf = malloc(st.st_size + 1);
	if (buf == NULL) {
		RTE_LOG(ERR, EAL, "%s(): failed to alloc memory\n", __func__);
		goto fail_fclose;
	}

	if (fread(buf, 1, st.st_size, f) == 0) {
		RTE_LOG(ERR, EAL, "%s(): cannot read sysfs file uevent\n",
			__func__);
		goto fail_free_buf;
	}
	buf[st.st_size] = '\0';
	end = buf + st.st_size;

	entry = uevent_find_entry(buf, end, "OF_FULLNAME=", uevent);
	if (entry == NULL)
		goto fail_free_buf_skip;

	linecpy((char *) dev->addr.devtree_path, entry,
		sizeof(dev->addr.devtree_path));

	RTE_LOG(DEBUG, EAL, "%s(): OF_FULLNAME=%s\n", __func__,
			dev->addr.devtree_path);

	entry = uevent_find_entry(buf, end, "OF_COMPATIBLE_N=", uevent);
	if (entry == NULL)
		goto fail_free_buf_skip; /* reported from uevent_find_entry */

	count = strtoul(entry, &err, 0);
	if (err == NULL) {
		RTE_LOG(ERR, EAL, "%s(): failed to parse OF_COMPATIBLE_N\n",
			__func__);
		goto fail_free_buf;
	}

	RTE_LOG(DEBUG, EAL, "%s(): OF_COMPATIBLE_N=%lu\n", __func__, count);

	dev->id.compatible = calloc(count + 1, sizeof(*dev->id.compatible));
	if (dev->id.compatible == NULL) {
		RTE_LOG(ERR, EAL, "%s(): failed to alloc memory\n",
			__func__);
		goto fail_free_buf;
	}

	if (count > 9999) /* FIXME: better way? */
		rte_exit(EXIT_FAILURE, "Strange count of OF_COMPATIBLE entries"
				"in sysfs uevent\n");

	for (i = 0; i < count; ++i) {
		char prefix[strlen("OF_COMPATIBLE_NNNN=")];
		snprintf(prefix, sizeof(prefix), "OF_COMPATIBLE_%lu=", i);

		entry = uevent_find_entry(buf, end, prefix, uevent);
		if (entry == NULL) {
			while (i-- > 0)
				free(dev->id.compatible[i]);
			goto fail_id_compatible;
		}

		dev->id.compatible[i] = linedup(entry);
		RTE_LOG(DEBUG, EAL, "%s(): %s%s\n", __func__, prefix,
				dev->id.compatible[i]);
		if (dev->id.compatible[i] == NULL) {
			RTE_LOG(ERR, EAL, "%s(): failed to alloc memory\n",
				__func__);

			while (i-- > 0)
				free(dev->id.compatible[i]);
			goto fail_id_compatible;
		}
	}

	dev->id.compatible[count] = NULL;
	return 0;

fail_id_compatible:
	free(dev->id.compatible);
fail_free_buf:
	free(buf);
fail_fclose:
	fclose(f);
	return -1;
fail_free_buf_skip:
	free(buf);
fail_fclose_skip:
	fclose(f);
	return 1;
}

static void
soc_device_uevent_free(struct rte_soc_device *dev)
{
	if (!dev)
		return;

	if (dev->id.compatible) {
		int i;

		for (i = 0; dev->id.compatible[i]; ++i)
			free(dev->id.compatible[i]);

		free(dev->id.compatible);
	}
}

static void
soc_device_free(struct rte_soc_device *dev)
{
	soc_device_uevent_free(dev);
	free(dev);
}

static int
soc_scan_one(const char *dirname)
{
	char filename[PATH_MAX];
	struct rte_soc_device *dev;
	unsigned long tmp;
	int rc;

	dev = calloc(1, sizeof(*dev));
	if (dev == NULL)
		return -1;

	snprintf(filename, sizeof(filename), "%s/numa_node", dirname);
	if (access(filename, R_OK) != 0) {
		/* no NUMA support */
		dev->numa_node = 0;
	} else {
		if (eal_parse_sysfs_value(filename, &tmp) < 0) {
			free(dev);
			return -1;
		}
		dev->numa_node = tmp;
	}

	snprintf(filename, sizeof(filename), "%s/uevent", dirname);
	rc = soc_device_from_uevent(dev, filename);
	if (rc) {
		free(dev);
		return rc;
	}

	dev->driver = NULL;
	dev->kdrv = RTE_SOC_KDRV_NONE;

	if (TAILQ_EMPTY(&soc_device_list)) {
		TAILQ_INSERT_TAIL(&soc_device_list, dev, next);
	} else {
		struct rte_soc_device *dev2;
		int rc;

		TAILQ_FOREACH(dev2, &soc_device_list, next) {
			rc = rte_eal_compare_soc_addr(&dev->addr, &dev2->addr);
			if (rc > 0)
				continue;

			if (rc < 0) {
				TAILQ_INSERT_BEFORE(dev2, dev, next);
			} else { /* already exists */
				dev2->kdrv = dev->kdrv;
				memmove(dev2->mem_resource, dev->mem_resource,
						sizeof(dev->mem_resource));
				soc_device_free(dev);
			}
			return 0;
		}
		TAILQ_INSERT_TAIL(&soc_device_list, dev, next);
	}

	return 0;
}

int
rte_eal_soc_scan(void)
{
	struct dirent *e;
	DIR *dir;
	char dirname[PATH_MAX];

	dir = opendir(SYSFS_SOC_DEVICES);
	if (dir == NULL) {
		RTE_LOG(ERR, EAL, "%s(): opendir failed: %s\n",
			__func__, strerror(errno));
		return -1;
	}

	while ((e = readdir(dir)) != NULL) {
		if (e->d_name[0] == '.')
			continue;

		snprintf(dirname, sizeof(dirname), "%s/%s", SYSFS_SOC_DEVICES,
				e->d_name);
		if (soc_scan_one(dirname) < 0)
			goto error;
	}

	closedir(dir);
	return 0;

error:
	closedir(dir);
	return -1;
}

int
rte_eal_soc_init(void)
{
	TAILQ_INIT(&soc_driver_list);
	TAILQ_INIT(&soc_device_list);

	if (internal_config.no_soc)
		return 0;

	if (rte_eal_soc_scan() < 0) {
		RTE_LOG(ERR, EAL, "%s(): Failed to scan for SoC devices\n",
			__func__);
		return -1;
	}

	return 0;
}
