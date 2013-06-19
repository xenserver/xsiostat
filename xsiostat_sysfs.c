/*
 * -----------------------------
 *  XenServer Storage I/O Stats
 * -----------------------------
 *  xsiostat_sysfs.c
 * ------------------
 *
 * Copyright (C) 2013 Citrix Systems Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 only. with the special
 * exception on linking described in file LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 */

// Header files
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include "xsiostat.h"

/*
 * int
 * mpool_update(mt_mempool_t *mpool, mt_mempool_entry_t mpool_field);
 * ------------------------------------------------------------------
 * This function reads the corresponding sysfs mpool and updates the
 * fields of the mpool struct accordingly.
 * Return values:
 *  -1 An error occurred.
 *   0 Success.
 */
int
mpool_update(mt_mempool_t *mpool, char *mpool_field){
	// Local variables
	FILE		*entry_fp = NULL;	// sysfs file pointer
	char		*entry_fn = NULL;	// sysfs file name
	char		buf[128];		// read buffer
	int		ret = 0;

	// Set the sysfs file name
	if (asprintf(&entry_fn, "%s/%s/%s", MT_SYSFS_BBPOOL, mpool->name, mpool_field) <= 0){
		perror("asprintf");
		fprintf(stderr, "Error allocating mpool entry name.\n");
		entry_fn = NULL;
		goto err;
	}

	// Open the sysfs entry
	if ((entry_fp = fopen(entry_fn, "r")) == NULL){
		perror("fopen");
		fprintf(stderr, "Error opening mpool entry: '%s'\n", entry_fn);
		goto err;
	}

	// Read the content
	memset(buf, 0, sizeof(buf));
	if (fgets(buf, sizeof(buf), entry_fp) == NULL){
		perror("fgets");
		fprintf(stderr, "Error reading mpool entry: '%s'\n", entry_fn);
		goto err;
	}

	// Update mempool accordingly
	if (!strcmp(mpool_field, MT_SYSFS_BBPOOL_FREE)){
		mpool->free = atoi(buf);
	}else
	if (!strcmp(mpool_field, MT_SYSFS_BBPOOL_SIZE)){
		mpool->size = atoi(buf);
	}

	// Skip error section
	goto out;

err:
	ret = -1;

out:
	if (entry_fn)
		free(entry_fn);
	if (entry_fp)
		fclose(entry_fp);

	return(ret);
}

/*
 * void mpools_clean(mt_mempool_t **mpools);
 * -----------------------------------------
 * This function removes mpools that have no vbds associated with it.
 */
void
mpools_clean(mt_mempool_t **mpools){
	// Local variables
	mt_mempool_t	*mpool_tmp;	// temporary mt_mempool_t pointer
	mt_mempool_t	*mpool_tmp2;	// temporary mt_mempool_t pointer

	// Loop through mpools
	mpool_tmp = mpool_tmp2 = *mpools;
	while (mpool_tmp){
		// Remove empty mpool
		if (mpool_tmp->vbds == NULL){
			// Handle first entry special case
			if (mpool_tmp == *mpools){
				*mpools = (*mpools)->next;
				free(mpool_tmp);
				mpool_tmp = mpool_tmp2 = *mpools;
			} else {
				mpool_tmp2->next = mpool_tmp->next;
				free(mpool_tmp);
				mpool_tmp = mpool_tmp2->next;
			}
		} else {
			// Move pointers ahead
			mpool_tmp2 = mpool_tmp;
			mpool_tmp = mpool_tmp->next;
		}
	}
}

/*
 * static int
 * vbd_get_mpool(char **mpool_name, int32_t domid, int32_t vbdid);
 * ---------------------------------------------------------------
 * This function looks in sysfs for the vbd entry indicated by
 * domid and vbdid. It allocates and fills mpool_name with the
 * corresponding name for the mpool in use by this vbd.
 * Return values:
 *  -1 An error has occurred.
 *   0 Success.
 */
static int
vbd_get_mpool(char **mpool_name, int32_t domid, int32_t vbdid){
	// Local variables
	FILE		*mpool_fp = NULL;	// sysfs entry filepointer
	char		*mpool_fn = NULL;	// sysfs entry pathname
	char		buf[256];		// read buffer
	int		ret = 0;		// return code

	// Allocate sysfs entry pathname
	if (asprintf(&mpool_fn, "%s/%s-%d-%d/%s", MT_SYSFS_XENBK, MT_SYSFS_XENBK_PREFIX, domid, vbdid, MT_SYSFS_XENBK_PGPOOL) <= 0){
		perror("asprintf");
		fprintf(stderr, "Error allocating memory for the vbd mpool sysfs entry.\n");
		mpool_fn = NULL;
		goto err;
	}

	// Open sysfs entry
	if ((mpool_fp = fopen(mpool_fn, "r")) == NULL){
		perror("fopen");
		fprintf(stderr, "Error opening the vbd mpool sysfs entry: '%s'.\n", mpool_fn);
		goto err;
	}

	// Read the mpool entry
	memset(buf, 0, sizeof(buf));
	if (fgets(buf, sizeof(buf), mpool_fp) == NULL){
		perror("fgets");
		fprintf(stderr, "Error reading from the vbd mpool sysfs entry: '%s'.\n", mpool_fn);
		goto err;
	}

	// Remove line break
	if ((strlen(buf) > 0) && (buf[strlen(buf)-1]) == '\n')
		buf[strlen(buf)-1] = 0;

	// Allocate return string
	if (asprintf(mpool_name, "%s", buf) <= 0){
		perror("asprintf");
		fprintf(stderr, "Error allocating memory for the vbd mpool name.\n");
		*mpool_name = NULL;
		goto err;
	}

	// Skip error section
	goto out;

err:
	ret = -1;

out:
	if (mpool_fp)
		fclose(mpool_fp);
	if (mpool_fn)
		free(mpool_fn);

	return(ret);
}

/*
 * static int
 * mpool_insert_vbd(char *name, mt_mempool_t **mpools, mt_vbd_t *vbd_tmp);
 * -----------------------------------------------------------------------
 * This function locates the mpool represented by name and inserts vbd_tmp
 * into its list of vbds. If there is no corresponding mpool (with name),
 * one will be allocated.
 * Return values:
 *  -1 An error occurred.
 *   0 Success.
 */
static int
mpool_insert_vbd(char *name, mt_mempool_t **mpools, mt_vbd_t *vbd_tmp){
	// Local variables
	mt_mempool_t	*mpool_tmp;
	int		ret = 0;

	// Loop through mpool
	mpool_tmp = *mpools;
	while (mpool_tmp){
		// Locate mpool matching name
		if (!strcmp(mpool_tmp->name, name)){
			vbd_tmp->next = mpool_tmp->vbds;
			mpool_tmp->vbds = vbd_tmp;
			mpool_tmp = NULL;
			goto out;
		}
		mpool_tmp = mpool_tmp->next;
	}

	// Create mpool for this entry
	if ((mpool_tmp = (mt_mempool_t *)calloc(1, sizeof(mt_mempool_t))) == NULL){
		perror("calloc");
		fprintf(stderr, "Error allocating memory for new mpool: '%s'\n", name);
		goto err;
	}

	// Fill it accordingly
	if ((asprintf(&(mpool_tmp->name), "%s", name)) <= 0){
		perror("asprintf");
		fprintf(stderr, "Error allocating name for new mpool: '%s'\n", name);
		mpool_tmp->name = NULL;
		goto err;
	}
	mpool_tmp->vbds = vbd_tmp;
	if (mpool_update(mpool_tmp, MT_SYSFS_BBPOOL_SIZE) != 0){
		goto err;
	}

	// Insert it in the mpools list
	mpool_tmp->next = *mpools;
	*mpools = mpool_tmp;
	mpool_tmp = NULL;

	// Skip error section
	goto out;

err:
	ret = -1;

out:
	if (mpool_tmp){
		free(mpool_tmp);
	}
	return(ret);
}

/*
 * int
 * vbd_bt_update(mt_vbd_t *vbd);
 * -----------------------------
 * This function updates the blktap related statistics for a vbd.
 * Return values:
 *  -1 An error has occurred.
 *   0 Success.
 */
int
vbd_bt_update(mt_vbd_t *vbd){
	// Local variables
	char		buf[1024];	// Read buffer
	int		ret = 0;	// Return code

	uint32_t	rds;		// Read requests completed
	uint32_t	rsc;		// Read sectors completed
	uint32_t	wrs;		// Write requests completed
	uint32_t	wsc;		// Write sectors completed

	// Read the entire stat entry to mem
	memset(buf, 0, sizeof(buf));
	fseek(vbd->btstat.statfp, 0, SEEK_SET);
	fread(buf, sizeof(buf), 1, vbd->btstat.statfp);

	// Check if fp had errors (VBD detached?)
	if (ferror(vbd->btstat.statfp)){
		perror("fseek/fread");
		fprintf(stderr, "Error rewinding/reading vbd (%d,%d) sysfs/stat file pointer.\n", vbd->domid, vbd->vbdid);
		goto err;
	}

	// Process read buffer
	//                1   2  3   4  5   6  7        1     3     5     7
	if (sscanf(buf, "%u %*u %u %*u %u %*u %u %*s", &rds, &rsc, &wrs, &wsc) != 4){
		perror("sscanf");
		fprintf(stderr, "Error reading content of vbd (%d,%d) sysfs/stat entry.\n", vbd->domid, vbd->vbdid);
		goto err;
	}
	vbd->btstat.rds_1 = vbd->btstat.rds_0;
	vbd->btstat.rds_0 = rds;
	vbd->btstat.rsc_1 = vbd->btstat.rsc_0;
	vbd->btstat.rsc_0 = rsc;
	vbd->btstat.wrs_1 = vbd->btstat.wrs_0;
	vbd->btstat.wrs_0 = wrs;
	vbd->btstat.wsc_1 = vbd->btstat.wsc_0;
	vbd->btstat.wsc_0 = wsc;

	// Read the entire inflight entry to mem
	memset(buf, 0, sizeof(buf));
	fseek(vbd->btstat.inflfp, 0, SEEK_SET);
	fread(buf, sizeof(buf), 1, vbd->btstat.inflfp);

	// Check if fp had errors (VBD detached?)
	if (ferror(vbd->btstat.inflfp)){
		perror("fseek/fread");
		fprintf(stderr, "Error rewinding/reading vbd (%d,%d) sysfs/inflight file pointer.\n", vbd->domid, vbd->vbdid);
		goto err;
	}

	// Process read buffer
	if (sscanf(buf, "%u %u", &(vbd->btstat.infrd), &(vbd->btstat.infwr)) != 2){
		perror("sscanf");
		fprintf(stderr, "Error reading content of vbd (%d,%d) sysfs/inflight entry.\n", vbd->domid, vbd->vbdid);
		goto err;
	}

	// Skip error section
	goto out;

err:
	ret = -1;
out:
	return(ret);
}

/*
 * int
 * vbd_bb_update(mt_vbd_t *vbd);
 * -----------------------------
 * This function updates the bbstat entry of the vbd.
 * Return values:
 *  -1 An error has occurred.
 *   0 Success.
 */
int
vbd_bb_update(mt_vbd_t *vbd){
	// Local variables
	char	buf[1024];	// Read buffer
	int	ret = 0;	// Return code

	// Read the entire entry to mem
	memset(buf, 0, sizeof(buf));
	fseek(vbd->bbstat.ringfp, 0, SEEK_SET);
	fread(buf, sizeof(buf), 1, vbd->bbstat.ringfp);

	// Check if fp had errors (VBD detached?)
	if (ferror(vbd->bbstat.ringfp)){
		perror("fseek/fread");
		fprintf(stderr, "Error rewinding vbd (%d,%d) io_ring file pointer.\n", vbd->domid, vbd->vbdid);
		goto err;
	}

	// Process read buffer
	if (sscanf(buf, "nr_ents %u\nreq prod %u %*[^\n]\nrsp prod %u %*s", &(vbd->bbstat.iorsz), &(vbd->bbstat.ioreq), &(vbd->bbstat.iorsp)) != 3){
		perror("sscanf");
		fprintf(stderr, "Error reading content of vbd (%d,%d) io_ring sysfs entry.\n", vbd->domid, vbd->vbdid);
		goto err;
	}

	// Skip error section
	goto out;

err:
	ret = -1;
out:
	return(ret);
}

/*
 * static int
 * vbd_populate(mt_vbd_t **vbd, int32_t domid, int32_t vbdid);
 * ----------------------------------------------------------
 * This function allocates and populates vbd based on domid
 * and vbdid.
 * Return values:
 *  -1 An error occurred.
 *   0 Success.
 */
static int
vbd_populate(mt_vbd_t **vbd, int32_t domid, int32_t vbdid){
	// Local variables
#ifdef	MT_USE_TAPDISK
	int	tdpid;			// tapdisk pid
#endif	/* MT_USE_TAPDISK */
	FILE	*pdevfp = NULL;		// File pointer for physical_device
	char	*ptr = NULL;		// temporary char pointer
	char	*ptrb = NULL;		// temporary char pointer
	int	i, j, k;		// temporary integers
	int	ret = 0;		// return code

	// Allocate the vbd entry
	if ((*vbd = (mt_vbd_t *)calloc(1, sizeof(mt_vbd_t))) == NULL){
		perror("calloc");
		fprintf(stderr, "Error allocating memory for mt_vbd_t entry.\n");
		goto err;
	}

	// Fill basic data
	(*vbd)->domid = domid;
	(*vbd)->vbdid = vbdid;

	// Open the blkback io_ring sysfs entry
	if (asprintf(&ptr, "%s/%s-%d-%d/%s", MT_SYSFS_XENBK, MT_SYSFS_XENBK_PREFIX, domid, vbdid, MT_SYSFS_XENBK_IORING) <= 0){
		perror("asprintf");
		fprintf(stderr, "Error allocating memory for vbd bbrfp entry.\n");
		ptr = NULL;
		goto err;
	}
	if (((*vbd)->bbstat.ringfp = fopen(ptr, "r")) == NULL){
		perror("fopen");
		fprintf(stderr, "Error opening vbd bbstat.ringfp: '%s'.\n", ptr);
		goto err;
	}
	free(ptr);
	ptr = NULL;

	// Locate the corresponding blktap minor number
	if (asprintf(&ptr, "%s/%s-%d-%d/%s", MT_SYSFS_XENBK, MT_SYSFS_XENBK_PREFIX, domid, vbdid, MT_SYSFS_XENBK_PHYSDEV) <= 0){
		perror("asprintf");
		fprintf(stderr, "Error allocating memory for vbd physical_device entry.\n");
		ptr = NULL;
		goto err;
	}
	if ((pdevfp = fopen(ptr, "r")) == NULL){
		perror("fopen");
		fprintf(stderr, "Error opening vbd physical_device sysfs entry.\n");
		goto err;
	}
	free(ptr);
	ptr = NULL;
	if (fscanf(pdevfp, "%*x:%x", &((*vbd)->btstat.minor)) != 1){
		// VBD does not have associated physical device, ignore it
		goto err;
	}
	fclose(pdevfp);
	pdevfp = NULL;

#ifdef	MT_USE_TAPDISK
	// TODO: If we ever use tapdisk stats again, then make the below into hash defined strings.
	// TODO: Also, tdmin should become 'minor'.
	// TODO: Also, stop borrowing 'btstat.statfp' and use a dedicated file pointer.

	// Locate the corresponding tapdisk pid
	if (asprintf(&ptr, "/sys/class/blktap2/blktap%d/task", (*vbd)->tdstat.tdmin) <= 0){
		perror("asprintf");
		fprintf(stderr, "Error allocating memory for blktap2 task entry.\n");
		ptr = NULL;
		goto err;
	}
	if (((*vbd)->btstat.statfp = fopen(ptr, "r")) == NULL){
		perror("fopen");
		fprintf(stderr, "Error opening blktap2 task entry (vbd %d,%d): '%s'.\n", (*vbd)->domid, (*vbd)->vbdid, ptr);
		goto err;
	}
	fscanf((*vbd)->btstat.statfp, "%d", &tdpid);
	fclose((*vbd)->btstat.statfp);
	(*vbd)->btstat.statfp = NULL;

	// Set tapdisk control socket file name
	if (asprintf(&((*vbd)->tdstat.tdcfn), "/var/run/blktap-control/ctl%d", tdpid) <= 0){
		perror("asprintf");
		fprintf(stderr, "Error allocating memory for tapdisk control socket name.\n");
		(*vbd)->tdstat.tdcfn = NULL;
		goto err;
	}
#endif	/* MT_USE_TAPDISK */

	// Calculate how many chars in the corresponding 'td' name
	i = (*vbd)->btstat.minor;
	j = 1;
	k = 26;
	while (i/k){ j++; i-=k; k*=26; } // 'j' has td name length
	if ((ptr = (char *)malloc(strlen(MT_SYSFS_TD_PREFIX)+j+1+strlen(MT_SYSFS_TD_INFLIGHT)+1)) == NULL){
	//                                                    ^^^ '/'                        ^^^ '\0'
		perror("malloc");
		fprintf(stderr, "Error allocating memory for blktap2 stat entry.\n");
		goto err;
	}
	sprintf(ptr, MT_SYSFS_TD_PREFIX);
	ptrb = ptr+strlen(MT_SYSFS_TD_PREFIX)+j-1;

	// Convert 'minor' to proper sysfs entry name (mimic udev's rule)
	i = (*vbd)->btstat.minor;
	k = j;
	while (j-->0){
		*ptrb-- = (i%26)+'a';
		i-=26; i/=26;
	}
	ptrb = ptr+strlen(MT_SYSFS_TD_PREFIX)+k;
	sprintf(ptrb, "/%s", MT_SYSFS_TD_STAT);
	// TODO: This works because len("stat") < len("inflight"). Ideally, we want to allocate the base
	// TODO: name (/sys/block/td<x>/) and then have separate variables for /stat, /inflight, etc.

	// Open the device's sysfs/stat entry
	if (((*vbd)->btstat.statfp = fopen(ptr, "r")) == NULL){
		perror("fopen");
		fprintf(stderr, "Error opening the blktap2 sysfs/stat entry: '%s'.\n", ptr);
		goto err;
	}

	// Open the device's sysfs/inflight entry
	sprintf(ptrb, "/%s", MT_SYSFS_TD_INFLIGHT);
	if (((*vbd)->btstat.inflfp = fopen(ptr, "r")) == NULL){
		perror("fopen");
		fprintf(stderr, "Error opening the blktap2 sysfs/inflight entry: '%s'.\n", ptr);
		goto err;
	}

	// Skip error section
	goto out;

err:
	ret = -1;

	if (pdevfp){
		fclose(pdevfp);
		pdevfp = NULL;
	}
	if (*vbd){
		if ((*vbd)->bbstat.ringfp)
			fclose((*vbd)->bbstat.ringfp);
		if ((*vbd)->btstat.statfp)
			fclose((*vbd)->btstat.statfp);
#ifdef	MT_USE_TAPDISK
		if ((*vbd)->tdstat.tdcfn)
			free((*vbd)->tdstat.tdcfn);
#endif	/* MT_USE_TAPDISK */
		free(*vbd);
	}

out:
	if (ptr)
		free(ptr);

	return(ret);
}


/*
 * int vbds_alloc(mt_mempool_t **mpools, int32_t domid, int32_t vbdid);
 * --------------------------------------------------------------------
 * Allocate the mt_vbd_t related entries and associate them with their
 * corresponding mpool.
 * If domid is non-negative, do so only for the vbds belonging to that domain.
 * If vbdid is non-negative, do so only for the that vbd (which must exist in
 * the corresponding domid).
 * Return values:
 *  -1 An error has occurred.
 *   0 Success.
 */
int
vbds_alloc(mt_mempool_t **mpools, int32_t domid, int32_t vbdid){
	// Local variables
	const char	vbd_fn[] = MT_SYSFS_XENBK;	// xen-backend sysfs prefix
	DIR		*vbd_fp = NULL;			// sysfs dir pointer
	struct dirent	*vbd_dir = NULL;		// sysfs dir entry
	mt_vbd_t	*vbd_tmp = NULL;		// temporary mt_vbd_t entry
	char		mask[64];			// sysfs vbd entry name mask
	char		*ptr;				// temporary auxiliary pointer
	int32_t		scanned_domid, scanned_vbdid;	// scanned dom and vbd ids
	int		ret = 0;			// return code

	// Open sysfs dir
	if ((vbd_fp = opendir(vbd_fn)) == NULL){
		perror("opendir");
		fprintf(stderr, "Error opening sysfs vbd dir: '%s'.\n", vbd_fn);
		goto err;
	}

	// Set the name mask accordingly
	snprintf(mask, sizeof(mask), "%s-", MT_SYSFS_XENBK_PREFIX);
	ptr = mask+strlen(mask);
	if (domid >= 0){
		snprintf(ptr, sizeof(mask)-strlen(mask), "%d-", domid);
		ptr = mask+strlen(mask);
		if (vbdid >= 0){
			snprintf(ptr, sizeof(mask)-strlen(mask), "%d", vbdid);
		}
	}

	// Read the sysfs directory
	while ((vbd_dir = readdir(vbd_fp)) != NULL){
		// Filter with mask
		if (vbdid >= 0){
			if (strcmp(vbd_dir->d_name, mask)){
				continue;
			}
		}else{
			if (strncmp(vbd_dir->d_name, mask, strlen(mask))){
				continue;
			}
		}

		// Parse domid and vbdid
		if (sscanf(vbd_dir->d_name, "vbd-%d-%d", &scanned_domid, &scanned_vbdid) != 2){
			continue;
		}

		// Allocate VBD entry
		if (vbd_populate(&vbd_tmp, scanned_domid, scanned_vbdid) != 0){
			// Skip this vbd and attempt to carry on
			continue;
		}

		// Fill VBD entry with initial values
#ifdef	MT_USE_TAPDISK
		if (vbd_bb_update(vbd_tmp) ||
		    vbd_bt_update(vbd_tmp) ||
		    vbd_td_update(vbd_tmp)) {
			goto err;
		}
#else
		if (vbd_bb_update(vbd_tmp) ||
		    vbd_bt_update(vbd_tmp)) {
			goto err;
		}
#endif	/* MT_USE_TAPDSIK */

		// Fetch memory pool that this vbd uses
		ptr = NULL;
		if (vbd_get_mpool(&ptr, scanned_domid, scanned_vbdid) != 0){
			goto err;
		}

		// Insert this vbd into the corresponding mpool entry
		if (mpool_insert_vbd(ptr, mpools, vbd_tmp) != 0){
			goto err;
		}
		free(ptr);
		ptr = NULL;
	}

	// Skip error section
	goto out;

err:
	ret = -1;
	// TODO: Ideally, have a function to clean mt_vbd_t up
	if (vbd_tmp){
		if (vbd_tmp->bbstat.ringfp)
			fclose(vbd_tmp->bbstat.ringfp);
		if (vbd_tmp->btstat.statfp)
			fclose(vbd_tmp->btstat.statfp);
		free(vbd_tmp);
	}

out:
	if (vbd_fp)
		closedir(vbd_fp);
	return(ret);
}
