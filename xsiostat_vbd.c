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
#include "xsiostat.h"

// Global variables
extern int PAGE_SIZE;

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

    // Allocate new VBD entry
    if (!(*vbd = calloc(1, sizeof(xsis_vbd_t)))){
        perror("calloc");
        goto err;
    }
    (*vbd)->domid = domid;
    (*vbd)->vbdid = vbdid;
    (*vbd)->shmfd = -1;

    // Open stats fd
    if (asprintf(&ptr, XSIS_VBD3_PATHFMT, domid, vbdid) < 0){
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
    vbd->tdstat.rop_0 = ((struct blkback_stats *)(vbd->shmmap))->st_rd_req;
    vbd->tdstat.rsc_1 = vbd->tdstat.rsc_0;
    vbd->tdstat.rsc_0 = ((struct blkback_stats *)(vbd->shmmap))->st_rd_sect;
    vbd->tdstat.wop_1 = vbd->tdstat.wop_0;
    vbd->tdstat.wop_0 = ((struct blkback_stats *)(vbd->shmmap))->st_wr_req;
    vbd->tdstat.wsc_1 = vbd->tdstat.wsc_0;
    vbd->tdstat.wsc_0 = ((struct blkback_stats *)(vbd->shmmap))->st_wr_sect;
    vbd->tdstat.wtu_1 = vbd->tdstat.wtu_0;
    vbd->tdstat.wtu_0 = ((struct blkback_stats *)(vbd->shmmap))->st_wr_sum_usecs;
    vbd->tdstat.rtu_1 = vbd->tdstat.rtu_0;
    vbd->tdstat.rtu_0 = ((struct blkback_stats *)(vbd->shmmap))->st_rd_sum_usecs;
    vbd->tdstat.infrd = ((struct blkback_stats *)(vbd->shmmap))->st_rd_req -
                        ((struct blkback_stats *)(vbd->shmmap))->st_rd_cnt;
    vbd->tdstat.infwr = ((struct blkback_stats *)(vbd->shmmap))->st_wr_req -
                        ((struct blkback_stats *)(vbd->shmmap))->st_wr_cnt;
    vbd->tdstat.low_mem_mode = ((struct blkback_stats *)(vbd->shmmap))->flags & BT3_LOW_MEMORY_MODE;

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
