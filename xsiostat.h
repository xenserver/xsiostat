/*
 * -----------------------------
 *  XenServer Storage I/O Stats
 * -----------------------------
 *  xsiostat.h
 * ------------
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

// Required headers
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/queue.h>

#include <blktap/tapdisk-metrics-stats.h>
typedef struct stats tapdisk_stats;

// Global definitions
#define XSIS_PROGNAME           "Storage I/O Stats"
#define XSIS_PROGNAME_LEN       strlen(XSIS_PROGNAME)

#define XSIS_VBD3_DIR           "/dev/shm/"
#define XSIS_VBD3_BASEFMT       "vbd3-%u-%u" // domid, vbdid

#define XSIS_TD3_DIR            "/dev/shm/"
#define XSIS_TD3_BASEFMT        "td3-%u" // tapdisk pid
#define XSIS_TD3_PATHFMT        XSIS_TD3_DIR XSIS_TD3_BASEFMT "/vbd-%u-%u" // domid, vbdid

#define	XSIS_INTERVAL           1000    // Default report interval (ms)
#define	XSIS_SECTOR_SZ          512     // Bytes per sector

// Tapdisk stats (from shm page)
typedef struct _xsis_tdstat_t {
    uint64_t            rop_0;          // read requests completed now
    uint64_t            rop_1;          // read requests completed last time
    uint64_t            rsc_0;          // read sectors completed now
    uint64_t            rsc_1;          // read sectors completed last time
    uint64_t            wop_0;          // write requests completed now
    uint64_t            wop_1;          // write requests completed last time
    uint64_t            wsc_0;          // write sectors completed now
    uint64_t            wsc_1;          // write sectors completed last time
    uint64_t            rtu_0;          // read ticks in usec now
    uint64_t            rtu_1;          // read ticks in usec last time
    uint64_t            wtu_0;          // write ticks in usec now
    uint64_t            wtu_1;          // write ticks in usec last time
    uint32_t            infrd;          // read requests inflight
    uint32_t            infwr;          // write requests inflight
    bool                low_mem_mode;   // tapdisk low memory mode
} xsis_tdstat_t;

// VBD general entry
typedef struct _xsis_vbd_t {
    uint32_t            domid;          // domain id owning this vbd
    uint32_t            vbdid;          // vbd id
    uint32_t            tdpid;          // tapdisk pid
    int32_t             shmfd;          // shared memory stats fd
    void                *shmmap;        // shared memory stats mapping
    xsis_tdstat_t       tdstat;         // tapdisk stat information
    LIST_ENTRY(_xsis_vbd_t) vbds;       // list
} xsis_vbd_t;

// Filter general entry
typedef struct _xsis_flt_t {
    uint32_t            filter;         // filter id
    LIST_ENTRY(_xsis_flt_t) flts;       // list
} xsis_flt_t;

// Lists
typedef LIST_HEAD(xsis_vbds, _xsis_vbd_t) xsis_vbds_t;
typedef LIST_HEAD(xsis_flts, _xsis_flt_t) xsis_flts_t;

// xsiostat_vbd interface
int
vbd_update(xsis_vbd_t *);

int
vbds_alloc(xsis_vbds_t *, xsis_flts_t *, xsis_flts_t *);

void
vbd_delete(xsis_vbd_t *, xsis_vbds_t *);

void
vbds_free(xsis_vbds_t *);

// xsiostat_flt interface
int
flt_isset(xsis_flts_t *, uint32_t);

int
flt_add(xsis_flts_t *, uint32_t);

void
flts_free(xsis_flts_t *);
