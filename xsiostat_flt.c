/*
 * -----------------------------
 *  XenServer Storage I/O Stats
 * -----------------------------
 *  xsiostat_flt.c
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
#include <stdlib.h>
#include <sys/queue.h>
#include "xsiostat.h"

int
flt_isset(xsis_flts_t *flts, uint32_t filter){
    // Local variables
    xsis_flt_t          *flt;           // Temporary FLT pointer
    int                 isset = 0;      // Return code

    // Check if filter is already in list
    LIST_FOREACH(flt, flts, flts)
        if (flt->filter == filter){
            isset = 1;
            break;
        }

    // Return
    return(isset);
}

int
flt_add(xsis_flts_t *flts, uint32_t filter){
    // Local variables
    xsis_flt_t          *flt;           // Temporary FLT pointer
    int                 err = 0;        // Return code

    // Check if filter is already in list
    if (flt_isset(flts, filter))
            goto err;

    // Allocate new filter and insert into list
    if (!(flt = (xsis_flt_t *)calloc(1, sizeof(xsis_flt_t)))){
        perror("calloc");
        goto err;
    }
    flt->filter = filter;
    LIST_INSERT_HEAD(flts, flt, flts);

out:
    // Return
    return(err);

err:
    err = 1;
    goto out;
}

void
flts_free(xsis_flts_t *flts){
    // Local variables
    xsis_flt_t          *flt;           // Temporary FLT pointer

    // Loop through FLTs, freeing resources
    LIST_FOREACH(flt, flts, flts){
        LIST_REMOVE(flt, flts);
        free(flt);
    }

    // Return
    return;
}
