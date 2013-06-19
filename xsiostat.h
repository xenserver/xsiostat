/*
 * -----------------------------
 *  XenServer Storage I/O Stats 
 * -----------------------------
 *  xsiostat.h
 * ------------
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

// Global definitions
#define MT_PROGNAME		"XenServer Storage I/O Stats"
#define MT_PROGNAME_LEN		strlen(MT_PROGNAME)

#define	MT_INTERVAL		1000	// Default interval between outputs (ms)
#define	MT_SECTOR_SZ		512	// Bytes per sector
#define	MT_SYSFS_XENBK		"/sys/devices/xen-backend"
#define	MT_SYSFS_XENBK_PREFIX	"vbd"
#define	MT_SYSFS_XENBK_IORING	"io_ring"
#define	MT_SYSFS_XENBK_PHYSDEV	"physical_device"
#define	MT_SYSFS_XENBK_PGPOOL	"page_pool"

#define	MT_SYSFS_TD_PREFIX	"/sys/block/td"
#define	MT_SYSFS_TD_INFLIGHT	"inflight"
#define	MT_SYSFS_TD_STAT	"stat"

#define	MT_SYSFS_BBPOOL		"/sys/kernel/blkback-pools"

#define	MT_SYSFS_BBPOOL_SIZE	"size"
#define	MT_SYSFS_BBPOOL_FREE	"free"

// Include tapdisk statistics
//#define	MT_USE_TAPDISK		1

#ifdef	__GNUC__
#define likely(x)		__builtin_expect(!!(x),1)
#define unlikely(x)		__builtin_expect(!!(x),0)
#else
#define likely(x)		(x)
#define unlikely(x)		(x)
#endif	/* __GNUC__ */

// blkback io_ring data
typedef struct _mt_bbstat_t {
	FILE *			ringfp;	// blkback sysfs/io_ring file pointer
	uint32_t		iorsz;	// blkback io_ring size
	uint32_t		ioreq;	// blkback io_ring req prod
	uint32_t		iorsp;	// blkback io_ring rsp prod
} mt_bbstat_t;

// blktap sysfs data
typedef struct _mt_btstat_t {
	FILE *			statfp;	// blktap sysfs/stat file pointer
	uint32_t		minor;	// blktap minor number
	uint32_t		rds_0;	// read requests completed now
	uint32_t		rds_1;	// read requests completed last time
	uint32_t		rsc_0;	// read sectors completed now
	uint32_t		rsc_1;	// read sectors completed last time
	uint32_t		wrs_0;	// write requests completed now
	uint32_t		wrs_1;	// write requests completed last time
	uint32_t		wsc_0;	// write sectors completed now
	uint32_t		wsc_1;	// write sectors completed last time
	FILE *			inflfp;	// blktap sysfs/inflight file pointer
	uint32_t		infrd;	// read requests inflight
	uint32_t		infwr;	// write requests inflight
} mt_btstat_t;

#ifdef	MT_USE_TAPDISK
// tapdisk data (from ctl socket)
typedef struct _mt_tdstat_t {
	char *			tdcfn;	// tapdisk ctl socket file name
	uint64_t		rds_0;	// read requests completed now
	uint64_t		rds_1;	// read requests completed last time
	uint64_t		rsc_0;	// read sectors completed now
	uint64_t		rsc_1;	// read sectors completed last time
	uint64_t		wrs_0;	// write requests completed now
	uint64_t		wrs_1;	// write requests completed last time
	uint64_t		wsc_0;	// write sectors completed now
	uint64_t		wsc_1;	// write sectors completed last time
	uint32_t		infrd;	// read requests inflight
	uint32_t		infwr;	// write requests inflight
} mt_tdstat_t;
#endif	/* MT_USE_TAPDISK */

// vbd general entry
typedef struct _mt_vbd_t {
	int32_t			domid;	// domain id owning this vbd
	int32_t			vbdid;	// vbd id
	mt_bbstat_t		bbstat;	// blkback stat information
	mt_btstat_t		btstat;	// blktap stat information
#ifdef	MT_USE_TAPDISK
	mt_tdstat_t		tdstat;	// tapdisk stat information
#endif	/* MT_USE_TAPDISK */
	struct _mt_vbd_t	*next;	// next entry
} mt_vbd_t;

// blkback memory pool general entry
typedef struct _mt_mempool_t {
	char			*name;	// blkback memory pool name
	uint32_t		size;	// number of entries in total
	uint32_t		free;	// number of entries free
	mt_vbd_t		*vbds;	// list of vbds
	struct _mt_mempool_t	*next;	// next entry
} mt_mempool_t;


// xsiostat_sysfs interface
// (see source file for documentation)

int
mpool_update(mt_mempool_t *mpool, char mpool_field[]);

int
vbd_bb_update(mt_vbd_t *vbd);

int
vbd_bt_update(mt_vbd_t *vbd);

#ifdef	MT_USE_TAPDISK
int
vbd_td_update(mt_vbd_t *vbd);
#endif	/* MT_USE_TAPDISK */

void
mpools_clean(mt_mempool_t **);

int
vbds_alloc(mt_mempool_t **, int32_t, int32_t);
