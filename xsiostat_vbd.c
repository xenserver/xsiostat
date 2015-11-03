/*
 * -----------------------------
 *  XenServer Storage I/O Stats
 * -----------------------------
 *  xsiostat_vbd.c
 * ----------------
 *
 * Copyright (C) 2013, 2014 Citrix Systems Inc.
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
 */

// Header files
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/queue.h>
#include <xenstore.h>
#include "xsiostat.h"

// Global variables
extern int PAGE_SIZE;

static uint32_t
vbd_read_tapdisk_pid(uint32_t domid, uint32_t vbdid)
{
    // XenStore tapdisk pid path
    static const char* PATH_FMT = "/local/domain/0/backend/vbd3/%u/%u/kthread-pid";

    // Local variables
    char *path;                 // Path storage
    unsigned int len;           // Temp variable
    void *value;                // Value returned by xs_read
    uint32_t retvalue = 0;      // Value converted to integer

    // Open Xenstore connection
    struct xs_handle *handle = xs_open(XS_OPEN_READONLY);

    if (!handle) {
        perror("xs_open");
        goto err;
    }

    // Format tapdisk pid path
    if (asprintf(&path, PATH_FMT, domid, vbdid) < 0) {
        perror("asprintf");
        goto asperr;
    }

    // Read value
    value = xs_read(handle, XBT_NULL, path, &len);

    // We don't print error here, as we would always report invalid cdrom entry,
    // for which there is no tapdisk pid
    if (!value) {
        goto readerr;
    }

    // Convert to int
    retvalue = atoi((const char *)value);

    free(value);
readerr:
    free(path);
asperr:
    xs_close(handle);
err:
    return retvalue;
}

static void
vbd_free(xsis_vbd_t *vbd){
    // Release VBD resources
    if (vbd){
        if (vbd->shmmap)
            (void)munmap(vbd->shmmap, PAGE_SIZE);
        if (vbd->shmfd >= 0)
            (void)close(vbd->shmfd);
        free(vbd);
    }
}

static int
vbd_open(xsis_vbd_t **vbd, uint32_t domid, uint32_t vbdid){
    // Local variables
    char                *ptr;           // Temporary char pointer
    int                 err = 0;        // Return code
    uint32_t            tdpid = 0;      // Tapdisk PID

    // Allocate new VBD entry
    if (!(*vbd = calloc(1, sizeof(xsis_vbd_t)))){
        perror("calloc");
        goto err;
    }

    // We don't print error here, as we would always report invalid cdrom entry,
    // for which there is no tapdisk pid
    tdpid = vbd_read_tapdisk_pid(domid, vbdid);
    if (!tdpid) {
        goto err;
    }

    (*vbd)->domid = domid;
    (*vbd)->vbdid = vbdid;
    (*vbd)->shmfd = -1;

    // Open stats fd
    if (asprintf(&ptr, XSIS_TD3_PATHFMT, tdpid, domid, vbdid) < 0){
        perror("asprintf");
        goto err;
    }
    if (((*vbd)->shmfd = open(ptr, O_RDONLY)) < 0){
        perror("open");
        goto err;
    }

    // mmap() stats entry
    if (!((*vbd)->shmmap = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED,
                                (*vbd)->shmfd, 0))){
        perror("mmap");
        goto err;
    }

out:
    // Return
    return(err);

err:
    vbd_free(*vbd);
    err = 1;
    goto out;
}

int
vbd_update(xsis_vbd_t *vbd){
    // Local variables
    struct stat         vbdst;          // Temporary stat struct
    int                 err = 0;        // Return code

    // Check if VBD remains valid
    if (fstat(vbd->shmfd, &vbdst) || !(vbdst.st_nlink))
        goto err;

    // Update VBD entries based on shm mapping
    vbd->tdstat.rop_1 = vbd->tdstat.rop_0;
    vbd->tdstat.rop_0 = ((struct tapdisk_stats *)(vbd->shmmap))->read_reqs_submitted;
    vbd->tdstat.rsc_1 = vbd->tdstat.rsc_0;
    vbd->tdstat.rsc_0 = ((struct tapdisk_stats *)(vbd->shmmap))->read_sectors;
    vbd->tdstat.wop_1 = vbd->tdstat.wop_0;
    vbd->tdstat.wop_0 = ((struct tapdisk_stats *)(vbd->shmmap))->write_reqs_submitted;
    vbd->tdstat.wsc_1 = vbd->tdstat.wsc_0;
    vbd->tdstat.wsc_0 = ((struct tapdisk_stats *)(vbd->shmmap))->write_sectors;
    vbd->tdstat.wtu_1 = vbd->tdstat.wtu_0;
    vbd->tdstat.wtu_0 = ((struct tapdisk_stats *)(vbd->shmmap))->write_total_ticks;
    vbd->tdstat.rtu_1 = vbd->tdstat.rtu_0;
    vbd->tdstat.rtu_0 = ((struct tapdisk_stats *)(vbd->shmmap))->read_total_ticks;
    vbd->tdstat.infrd = ((struct tapdisk_stats *)(vbd->shmmap))->read_reqs_submitted -
                        ((struct tapdisk_stats *)(vbd->shmmap))->read_reqs_completed;
    vbd->tdstat.infwr = ((struct tapdisk_stats *)(vbd->shmmap))->write_reqs_submitted -
                        ((struct tapdisk_stats *)(vbd->shmmap))->write_reqs_completed;

    //Zero for now as we don't have this information in tapdisk stats file
    vbd->tdstat.low_mem_mode = 0;

out:
    // Return
    return(err);

err:
    err = 1;
    goto out;
}

int
vbds_alloc(xsis_vbds_t *vbds, xsis_flts_t *domids, xsis_flts_t *vbdids){
    // Local variables
    DIR                 *dp = NULL;     // dir pointer
    struct dirent       *dirp;          // dirent pointer
    xsis_vbd_t          *vbd;           // Temporary VBD pointer
    uint32_t            domid;          // Temporary DOM ID
    uint32_t            vbdid;          // Temporary VBD ID
    int                 err = 0;        // Return code

    // Open VBD3 base dir
    if (!(dp = opendir(XSIS_VBD3_DIR))){
        perror("opendir");
        goto err;
    }

    // Scan for valid VBD entries
    while ((dirp = readdir(dp))){
        // Skip irrelevant entries and fetch DOM/VBD ids
        if (sscanf(dirp->d_name, XSIS_VBD3_BASEFMT, &domid, &vbdid) != 2)
            continue;

        // Filter domids and vbdids
        if (!LIST_EMPTY(domids))
            if (!flt_isset(domids, domid))
                continue;
        if (!LIST_EMPTY(vbdids))
            if (!flt_isset(vbdids, vbdid))
                continue;

        // Do not add repeated entries
        LIST_FOREACH(vbd, vbds, vbds)
            if ((vbd->domid == domid) && (vbd->vbdid == vbdid))
                break;
        if (vbd)
            continue;

        // Allocate VBD entry
        if (vbd_open(&vbd, domid, vbdid))
            continue;

        // Insert new VBD in list
        LIST_INSERT_HEAD(vbds, vbd, vbds);
    }

out:
    // Close VBD3 base dir
    if (dp)
        closedir(dp);

    // Return
    return(err);

err:
    err = 1;
    goto out;
}

void
vbd_delete(xsis_vbd_t *vbd, xsis_vbds_t *vbds){
    LIST_REMOVE(vbd, vbds);
    vbd_free(vbd);
}

void
vbds_free(xsis_vbds_t *vbds){
    // Local variables
    xsis_vbd_t          *vbd;           // Temporary VBD pointer

    // Loop through VBDs, freeing resources
    LIST_FOREACH(vbd, vbds, vbds){
        vbd_delete(vbd, vbds);
    }

    // Return
    return;
}
